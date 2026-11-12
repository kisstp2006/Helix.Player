#include <Support/Golden.h>
#include <Support/PlayerRuns.h>
#include <Support/TestData.h>

#include <Helix/Platform/Time/Time.h>
#include <Helix/Testing/Test.h>

#include <cstdio>
#include <cstdlib>

using namespace helix;
using namespace helix::player;
using namespace helix::player::testing;

// The exit run of the engine runtime core: a scene written by hand (camera, light, corpus models with materials)
// loads from a data folder and renders in the C++ player on every device.
#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
namespace
{
    // The data folder as a build would leave it: the files of Tests/Exit/Data, the corpus models and the shaders.
    struct ExitFolder
    {
        String root;

        ExitFolder()
        {
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-exit-{}", Guid::Generate().ToString()));
            CopyTree(HELIX_PLAYER_EXIT_DATA, root);
            String models = platform::JoinPath(root, "Models");
            CopyTree(platform::JoinPath(HELIX_PLAYER_CORPUS_MODELS, "PolyHaven/wooden_crate_02/glTF"), platform::JoinPath(models, "wooden_crate_02"));
            CopyTree(platform::JoinPath(HELIX_PLAYER_CORPUS_MODELS, "Khronos/Suzanne/glTF"), platform::JoinPath(models, "Suzanne"));
            Array<u8> bytes;
            platform::ReadFile(platform::JoinPath(HELIX_PLAYER_CORPUS_MODELS, "Khronos/BoxVertexColors/glTF-Binary/BoxVertexColors.glb"), bytes);
            platform::WriteFile(platform::JoinPath(models, "BoxVertexColors.glb"), bytes);
            String shaders = platform::JoinPath(root, "Shaders");
            platform::CreateDirectories(shaders);
            platform::WriteFile(platform::JoinPath(shaders, "HelixPlayer.TestShaders.dll"),
                                Span<const u8>(kPlayerTestShadersAssembly, sizeof(kPlayerTestShadersAssembly)));
            platform::WriteFile(platform::JoinPath(shaders, "HelixPlayer.TestShaders.pdb"),
                                Span<const u8>(kPlayerTestShadersSymbols, sizeof(kPlayerTestShadersSymbols)));
        }

        ~ExitFolder() { RemoveTree(root); }
    };

    String LogText(StringView path)
    {
        Array<u8> bytes;
        platform::ReadFile(path, bytes);
        return String(StringView(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    }

    bool CleanLog(StringView text)
    {
        bool clean = !text.Contains("[warning]") && !text.Contains("[error]");
        if (!clean)
            std::printf("%.*s\n", int(text.Size()), text.Data());
        return clean;
    }

    constexpr StringView kWidth = "480";
    constexpr StringView kHeight = "270";
}
#endif

HX_TEST(Exit, TheSceneRendersOnEveryDevice)
{
#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
    ExitFolder data;
    // Kept to look at or to serve to the Web player with HELIX_PLAYER_EXPORT_EXIT_DATA=<folder>.
    if (const char* exportTo = std::getenv("HELIX_PLAYER_EXPORT_EXIT_DATA"))
        CopyTree(data.root, exportTo);
    u32 runs = 0;
    Array<DeviceArguments> devices = Devices();
    for (const DeviceArguments& device : devices)
    {
        // Batch mode (a texture) and a window (the swap chain): the same picture.
        for (int windowed = 0; windowed < 2; windowed++)
        {
            if (windowed && !platform::Window::IsSupported())
                continue;
            String shot = platform::JoinPath(platform::TempDirectory(), Format("helix-exit-{}.png", Guid::Generate().ToString()));
            String log = platform::JoinPath(platform::TempDirectory(), Format("helix-exit-{}.log", Guid::Generate().ToString()));
            Array<StringView> args;
            const StringView fixed[] = {"-dataFolder", data.root, "-frames", "3", "-screenshot", shot, "-logFile", log, "-screen-width", kWidth,
                                        "-screen-height", kHeight};
            for (StringView a : fixed)
                args.Push(a);
            if (!windowed)
                args.Push("-batchmode");
            for (const String& a : device.args)
                args.Push(a);
            u64 frames = 0;
            i32 code = RunFrames(Span<const StringView>(args.Data(), args.Size()), frames);
            std::printf("    %s %s: exit %d after %llu frames\n", device.name, windowed ? "window" : "batch", code, static_cast<unsigned long long>(frames));
            HX_CHECK(code == 0 && frames == 3);
            String text = LogText(log);
            HX_CHECK(text.View().Contains("Exit Scene") && CleanLog(text));
            image::Image img;
            HX_REQUIRE(ReadPng(shot, img));
            HX_CHECK(MatchGolden("ExitScene", reinterpret_cast<const u32*>(img.pixels.Data()), img.width, img.height, 4));
            platform::RemoveFile(shot);
            platform::RemoveFile(log);
            runs++;
        }
    }
    std::printf("    %u runs\n", runs);
#endif
}

HX_TEST(Exit, TheExecutableRendersTheScene)
{
#if defined(HELIX_PLAYER_EXECUTABLE)
    Array<DeviceArguments> devices = Devices();
    if (devices.Empty())
        return;
    ExitFolder data;
    String shot = platform::JoinPath(platform::TempDirectory(), Format("helix-exit-{}.png", Guid::Generate().ToString()));
    String log = platform::JoinPath(platform::TempDirectory(), Format("helix-exit-{}.log", Guid::Generate().ToString()));
    Array<StringView> args;
    const StringView fixed[] = {"-batchmode", "-frames", "3", "-dataFolder", data.root, "-screenshot", shot, "-logFile", log, "-screen-width", kWidth,
                                "-screen-height", kHeight};
    for (StringView a : fixed)
        args.Push(a);
    for (const String& a : devices[0].args)
        args.Push(a);
    HX_CHECK(RunProcess(Span<const StringView>(args.Data(), args.Size())) == 0);
    HX_CHECK(CleanLog(LogText(log)));
    image::Image img;
    HX_REQUIRE(ReadPng(shot, img));
    HX_CHECK(MatchGolden("ExitScene", reinterpret_cast<const u32*>(img.pixels.Data()), img.width, img.height, 4));
    platform::RemoveFile(shot);
    platform::RemoveFile(log);
#endif
}

HX_TEST(Exit, LoadAndFrameTimes)
{
#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
    // The first device: from Start to the first frame drawn (the data folder, the shaders compiled, the models
    // decoded and uploaded), then the mean of 200 frames in batch mode at 1280 x 720.
    Array<DeviceArguments> devices = Devices();
    if (devices.Empty())
        return;
    ExitFolder data;
    Array<StringView> args;
    const StringView fixed[] = {"-batchmode", "-nolog", "-dataFolder", data.root, "-screen-width", "1280", "-screen-height", "720"};
    for (StringView a : fixed)
        args.Push(a);
    for (const String& a : devices[0].args)
        args.Push(a);
    PlayerOptions options;
    Array<String> errors;
    HX_REQUIRE(ParseCommandLine(Span<const StringView>(args.Data(), args.Size()), options, errors));
    Player player;
    platform::Stopwatch load;
    HX_REQUIRE(player.Start(options));
    HX_REQUIRE(player.Frame());
    f64 loadSeconds = load.ElapsedSeconds();
    for (int i = 0; i < 20; i++)
        player.Frame();
    platform::Stopwatch frames;
    constexpr int kFrames = 200;
    for (int i = 0; i < kFrames; i++)
        HX_REQUIRE(player.Frame());
    player.Device()->WaitIdle();
    f64 frameMs = frames.ElapsedSeconds() * 1000.0 / kFrames;
    std::printf("    %s: start to first frame %.1f ms, %.3f ms a frame at 1280 x 720\n", devices[0].name, loadSeconds * 1000.0, frameMs);
    player.Shutdown();
    HX_CHECK(loadSeconds > 0 && frameMs > 0);
#endif
}
