#pragma once

#include <Helix/Engine/Asset/Asset.h>
#include <Helix/Engine/Input/InputFeed.h>
#include <Helix/Player/CommandLine/CommandLine.h>
#include <Helix/Player/Data/DataFolder.h>
#include <Helix/Player/Log/PlayerLog.h>
#include <Helix/Player/Scripts/PlayerScripts.h>
#include <Helix/Platform/System/Process.h>
#include <Helix/Platform/Window/Window.h>
#include <Helix/RHI/Device/Device.h>

// The player: the data folder and its scripts, the log, a window and a graphics device from the command line, and the
// frame: events to the engine, Application::RunFrame through the player loop, Present, then what scripts asked of the
// Screen. -batchmode draws into a texture without a window, -nographics runs without a device.
namespace helix::player
{
    class Player
    {
    public:
        Player() = default;
        ~Player() { Shutdown(); }
        Player(const Player&) = delete;
        Player& operator=(const Player&) = delete;

        // False when the player cannot run (the log says why); the reader reads the data folder (null: files).
        bool Start(const PlayerOptions& options, engine::DataFileReader* reader = nullptr);
        // One frame; false once the application has quit.
        bool Frame();
        // Back to the state before Start: the engine's objects, assets and settings too.
        void Shutdown();

        i32 ExitCode() const { return exitCode_; }
        u64 FrameCount() const { return frames_; }
        bool Running() const { return started_ && !quit_; }
        const PlayerOptions& Options() const { return options_; }
        platform::Window* Window() const { return window_; }
        rhi::Device* Device() const { return device_; }
        const DataFolder& Data() const { return data_; }
        const PlayerScripts& Scripts() const { return scripts_; }

        // A window of a tool drawn as a display of its own (an editor's floating window): its client area's place and
        // size on the desktop, a frame of its own (its hit test set on DisplayWindow), owned by the main window. Its
        // display index (Displays), -1 when it could not open.
        struct ToolWindowDesc
        {
            StringView title = "Helix";
            i32 x = 0, y = 0, width = 400, height = 300;
            bool customFrame = true;
        };
        i32 OpenToolWindow(const ToolWindowDesc& desc);
        void CloseToolWindow(i32 display);
        // The window of a display (0: the main window), null for none.
        platform::Window* DisplayWindow(i32 display) const;

    private:
        // A window of a display other than the main one: a game display activated, or a tool's window.
        struct ExtraWindow
        {
            i32 display = 0;
            platform::Window* window = nullptr;
            rhi::SwapChainHandle chain{};
            bool tool = false;
        };
        static bool PlaceDisplay(void* user, i32 index, i32 width, i32 height, i32 x, i32 y);
        bool OpenDisplayWindow(i32 display, const platform::WindowDesc& desc, bool tool);
        void UpdateDisplayWindows();
        void CloseDisplayWindow(usize at);

        bool CreateDevice();
        void DestroyDevice();
        void UpdateTarget();
        void ReportScreen(bool displaysChanged);
        void ApplyScreenRequests();
        void HandleEvents();
        // The input method on while the frame asked for it (InputFeed::RequestTextInput), at its caret.
        void ApplyTextInput();
        void ApplyCursor();

        PlayerOptions options_;
        DataFolder data_;
        PlayerScripts scripts_;
        PlayerLog log_;
        platform::Window* window_ = nullptr;
        platform::NativeWindowHandle deviceWindow_{};
        rhi::Device* device_ = nullptr;
        // Batch mode: the frame's textures.
        rhi::TextureHandle color_, depth_;
        u32 width_ = 0, height_ = 0;
        Array<platform::Event> events_;
        Array<engine::InputEvent> inputEvents_;
        Array<ExtraWindow> extraWindows_;
        Array<platform::Event> extraEvents_;
        // The engine's cursor last given to the window (none yet: the window's own arrow).
        u64 cursorVersion_ = ~0ull;
        Array<platform::DisplayInfo> displays_;
        Array<platform::DisplayMode> modes_;
        u32 modesDisplay_ = ~0u;
        bool displaysDirty_ = true;
        bool started_ = false;
        bool quit_ = false;
        platform::SingleInstance instance_;
        bool jobs_ = false;
        i32 exitCode_ = 0;
        u64 frames_ = 0;
    };

    // A whole run from the arguments after the executable: start, the main loop, shut down; the exit code. On the Web
    // the loop goes on after this returns.
    i32 RunPlayer(Span<const StringView> args, engine::DataFileReader* reader = nullptr);
}
