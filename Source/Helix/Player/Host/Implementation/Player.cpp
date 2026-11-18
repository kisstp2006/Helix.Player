#include <Helix/Player/Host/Player.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Application/Application.h>
#include <Helix/Engine/GameObject/GameObject.h>
#include <Helix/Engine/GameObject/Lifecycle.h>
#include <Helix/Engine/Input/Cursor.h>
#include <Helix/Engine/Input/SystemClipboard.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Rendering/ScreenCapture.h>
#include <Helix/Engine/Scene/SceneManager.h>
#include <Helix/Engine/Screen/Display.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Engine/Shader/Shader.h>
#include <Helix/Engine/Time/Time.h>
#include <Helix/Engine/Transform/Transform.h>
#include <Helix/Jobs/Jobs.h>
#include <Helix/Platform/System/Process.h>
#include <Helix/Platform/Time/Time.h>
#if HX_PLATFORM_ANDROID
#include <Helix/Platform/Window/Android.h>
#endif

namespace helix::player
{
    using namespace helix::engine;

    namespace
    {
        LogChannel PlayerLogChannel{"Player"};

        constexpr i32 kDefaultWidth = 1280, kDefaultHeight = 720;
        // Batch mode draws at this size unless the command line gives one.
        constexpr i32 kBatchWidth = 640, kBatchHeight = 480;

        rhi::Backend BackendFor(const PlayerOptions& o)
        {
#if HX_PLATFORM_WEB
            (void)o;
            return rhi::Backend::WebGL2;
#elif HX_PLATFORM_ANDROID
            (void)o;
            return rhi::Backend::OpenGLES;
#elif HX_PLATFORM_WINDOWS
            return o.graphicsApi == GraphicsApi::OpenGL ? rhi::Backend::OpenGL : rhi::Backend::D3D11;
#else
            if (o.graphicsApi == GraphicsApi::D3D11)
                HX_LOG_WARNING(PlayerLogChannel, "-force-d3d11: Direct3D 11 is a Windows backend; OpenGL is used.");
            return rhi::Backend::OpenGL;
#endif
        }

        const char* BackendName(rhi::Backend backend)
        {
            switch (backend)
            {
            case rhi::Backend::D3D11: return "Direct3D";
            case rhi::Backend::OpenGLES: return "OpenGL ES";
            case rhi::Backend::WebGL2: return "WebGL 2 (OpenGL ES)";
            default: return "OpenGL";
            }
        }

        platform::WindowMode WindowModeFor(FullScreenMode mode)
        {
            switch (mode)
            {
            case FullScreenMode::ExclusiveFullScreen: return platform::WindowMode::ExclusiveFullScreen;
            case FullScreenMode::FullScreenWindow: return platform::WindowMode::FullScreen;
            case FullScreenMode::MaximizedWindow: return platform::WindowMode::Maximized;
            default: return platform::WindowMode::Windowed;
            }
        }

        FullScreenMode FullScreenModeFor(platform::WindowMode mode)
        {
            switch (mode)
            {
            case platform::WindowMode::ExclusiveFullScreen: return FullScreenMode::ExclusiveFullScreen;
            case platform::WindowMode::FullScreen: return FullScreenMode::FullScreenWindow;
            case platform::WindowMode::Maximized: return FullScreenMode::MaximizedWindow;
            default: return FullScreenMode::Windowed;
            }
        }

        bool SameHandle(const platform::NativeWindowHandle& a, const platform::NativeWindowHandle& b) { return a.window == b.window && a.display == b.display; }

        // Every object of the run, by id (destroying a root frees its children, so pointers taken up front dangle).
        void DestroyAllObjects()
        {
            Array<Object*> all;
            Array<EntityId> ids;
            FindObjectsOfTypeAll(ClassOf<GameObject>(), all);
            for (Object* o : all)
                ids.Push(o->GetEntityId());
            for (EntityId id : ids)
                if (GameObject* g = EntityIdToObject<GameObject>(id))
                    if (!g->transform() || !g->transform()->parent())
                        DestroyImmediate(g, true);
            FindObjectsOfTypeAll(ClassOf<Object>(), all);
            ids.Clear();
            for (Object* o : all)
                ids.Push(o->GetEntityId());
            for (EntityId id : ids)
                if (Object* o = EntityIdToObject(id))
                    DestroyImmediate(o, true);
            while (PendingDestroyCount())
                ProcessDestroyQueue(), SetLifecycleTime(LifecycleTime() + 1e9);
        }
    }

    bool Player::Start(const PlayerOptions& options, engine::DataFileReader* reader)
    {
        Shutdown();
        options_ = options;
        started_ = true;
        quit_ = false;
        exitCode_ = 0;
        frames_ = 0;
        if (options_.waitForNativeDebugger)
            platform::WaitForNativeDebugger();
        Application::SetBatchMode(options_.batchMode);

        // The data folder first: its settings name the product, and with it where the log goes.
        String root = options_.dataFolder.Empty() ? DefaultDataFolder() : options_.dataFolder;
        Array<String> errors;
        data_.Open(root, reader, errors);
        Application::SetDataPath(root);
        if (!options_.noLog)
        {
#if HX_PLATFORM_ANDROID || HX_PLATFORM_WEB
            // The system's log (logcat, the browser console) unless a file is asked for.
            String path = options_.logFile;
#else
            String path = options_.logFileGiven && !options_.logFile.Empty() ? options_.logFile : PlayerLog::DefaultPath(Application::persistentDataPath());
#endif
            if (!log_.Open(path, options_.timestamps))
                HX_LOG_WARNING(PlayerLogChannel, "The log file '{}' could not be opened.", path);
        }
        else
            SetConsoleLogging(false);
        HX_LOG_INFO(PlayerLogChannel, "{} {} by {} on Helix {}, data folder '{}'.", Application::productName(), Application::version(),
                    Application::companyName(), Application::helixVersion(), root);
        for (const String& e : errors)
            HX_LOG_WARNING(PlayerLogChannel, "{}", e);

        if (options_.singleInstance)
        {
            if (!instance_.Acquire(Format("Player.{}.{}", Application::companyName(), Application::productName())))
            {
                HX_LOG_INFO(PlayerLogChannel, "-single-instance: another instance runs.");
                quit_ = true;
                return false;
            }
        }
        jobs_ = !jobs::IsInitialized() && jobs::Initialize();

        if (!options_.batchMode)
        {
            if (!platform::Window::IsSupported())
            {
                HX_LOG_ERROR(PlayerLogChannel, "No display to open a window on (-batchmode runs without one).");
                return false;
            }
            platform::WindowDesc desc;
            desc.title = Application::productName();
            desc.width = options_.screenWidth > 0 ? options_.screenWidth : kDefaultWidth;
            desc.height = options_.screenHeight > 0 ? options_.screenHeight : kDefaultHeight;
            desc.decorated = !options_.popupWindow;
            desc.customFrame = options_.customFrame;
            desc.display = options_.monitor > 0 ? u32(options_.monitor - 1) : 0;
            desc.parent = reinterpret_cast<void*>(uptr(options_.parentWindow));
            if (options_.screenFullscreen == 1)
                desc.mode = options_.windowMode == WindowModeArgument::Exclusive ? platform::WindowMode::ExclusiveFullScreen : platform::WindowMode::FullScreen;
            if (desc.mode == platform::WindowMode::ExclusiveFullScreen)
                desc.exclusiveMode = {desc.width, desc.height, 0, 1};
            window_ = platform::Window::Create(desc);
            if (!window_)
            {
                HX_LOG_ERROR(PlayerLogChannel, "The window could not be created.");
                return false;
            }
            // The other displays open their windows through the player (Display.Activate, the tools' windows).
            Displays::SetHost(&PlaceDisplay, this);
            // The scripts' clipboard (GuiUtility.systemCopyBuffer, the text fields) is the window's.
            SystemClipboard::SetHost({[](void* w) { return static_cast<platform::Window*>(w)->GetClipboardText(); },
                                          [](void* w, StringView text) { return static_cast<platform::Window*>(w)->SetClipboardText(text); }, window_});
        }
        else
        {
            width_ = u32(options_.screenWidth > 0 ? options_.screenWidth : kBatchWidth);
            height_ = u32(options_.screenHeight > 0 ? options_.screenHeight : kBatchHeight);
        }

        if (!options_.noGraphics && !CreateDevice())
        {
            // Android gets its surface later; elsewhere a missing device ends the run.
#if !HX_PLATFORM_ANDROID
            return false;
#endif
        }
        if (options_.noGraphics && !options_.batchMode)
            HX_LOG_WARNING(PlayerLogChannel, "-nographics applies with -batchmode; the window opens without drawing.");
        ReportScreen(true);

        // The scripts of the data folder (C++ game modules, C# assemblies) before a scene can name them.
        scripts_.Start(root, options_.managedFolder.View());

        // The scene to run: -scene, else the first of the build.
        if (!options_.scene.Empty())
            SceneManager::LoadScene(options_.scene);
        else if (SceneManager::sceneCountInBuildSettings() > 0)
            SceneManager::LoadScene(0);
        else
            HX_LOG_WARNING(PlayerLogChannel, "Nothing to play: the data folder has no build scenes and no -scene was given.");
        return true;
    }

    bool Player::CreateDevice()
    {
        rhi::DeviceDesc desc;
        desc.backend = BackendFor(options_);
        desc.window = window_ ? window_->GetNativeHandle() : platform::NativeWindowHandle{};
        if (window_ && !desc.window.window)
            return false;
        desc.adapter = options_.deviceIndex;
        desc.warp = options_.warp;
        desc.vsync = !options_.batchMode;
        desc.swapChainDepthFormat = rhi::Format::D24UnormS8Uint;
        // The render thread submits a frame while the next one is made (the RHI runs inline where there are no threads).
        desc.threaded = options_.renderThread;
        if (options_.glVersion != 0)
            HX_LOG_INFO(PlayerLogChannel, "-force-glcore{}: the driver's newest core context is used, OpenGL {}.{} or later is needed.", options_.glVersion,
                        options_.glVersion / 10, options_.glVersion % 10);
        device_ = rhi::Device::Create(desc);
        if (!device_)
        {
            HX_LOG_ERROR(PlayerLogChannel, "No {} graphics device (adapter {}).", BackendName(desc.backend), options_.deviceIndex);
            return false;
        }
        deviceWindow_ = desc.window;
        const rhi::DeviceCaps& caps = device_->Caps();
        HX_LOG_INFO(PlayerLogChannel, "Graphics: {} {}.{} on {}{}.", BackendName(caps.backend), caps.apiMajor, caps.apiMinor, caps.renderer,
                    device_->Threaded() ? ", render thread" : "");
        Graphics::SetDevice(device_);
        UpdateTarget();
        for (ExtraWindow& w : extraWindows_)
            w.chain = device_->CreateSwapChain({.window = w.window->GetNativeHandle(), .vsync = false});
        UpdateDisplayWindows();
        return true;
    }

    void Player::DestroyDevice()
    {
        if (!device_)
            return;
        device_->WaitIdle();
        Graphics::SetDevice(nullptr);
        for (ExtraWindow& w : extraWindows_)
            if (w.chain.IsValid())
            {
                device_->Destroy(w.chain);
                w.chain = {};
            }
        device_->Destroy(color_);
        device_->Destroy(depth_);
        color_ = {};
        depth_ = {};
        rhi::Device::Destroy(device_);
        device_ = nullptr;
        deviceWindow_ = {};
    }

    // The default target of the cameras: the swap chain, or the batch mode textures, at the current size.
    void Player::UpdateTarget()
    {
        u32 w = width_, h = height_;
        if (window_)
        {
            i32 fw = 0, fh = 0;
            window_->GetFramebufferSize(fw, fh);
            w = u32(fw > 0 ? fw : 0);
            h = u32(fh > 0 ? fh : 0);
        }
        bool sizeChanged = w != Graphics::Target().width || h != Graphics::Target().height;
        if (!device_)
        {
            width_ = w;
            height_ = h;
            return;
        }
        if (!sizeChanged && (window_ || color_.IsValid()))
            return;
        width_ = w;
        height_ = h;
        const rhi::DeviceCaps& caps = device_->Caps();
        if (window_)
        {
            if (w > 0 && h > 0)
                device_->Resize(w, h);
            Graphics::SetTarget({{}, {}, w, h, caps.swapChainFormat, caps.swapChainDepthFormat});
            return;
        }
        device_->Destroy(color_);
        device_->Destroy(depth_);
        color_ = device_->CreateTexture({.format = rhi::Format::RGBA8Unorm, .width = w, .height = h,
                                         .usage = rhi::TextureRenderTarget | rhi::TextureSampled, .debugName = "PlayerFrame"});
        depth_ = device_->CreateTexture({.format = rhi::Format::D24UnormS8Uint, .width = w, .height = h, .usage = rhi::TextureDepthStencil,
                                         .debugName = "PlayerFrameDepth"});
        Graphics::SetTarget({color_, depth_, w, h, rhi::Format::RGBA8Unorm, rhi::Format::D24UnormS8Uint});
    }

    void Player::ReportScreen(bool displaysChanged)
    {
        u32 display = window_ ? window_->GetDisplay() : 0;
        // The displays and the modes of the window's display only when something may have changed them (asking the
        // system for the modes takes milliseconds).
        if (displaysChanged || displaysDirty_ || display != modesDisplay_)
        {
            if (platform::Window::IsSupported())
                platform::GetDisplays(displays_);
            modes_.Clear();
            if (display < displays_.Size())
                platform::GetDisplayModes(display, modes_);
            modesDisplay_ = display;
            displaysDirty_ = false;
        }
        // The monitors are displays (the tools' windows come after them).
        if (i32(displays_.Size()) > Displays::Count() && extraWindows_.Empty())
            Displays::SetCount(i32(displays_.Size()));
        ScreenReport r;
        r.width = i32(width_);
        r.height = i32(height_);
        r.display = display;
        if (window_)
        {
            window_->GetPosition(r.windowPosition.x, r.windowPosition.y);
            r.fullScreenMode = FullScreenModeFor(window_->GetMode());
            r.dpi = window_->GetContentScale() * 96.0f;
        }
        for (const platform::DisplayInfo& d : displays_)
        {
            DisplayInfo info;
            info.name = d.name;
            info.width = d.width;
            info.height = d.height;
            info.refreshRate = {d.refreshNumerator, d.refreshDenominator};
            info.workArea = {d.workX, d.workY, d.workWidth, d.workHeight};
            r.displays.Push(Move(info));
        }
        if (display < displays_.Size())
        {
            const platform::DisplayInfo& d = displays_[display];
            if (r.dpi == 0)
                r.dpi = d.dpi;
            for (const platform::DisplayMode& m : modes_)
                r.resolutions.Push({m.width, m.height, {m.refreshNumerator, m.refreshDenominator}});
            // Windowed, the current resolution is the window's; full screen, the display's.
            bool full = r.fullScreenMode == FullScreenMode::FullScreenWindow || r.fullScreenMode == FullScreenMode::ExclusiveFullScreen;
            r.currentResolution = full ? Resolution{d.width, d.height, {d.refreshNumerator, d.refreshDenominator}}
                                       : Resolution{r.width, r.height, {d.refreshNumerator, d.refreshDenominator}};
        }
        else
            r.currentResolution = {r.width, r.height, {}};
#if HX_PLATFORM_ANDROID
        r.fullScreenMode = FullScreenMode::FullScreenWindow;
        r.orientation = width_ >= height_ ? ScreenOrientation::LandscapeLeft : ScreenOrientation::Portrait;
#endif
        Screen::Report(r);
    }

    void Player::ApplyScreenRequests()
    {
        ScreenRequests r;
        if (!Screen::TakeRequests(r))
            return;
        if (r.resolution)
        {
            if (window_)
            {
                platform::WindowMode mode = WindowModeFor(r.fullScreenMode);
                platform::WindowMode was = window_->GetMode();
                bool wasFull = was == platform::WindowMode::FullScreen || was == platform::WindowMode::ExclusiveFullScreen;
                if (mode == platform::WindowMode::Windowed)
                {
                    window_->SetMode(mode, 0, {});
                    // Full screen draws at the display's size (no scaled lower resolution yet): leaving it with that
                    // size, as Screen.fullScreen = false does, gives the window back its own size.
                    if (!wasFull || r.width != i32(width_) || r.height != i32(height_))
                        window_->SetSize(r.width, r.height);
                }
                else
                    window_->SetMode(mode, window_->GetDisplay(), {r.width, r.height, r.refreshRate.numerator, r.refreshRate.denominator});
                displaysDirty_ = true;
            }
            else if (r.width > 0 && r.height > 0)
            {
                // The batch mode textures follow at the next frame.
                width_ = u32(r.width);
                height_ = u32(r.height);
            }
        }
        if (r.move)
        {
            if (window_ && r.moveDisplay < displays_.Size())
            {
                const platform::DisplayInfo& d = displays_[r.moveDisplay];
                if (window_->GetMode() == platform::WindowMode::Windowed)
                    window_->SetPosition(d.x + r.movePosition.x, d.y + r.movePosition.y);
                else
                    window_->SetMode(window_->GetMode(), r.moveDisplay, {});
            }
            Screen::FinishWindowMove(r.moveOperation);
        }
        if (r.sleepTimeout)
            platform::SetKeepScreenOn(r.sleepTimeoutValue == SleepTimeout::NeverSleep);
        if (r.orientation)
            platform::SetDisplayOrientation(platform::DisplayOrientation(u8(r.orientationValue)));
        // Brightness and the screen's MSAA stay as reported: the desktop systems and the swap chain have neither.
    }

    void Player::HandleEvents()
    {
        events_.Clear();
        if (!window_)
            return;
        window_->PollEvents(events_);
        for (const platform::Event& e : events_)
        {
            switch (e.type)
            {
            case platform::EventType::Quit:
            case platform::EventType::WindowClose:
                Application::Quit(0);
                break;
            case platform::EventType::WindowFocus:
            case platform::EventType::WindowBlur:
                if (extraWindows_.Empty())
                    Application::SetFocused(e.type == platform::EventType::WindowFocus);
                break;
            case platform::EventType::WindowResize:
                displaysDirty_ = true;
                break;
            default:
                break;
            }
        }
        // The input to the engine, where the UI and the game read it (none is taken away).
        inputEvents_.Clear();
        f64 now = Time::realtimeSinceStartupAsDouble();
        for (const platform::Event& e : events_)
            if (u8(e.type) >= u8(platform::EventType::KeyDown))
            {
                InputEvent ie;
                ie.event = e;
                ie.time = now;
                inputEvents_.Push(ie);
            }
        // The other displays' windows: their input tells their display; a window's focus is the application's while
        // any of its windows has it.
        bool focusChanged = false;
        for (ExtraWindow& w : extraWindows_)
        {
            extraEvents_.Clear();
            w.window->PollEvents(extraEvents_);
            for (const platform::Event& e : extraEvents_)
            {
                if (e.type == platform::EventType::WindowFocus || e.type == platform::EventType::WindowBlur)
                    focusChanged = true;
                if (u8(e.type) >= u8(platform::EventType::KeyDown))
                {
                    InputEvent ie;
                    ie.event = e;
                    ie.time = now;
                    ie.display = w.display;
                    inputEvents_.Push(ie);
                }
            }
        }
        for (const platform::Event& e : events_)
            focusChanged = focusChanged || e.type == platform::EventType::WindowFocus || e.type == platform::EventType::WindowBlur;
        if (focusChanged && !extraWindows_.Empty())
        {
            bool any = window_->HasFocus();
            for (const ExtraWindow& w : extraWindows_)
                any = any || w.window->HasFocus();
            Application::SetFocused(any);
        }
        UpdateDisplayWindows();
        InputFeed::SetFrame(Span<const InputEvent>(inputEvents_.Data(), inputEvents_.Size()), window_->Composition());
    }

    void Player::ApplyTextInput()
    {
        i32 x = 0, y = 0, h = 0;
        bool on = InputFeed::TakeTextInputRequest(x, y, h);
        if (window_)
            window_->SetTextInput(on, x, y, h);
    }

    void Player::ApplyCursor()
    {
        // The frame's cursor, once it changed (what the platform cannot show leaves the pointer as it was).
        const Cursor::State& c = Cursor::Current();
        if (!window_ || c.version == cursorVersion_)
            return;
        cursorVersion_ = c.version;
        if (c.image)
            window_->SetCursorImage(Span<const u8>(c.pixels.Data(), c.pixels.Size()), c.width, c.height, c.hotspotX, c.hotspotY);
        else
            window_->SetCursor(c.shape);
    }

    // ------------------------------------------------------------------------------------------- the displays

    bool Player::PlaceDisplay(void* user, i32 index, i32 width, i32 height, i32 x, i32 y)
    {
        auto* p = static_cast<Player*>(user);
        if (!p->window_ || index <= 0)
            return false;
        for (ExtraWindow& w : p->extraWindows_)
            if (w.display == index)
            {
                if (width > 0 && height > 0)
                    w.window->SetSize(width, height);
                if (x != Displays::kNoPlace && y != Displays::kNoPlace)
                    w.window->SetPosition(x, y);
                p->UpdateDisplayWindows();
                return true;
            }
        // A game display: a window on its monitor (the monitor's work area unless a size is given).
        platform::WindowDesc desc;
        String title = Format("{} {}", Application::productName(), index + 1);
        desc.title = title.View();
        desc.display = u32(index);
        desc.width = width > 0 ? width : 1280;
        desc.height = height > 0 ? height : 720;
        if (usize(index) < p->displays_.Size() && width <= 0)
        {
            desc.width = p->displays_[usize(index)].workWidth;
            desc.height = p->displays_[usize(index)].workHeight;
        }
        if (x != Displays::kNoPlace && y != Displays::kNoPlace)
        {
            desc.x = x;
            desc.y = y;
        }
        return p->OpenDisplayWindow(index, desc, false);
    }

    bool Player::OpenDisplayWindow(i32 display, const platform::WindowDesc& desc, bool tool)
    {
        platform::Window* window = platform::Window::Create(desc);
        if (!window)
        {
            HX_LOG_ERROR(PlayerLogChannel, "The window of display {} did not open.", display);
            return false;
        }
        ExtraWindow w;
        w.display = display;
        w.window = window;
        w.tool = tool;
        if (device_)
            w.chain = device_->CreateSwapChain({.window = window->GetNativeHandle(), .vsync = false});
        extraWindows_.Push(w);
        UpdateDisplayWindows();
        return true;
    }

    i32 Player::OpenToolWindow(const ToolWindowDesc& d)
    {
        if (!window_)
            return -1;
        platform::WindowDesc desc;
        desc.title = d.title;
        desc.width = d.width;
        desc.height = d.height;
        desc.x = d.x;
        desc.y = d.y;
        desc.customFrame = d.customFrame;
        desc.owner = window_->GetNativeHandle().window;
        DisplayState state;
        state.systemWidth = state.renderingWidth = d.width;
        state.systemHeight = state.renderingHeight = d.height;
        i32 display = Displays::Add(state);
        if (!OpenDisplayWindow(display, desc, true))
        {
            Displays::Remove(display);
            return -1;
        }
        return display;
    }

    void Player::CloseToolWindow(i32 display)
    {
        for (usize i = 0; i < extraWindows_.Size(); i++)
            if (extraWindows_[i].display == display && extraWindows_[i].tool)
            {
                CloseDisplayWindow(i);
                return;
            }
    }

    platform::Window* Player::DisplayWindow(i32 display) const
    {
        if (display == 0)
            return window_;
        for (const ExtraWindow& w : extraWindows_)
            if (w.display == display)
                return w.window;
        return nullptr;
    }

    void Player::CloseDisplayWindow(usize at)
    {
        ExtraWindow w = extraWindows_[at];
        extraWindows_.RemoveAt(at);
        if (device_ && w.chain.IsValid())
        {
            device_->WaitIdle();
            device_->Destroy(w.chain);
        }
        platform::Window::Destroy(w.window);
        if (w.tool)
            Displays::Remove(w.display);
        else
            Displays::Set(w.display, DisplayState());
    }

    // The displays' states from their windows: the swap chains follow the windows' sizes.
    void Player::UpdateDisplayWindows()
    {
        for (ExtraWindow& w : extraWindows_)
        {
            i32 fw = 0, fh = 0, x = 0, y = 0;
            w.window->GetFramebufferSize(fw, fh);
            w.window->GetPosition(x, y);
            const DisplayState& was = Displays::Get(w.display);
            if (device_ && w.chain.IsValid() && (fw != was.systemWidth || fh != was.systemHeight) && fw > 0 && fh > 0)
                device_->Resize(w.chain, u32(fw), u32(fh));
            DisplayState state;
            state.active = true;
            state.swapChain = w.chain;
            state.systemWidth = fw;
            state.systemHeight = fh;
            state.renderingWidth = was.renderingWidth > 0 && was.active ? was.renderingWidth : fw;
            state.renderingHeight = was.renderingHeight > 0 && was.active ? was.renderingHeight : fh;
            state.x = x;
            state.y = y;
            Displays::Set(w.display, state);
        }
        // The main window's place too (RelativeMouseAt, the tools' drags between windows).
        if (window_)
        {
            DisplayState main = Displays::Get(0);
            window_->GetPosition(main.x, main.y);
            main.systemWidth = main.renderingWidth = i32(width_);
            main.systemHeight = main.renderingHeight = i32(height_);
            main.active = true;
            Displays::Set(0, main);
        }
    }

    bool Player::Frame()
    {
        if (!started_ || quit_)
            return false;
        HandleEvents();
        // The surface of the window can come and go (Android): the device follows it.
        if (window_ && !options_.noGraphics)
        {
            platform::NativeWindowHandle now = window_->GetNativeHandle();
            if (device_ && !SameHandle(now, deviceWindow_))
                DestroyDevice();
            if (!device_ && now.window)
                CreateDevice();
#if HX_PLATFORM_ANDROID
            platform::android::AcknowledgeNativeWindow(device_ ? static_cast<ANativeWindow*>(deviceWindow_.window) : nullptr);
#endif
        }
        UpdateTarget();
        ReportScreen(false);
        // Paused without focus (unless running in the background) and while a window has no surface (Android in the
        // background); a quit still runs its frame.
        bool waitingForSurface = window_ && !options_.noGraphics && !device_;
        if ((!Application::ShouldRun() || waitingForSurface) && !(window_ && window_->CloseRequested()))
        {
            platform::SleepSeconds(0.01);
            return true;
        }
        bool last = options_.frames > 0 && frames_ + 1 >= u64(options_.frames);
        if (last)
        {
            if (!options_.screenshot.Empty())
                ScreenCapture::CaptureScreenshot(options_.screenshot);
            Application::Quit(0);
        }
        bool drawing = device_ && width_ > 0 && height_ > 0;
        if (drawing)
            device_->BeginFrame();
        bool running = Application::RunFrame();
        if (drawing)
        {
            device_->EndFrame();
            for (const ExtraWindow& w : extraWindows_)
                if (w.chain.IsValid())
                    device_->Present(w.chain);
            if (window_)
                device_->Present();
        }
        frames_++;
        ApplyScreenRequests();
        ApplyTextInput();
        ApplyCursor();
        if (!running)
        {
            quit_ = true;
            exitCode_ = Application::ExitCode();
        }
        return running;
    }

    void Player::Shutdown()
    {
        if (!started_)
            return;
        started_ = false;
        DestroyDevice();
        while (!extraWindows_.Empty())
            CloseDisplayWindow(extraWindows_.Size() - 1);
        Displays::SetHost(nullptr, nullptr);
        Displays::SetCount(1);
        if (window_)
        {
            SystemClipboard::SetHost({});
            platform::Window::Destroy(window_);
            window_ = nullptr;
        }
        // The engine back to its state at startup, so a process can run the player again.
        SceneManager::Reset();
        DestroyAllObjects();
        // The modules and the C# side once their scripts are gone.
        scripts_.Stop();
        ResetAssets();
        data_.Close();
        Shader::Globals().Clear();
        Graphics::SetTarget({});
        Screen::Reset();
        InputFeed::Reset();
        Cursor::Reset();
        cursorVersion_ = ~0ull;
        Time::Reset();
        Application::Reset();
        if (jobs_)
        {
            jobs::Shutdown();
            jobs_ = false;
        }
        instance_.Release();
        log_.Close();
        displays_.Clear();
        displaysDirty_ = true;
        width_ = height_ = 0;
    }

    namespace
    {
        struct Run
        {
            Player player;
        };

        bool RunFrame(void* user) { return static_cast<Run*>(user)->player.Frame(); }
    }

    i32 RunPlayer(Span<const StringView> args, engine::DataFileReader* reader)
    {
        PlayerOptions options;
        Array<String> errors;
        if (!ParseCommandLine(args, options, errors))
            for (const String& e : errors)
                HX_LOG_ERROR(PlayerLogChannel, "Command line: {}", e);
#if HX_PLATFORM_WEB
        // The browser runs the frames after main returns: the player lives on (main returns to the .NET runtime that
        // called it, which must not be unwound). Batch mode without graphics draws nothing: its frames run in main,
        // which answers the exit code (a run in node).
        static Run run;
        if (!run.player.Start(options, reader))
            return 1;
        if (options.batchMode && options.noGraphics)
        {
            while (run.player.Frame())
            {
            }
            i32 code = run.player.ExitCode();
            run.player.Shutdown();
            return code;
        }
        platform::RunMainLoop(&RunFrame, &run, true);
        return 0;
#else
        Run run;
        if (!run.player.Start(options, reader))
        {
            // Another instance running (-single-instance) is no failure.
            i32 code = run.player.Running() ? 1 : 0;
            run.player.Shutdown();
            return code;
        }
        platform::RunMainLoop(&RunFrame, &run);
        i32 code = run.player.ExitCode();
        run.player.Shutdown();
        return code;
#endif
    }
}
