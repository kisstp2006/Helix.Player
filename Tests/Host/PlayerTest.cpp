#include <Support/Golden.h>
#include <Support/PlayerRuns.h>
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


using namespace helix;
using namespace helix::player;
using namespace helix::player::testing;

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
