#include <Support/Golden.h>
#include <Support/TestData.h>

#include <Helix/Engine/Application/Application.h>
#include <Helix/Engine/Rendering/Graphics.h>
#include <Helix/Engine/Scene/SceneManager.h>
#include <Helix/Engine/Screen/Screen.h>
#include <Helix/Image/Codecs/Png.h>
#include <Helix/Player/Host/Player.h>
#include <Helix/RHI/Device/Device.h>
#include <Helix/Testing/Test.h>

#include <cstdio>
#include <cstdlib>

#if HX_PLATFORM_WINDOWS
#include <windows.h>
#endif

using namespace helix;
using namespace helix::player;
using namespace helix::player::testing;

namespace
{
    // The devices of this machine as the command line chooses them: every D3D11 adapter, OpenGL.
    struct DeviceArguments
    {
        const char* name;
        Array<String> args;
    };

#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
    // A device of the backend and adapter can be made here (CI machines list OpenGL without a usable driver).
    bool CanCreate(rhi::Backend backend, i32 adapter, bool warp = false)
    {
        rhi::Device* d = rhi::Device::Create({.backend = backend, .adapter = adapter, .warp = warp, .vsync = false});
        if (!d)
        {
            std::printf("    no %s device here, skipped\n", backend == rhi::Backend::D3D11 ? "D3D11" : "OpenGL");
            return false;
        }
        rhi::Device::Destroy(d);
        return true;
    }
#endif

    Array<DeviceArguments> Devices()
    {
        Array<DeviceArguments> out;
#if HX_PLATFORM_WINDOWS
        char name[128];
        for (u32 i = 0; rhi::Device::AdapterName(rhi::Backend::D3D11, i, name, sizeof(name)); i++)
        {
            if (!CanCreate(rhi::Backend::D3D11, i32(i)))
                continue;
            DeviceArguments d{"D3D11", Array<String>()};
            d.args.Push(String("-force-d3d11"));
            d.args.Push(String("-force-device-index"));
            d.args.Push(Format("{}", i));
            out.Push(Move(d));
        }
        if (out.Empty() && CanCreate(rhi::Backend::D3D11, -1, true))
        {
            DeviceArguments d{"D3D11 WARP", Array<String>()};
            d.args.Push(String("-force-d3d11"));
            d.args.Push(String("-force-driver-type-warp"));
            out.Push(Move(d));
        }
        if (rhi::Device::IsBackendAvailable(rhi::Backend::OpenGL) && CanCreate(rhi::Backend::OpenGL, -1))
        {
            DeviceArguments d{"OpenGL", Array<String>()};
            d.args.Push(String("-force-glcore"));
            out.Push(Move(d));
        }
#elif HX_PLATFORM_LINUX
        if (rhi::Device::IsBackendAvailable(rhi::Backend::OpenGL) && CanCreate(rhi::Backend::OpenGL, -1))
            out.Push(DeviceArguments{"OpenGL", Array<String>()});
#endif
        return out;
    }

    // A run of the player in this process: the arguments, frames until it quits; the exit code, -1 when it did not start.
    i32 RunFrames(Span<const StringView> args, u64& frames, u32 limit = 1000)
    {
        PlayerOptions options;
        Array<String> errors;
        if (!ParseCommandLine(args, options, errors))
            return -2;
        Player player;
        if (!player.Start(options))
            return -1;
        for (u32 i = 0; i < limit && player.Frame(); i++)
        {
        }
        frames = player.FrameCount();
        i32 code = player.Running() ? -3 : player.ExitCode();
        player.Shutdown();
        return code;
    }

#if defined(HELIX_PLAYER_EXECUTABLE)
    // Runs the player executable with the arguments and waits for it; its exit code, -1 when it did not start.
    i32 RunProcess(Span<const StringView> args)
    {
#if HX_PLATFORM_WINDOWS
        String line = Format("\"{}\"", HELIX_PLAYER_EXECUTABLE);
        for (StringView a : args)
            line.Append(Format(" \"{}\"", a));
        wchar_t wide[4096];
        int n = MultiByteToWideChar(CP_UTF8, 0, line.Data(), int(line.Size()), wide, 4095);
        wide[n > 0 ? n : 0] = 0;
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, wide, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
            return -1;
        WaitForSingleObject(pi.hProcess, 120000);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return i32(code);
#else
        String line = Format("'{}'", HELIX_PLAYER_EXECUTABLE);
        for (StringView a : args)
            line.Append(Format(" '{}'", a));
        int status = std::system(line.CStr());
        return status < 0 ? -1 : (status >> 8) & 0xff;
#endif
    }
#endif

    bool ReadPng(StringView path, image::Image& out)
    {
        Array<u8> bytes;
        return platform::ReadFile(path, bytes) && image::DecodePng(Span<const u8>(bytes.Data(), bytes.Size()), out);
    }
}

HX_TEST(Player, RendersTheFirstBuildSceneOnEveryDevice)
{
    TestDataFolder data;
    u32 runs = 0;
    for (const DeviceArguments& device : Devices())
    {
        // Batch mode (a texture) and a window (the swap chain): the same picture.
        for (int windowed = 0; windowed < 2; windowed++)
        {
            if (windowed && !platform::Window::IsSupported())
                continue;
            String shot = platform::JoinPath(platform::TempDirectory(), Format("helix-player-{}.png", Guid::Generate().ToString()));
            Array<String> args;
            args.Push(String("-dataFolder"));
            args.Push(data.root);
            args.Push(String("-frames"));
            args.Push(String("3"));
            args.Push(String("-screenshot"));
            args.Push(shot);
            args.Push(String("-screen-width"));
            args.Push(String("320"));
            args.Push(String("-screen-height"));
            args.Push(String("200"));
            args.Push(String("-nolog"));
            if (!windowed)
                args.Push(String("-batchmode"));
            for (const String& a : device.args)
                args.Push(a);
            Array<StringView> views;
            for (const String& a : args)
                views.Push(a);
            u64 frames = 0;
            i32 code = RunFrames(Span<const StringView>(views.Data(), views.Size()), frames);
            std::printf("    %s %s: exit %d after %llu frames\n", device.name, windowed ? "window" : "batch", code, static_cast<unsigned long long>(frames));
            HX_CHECK(code == 0 && frames == 3);
            image::Image img;
            HX_REQUIRE(ReadPng(shot, img));
            HX_CHECK(img.width == 320 && img.height == 200);
            HX_CHECK(MatchGolden("PlayerScene", reinterpret_cast<const u32*>(img.pixels.Data()), img.width, img.height, 4));
            platform::RemoveFile(shot);
            runs++;
        }
    }
    std::printf("    %u runs\n", runs);
}

HX_TEST(Player, TheExecutableRendersTheDataFolder)
{
#if defined(HELIX_PLAYER_EXECUTABLE)
    Array<DeviceArguments> devices = Devices();
    if (devices.Empty())
        return;
    TestDataFolder data;
    String shot = platform::JoinPath(platform::TempDirectory(), Format("helix-player-{}.png", Guid::Generate().ToString()));
    String log = platform::JoinPath(platform::TempDirectory(), Format("helix-player-{}.log", Guid::Generate().ToString()));
    Array<StringView> args;
    const StringView fixed[] = {"-batchmode", "-frames", "3", "-screen-width", "320", "-screen-height", "200", "-dataFolder"};
    for (StringView a : fixed)
        args.Push(a);
    args.Push(data.root);
    args.Push("-screenshot");
    args.Push(shot);
    args.Push("-logFile");
    args.Push(log);
    for (const String& a : devices[0].args)
        args.Push(a);
    i32 code = RunProcess(Span<const StringView>(args.Data(), args.Size()));
    Array<u8> logBytes;
    platform::ReadFile(log, logBytes);
    StringView logText(reinterpret_cast<const char*>(logBytes.Data()), logBytes.Size());
    if (code != 0 || logText.Contains("[warning]") || logText.Contains("[error]"))
        std::printf("%.*s\n", int(logText.Size()), logText.Data());
    HX_CHECK(code == 0);
    HX_CHECK(logText.Contains("Player Test") && !logText.Contains("[error]") && !logText.Contains("[warning]"));
    image::Image img;
    HX_REQUIRE(ReadPng(shot, img));
    HX_CHECK(MatchGolden("PlayerScene", reinterpret_cast<const u32*>(img.pixels.Data()), img.width, img.height, 4));
    platform::RemoveFile(shot);
    platform::RemoveFile(log);
#endif
}

HX_TEST(Player, NoGraphicsRunsTheLoopWithoutADevice)
{
    TestDataFolder data;
    const StringView args[] = {"-batchmode", "-nographics", "-nolog", "-dataFolder", data.root, "-frames", "5"};
    u64 frames = 0;
    HX_CHECK(RunFrames(Span<const StringView>(args, 7), frames) == 0 && frames == 5);
    // The engine is back to its state at startup.
    HX_CHECK(engine::Graphics::Device() == nullptr && engine::SceneManager::sceneCountInBuildSettings() == 0);
}

HX_TEST(Player, ScreenRequestsReachTheWindow)
{
    if (!platform::Window::IsSupported())
        return;
#if HX_PLATFORM_WINDOWS
    TestDataFolder data;
    const StringView args[] = {"-nolog", "-nographics", "-dataFolder", data.root, "-screen-width", "400", "-screen-height", "240"};
    PlayerOptions options;
    Array<String> errors;
    HX_REQUIRE(ParseCommandLine(Span<const StringView>(args, 8), options, errors));
    Player player;
    HX_REQUIRE(player.Start(options));
    HX_REQUIRE(player.Frame());
    HX_CHECK(engine::Screen::width() == 400 && engine::Screen::height() == 240);
    HX_CHECK(!engine::Screen::fullScreen() && !engine::Screen::resolutions().Empty());
    engine::Screen::SetResolution(480, 300, false);
    HX_REQUIRE(player.Frame());
    HX_REQUIRE(player.Frame());
    HX_CHECK(engine::Screen::width() == 480 && engine::Screen::height() == 300);
    engine::Screen::SetFullScreen(true);
    HX_REQUIRE(player.Frame());
    HX_REQUIRE(player.Frame());
    HX_CHECK(engine::Screen::fullScreen() && engine::Screen::width() == engine::Screen::mainWindowDisplayInfo().width);
    engine::Screen::SetFullScreen(false);
    HX_REQUIRE(player.Frame());
    HX_REQUIRE(player.Frame());
    HX_CHECK(!engine::Screen::fullScreen() && engine::Screen::width() == 480);
    engine::DisplayInfo display = engine::Screen::mainWindowDisplayInfo();
    engine::AsyncOperation move = engine::Screen::MoveMainWindowTo(display, Vector2Int{40, 30});
    HX_REQUIRE(player.Frame());
    HX_CHECK(move.isDone());
    HX_REQUIRE(player.Frame());
    HX_CHECK((engine::Screen::mainWindowPosition() == Vector2Int{40, 30}) || display.workArea.x != 0);
    engine::Application::Quit(0);
    HX_CHECK(!player.Frame() && player.ExitCode() == 0);
    player.Shutdown();
#endif
}
