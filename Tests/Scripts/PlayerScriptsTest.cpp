#include <Support/PlayerRuns.h>
#include <Support/TestData.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Script/MonoScript.h>
#include <Helix/Player/Scripts/PlayerScripts.h>
#include <Helix/Testing/Test.h>

#include <chrono>

using namespace helix;
using namespace helix::player;
using namespace helix::player::testing;

namespace
{
#if (defined(HELIX_PLAYER_EXECUTABLE) && defined(HELIX_PLAYER_TEST_MODULE)) || defined(HELIX_PLAYER_WEB_SCRIPTS)
    // A data folder whose scene has a GameObject with a C# script and a C++ script: the engine's assemblies and the
    // test assembly in Managed/, the test game module in Modules/, the two script assets. On the Web the app carries
    // the assemblies (Managed/ names the game's) and there is no module.
    struct ScriptedDataFolder
    {
        String root;

        // The engine's assemblies: paths joined with | (the build's, ReadyToRun images in release builds).
        explicit ScriptedDataFolder(StringView engineAssemblies = StringView())
        {
            (void)engineAssemblies;
            using namespace engine;
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-player-scripts-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
#if defined(HELIX_PLAYER_WEB_SCRIPTS)
            Write(Format("{}/HelixPlayer.TestScripts.dll", kManagedFolder), "carried by the web app");
#else
            StringView assemblies = engineAssemblies.Empty() ? StringView(HELIX_PLAYER_ENGINE_ASSEMBLIES) : engineAssemblies;
            for (usize at = 0; at < assemblies.Size();)
            {
                usize end = assemblies.Find('|', at);
                if (end == StringView::npos)
                    end = assemblies.Size();
                Copy(assemblies.Substr(at, end - at), kManagedFolder);
                at = end + 1;
            }
            Copy(HELIX_PLAYER_TEST_SCRIPTS, kManagedFolder);
            Copy(HELIX_PLAYER_TEST_MODULE, kModulesFolder);
#endif

            Guid greeter = Guid::Generate(), spinner = Guid::Generate(), scene = Guid::Generate();
            WriteScript("Scripts/Greeter.asset", "Greeter", "HelixPlayer.TestScripts");
            WriteScript("Scripts/Spinner.asset", "Spinner", "PlayerTestModule");
            String yaml = Format(R"(%YAML 1.1
%TAG !h! tag:helixengine,2026:
--- !h!1 &100
GameObject:
  m_Component:
  - component: {{fileID: 101}}
  - component: {{fileID: 102}}
  - component: {{fileID: 103}}
  m_Name: Scripted
  m_IsActive: 1
--- !h!4 &101
Transform:
  m_GameObject: {{fileID: 100}}
  m_Children: []
  m_Father: {{fileID: 0}}
--- !h!114 &102
GameBehaviour:
  m_GameObject: {{fileID: 100}}
  m_Enabled: 1
  m_Script: {{fileID: 11500000, guid: {}, type: 3}}
--- !h!114 &103
GameBehaviour:
  m_GameObject: {{fileID: 100}}
  m_Enabled: 1
  m_Script: {{fileID: 11500000, guid: {}, type: 3}}
)",
                                 greeter.ToString(), spinner.ToString());
            Write("Scenes/Scripted.helix", yaml);
            const char* settings = R"(%YAML 1.1
%TAG !h! tag:helixengine,2026:
--- !h!129 &1
PlayerSettings:
  companyName: Helix Tests
  productName: Player Scripts Test
  bundleVersion: 1.0
--- !h!141 &2
BuildSettings:
  scenes:
  - Scenes/Scripted.helix
)";
            Write(kSettingsFile, settings);
            Array<AssetEntry> entries;
            Add(entries, greeter, "Scripts/Greeter.asset");
            Add(entries, spinner, "Scripts/Spinner.asset");
            Add(entries, scene, "Scenes/Scripted.helix");
            String error;
            WriteAssetIndex(root, Span<const AssetEntry>(entries.Data(), entries.Size()), error);
        }

        ~ScriptedDataFolder() { RemoveTree(root); }

        void Copy(StringView file, StringView folder)
        {
            Array<u8> bytes;
            if (!platform::ReadFile(file, bytes))
                std::printf("    %.*s is missing\n", int(file.Size()), file.Data());
            String to = platform::JoinPath(root, folder);
            platform::CreateDirectories(to);
            platform::WriteFile(platform::JoinPath(to, platform::GetFileName(file)), bytes);
        }

        void Write(StringView path, StringView text)
        {
            String full = platform::JoinPath(root, path);
            platform::CreateDirectories(String(platform::GetDirectoryName(full)));
            platform::WriteFile(full, Span<const u8>(reinterpret_cast<const u8*>(text.Data()), text.Size()));
        }

        // The script asset of a class in an assembly or game module.
        void WriteScript(StringView path, StringView name, StringView assembly)
        {
            using namespace engine;
            MonoScript* script = NewObject<MonoScript>();
            script->SetName(name);
            script->className = String(name);
            script->nameSpace = String("PlayerTests");
            script->assemblyName = String(assembly);
            Array<u8> bytes;
            String error;
            Object* const objects[] = {script};
            WriteAssetFile(Span<Object* const>(objects, 1), bytes, error, false);
            DestroyImmediate(script, true);
            Write(path, StringView(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
        }

        static void Add(Array<engine::AssetEntry>& entries, const Guid& guid, StringView path)
        {
            engine::AssetEntry e;
            e.guid = guid;
            e.path = String(path);
            entries.Push(Move(e));
        }
    };
#endif
}

HX_TEST(PlayerScripts, AFolderWithoutScriptsStartsNothing)
{
    TestDataFolder data;
    PlayerScripts scripts;
    scripts.Start(data.root);
    HX_CHECK(scripts.Modules().Size() == 0 && scripts.Assemblies().Size() == 0 && !scripts.ManagedRunning());
    scripts.Stop();
}

HX_TEST(PlayerScripts, TheExecutableRunsCSharpAndCppScriptsOfTheDataFolder)
{
#if defined(HELIX_PLAYER_EXECUTABLE) && defined(HELIX_PLAYER_TEST_MODULE)
    ScriptedDataFolder data;
    String log = platform::JoinPath(platform::TempDirectory(), Format("helix-player-{}.log", Guid::Generate().ToString()));
    Array<StringView> args;
    const StringView fixed[] = {"-batchmode", "-nographics", "-frames", "4", "-dataFolder"};
    for (StringView a : fixed)
        args.Push(a);
    args.Push(data.root);
    args.Push("-logFile");
    args.Push(log);
    i32 code = RunProcess(Span<const StringView>(args.Data(), args.Size()));
    Array<u8> logBytes;
    platform::ReadFile(log, logBytes);
    StringView text(reinterpret_cast<const char*>(logBytes.Data()), logBytes.Size());
    bool clean = !text.Contains("[warning]") && !text.Contains("[error]");
    if (code != 0 || !clean)
        std::printf("%.*s\n", int(text.Size()), text.Data());
    HX_CHECK(code == 0 && clean);
    // The module and the assembly, the messages of both scripts in the scene, SendMessage from C# to C++, and their
    // OnDestroy when the player shuts down.
    const char* expected[] = {"Game module PlayerTestModule loaded.", "C# assembly HelixPlayer.TestScripts.dll loaded: 1 script classes.",
                              "Greeter.Awake on Scripted", "Spinner.Awake on Scripted", "Spinner.Start", "Greeter.Start",
                              "Spinner.Hello", "Spinner.Update twice", "Greeter.OnDestroy", "Spinner.OnDestroy"};
    for (const char* e : expected)
    {
        if (!text.Contains(e))
            std::printf("    not in the log: %s\n", e);
        HX_CHECK(text.Contains(e));
    }
    platform::RemoveFile(log);
#endif
}

#if defined(HELIX_PLAYER_READY_TO_RUN)
namespace
{
    // A ReadyToRun image: an assembly whose CLI header names a native header (ManagedNativeHeader, at 0x40 of it).
    bool IsReadyToRun(StringView path)
    {
        Array<u8> pe;
        if (!platform::ReadFile(path, pe) || pe.Size() < 0x200)
            return false;
        auto u16At = [&](usize at) { return at + 2 <= pe.Size() ? u16(pe[at] | (pe[at + 1] << 8)) : u16(0); };
        auto u32At = [&](usize at) { return at + 4 <= pe.Size() ? u32(pe[at] | (pe[at + 1] << 8) | (pe[at + 2] << 16) | (u32(pe[at + 3]) << 24)) : 0u; };
        usize header = u32At(0x3C);
        usize optional = header + 24;
        bool plus = u16At(optional) == 0x20B;
        usize directories = optional + (plus ? 112 : 96);
        u32 cliRva = u32At(directories + 14 * 8);
        usize sections = optional + u16At(header + 20);
        for (u32 i = 0; i < u16At(header + 6); i++)
        {
            usize s = sections + i * 40;
            u32 va = u32At(s + 12), size = u32At(s + 16), raw = u32At(s + 20);
            if (cliRva >= va && cliRva < va + size)
                return u32At(raw + (cliRva - va) + 0x40) != 0;
        }
        return false;
    }

    // The player executable on a data folder in batch mode: its time to the end, and whether its log has the C#
    // script's messages.
    f64 TimedRun(StringView root, bool& scripted)
    {
        String log = platform::JoinPath(platform::TempDirectory(), Format("helix-player-{}.log", Guid::Generate().ToString()));
        const StringView args[] = {"-batchmode", "-nographics", "-frames", "1", "-dataFolder", root, "-logFile", log};
        auto start = std::chrono::steady_clock::now();
        i32 code = RunProcess(Span<const StringView>(args, 8));
        f64 ms = std::chrono::duration<f64, std::milli>(std::chrono::steady_clock::now() - start).count();
        Array<u8> bytes;
        platform::ReadFile(log, bytes);
        StringView text(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
        scripted = code == 0 && text.Contains("Greeter.Start");
        platform::RemoveFile(log);
        return ms;
    }
}
#endif

HX_TEST(PlayerScripts, TheReleasePlayerTakesReadyToRunImages)
{
#if defined(HELIX_PLAYER_READY_TO_RUN) && defined(HELIX_PLAYER_EXECUTABLE) && defined(HELIX_PLAYER_TEST_MODULE)
    ScriptedDataFolder r2r(HELIX_PLAYER_ENGINE_ASSEMBLIES), il(HELIX_PLAYER_IL_ASSEMBLIES);
    String core = platform::JoinPath(platform::JoinPath(r2r.root, kManagedFolder), kCoreModuleAssembly);
    String coreIl = platform::JoinPath(platform::JoinPath(il.root, kManagedFolder), kCoreModuleAssembly);
    HX_CHECK(IsReadyToRun(core) && !IsReadyToRun(coreIl));
    // Start up to the end of one frame with the C# scripts running, the best of three runs each.
    f64 best[2] = {1e9, 1e9};
    for (int round = 0; round < 3; round++)
        for (int k = 0; k < 2; k++)
        {
            bool scripted = false;
            f64 ms = TimedRun(k == 0 ? r2r.root : il.root, scripted);
            HX_CHECK(scripted);
            best[k] = ms < best[k] ? ms : best[k];
        }
    std::printf("    player start to the first frame with C# scripts: %.0f ms with ReadyToRun, %.0f ms with IL only\n", best[0], best[1]);
#endif
}

#if defined(HELIX_PLAYER_WEB_SCRIPTS)
namespace
{
    void CollectLog(const LogMessage& message, void* user) { static_cast<Array<String>*>(user)->Push(String(message.text)); }
}
#endif

HX_TEST(PlayerScripts, TheWebPlayerRunsCSharpScriptsOfTheDataFolder)
{
#if defined(HELIX_PLAYER_WEB_SCRIPTS)
    // The player in this process (the Web's), its C# side started by the browser runtime of the test app.
    ScriptedDataFolder data;
    Array<String> lines;
    AddLogSink(&CollectLog, &lines);
    const StringView args[] = {"-batchmode", "-nographics", "-nolog", "-dataFolder", data.root, "-frames", "4"};
    u64 frames = 0;
    i32 code = RunFrames(Span<const StringView>(args, 7), frames);
    RemoveLogSink(&CollectLog, &lines);
    HX_CHECK(code == 0 && frames == 4);
    const char* expected[] = {"C# assembly HelixPlayer.TestScripts.dll loaded: 1 script classes.", "Greeter.Awake on Scripted", "Greeter.Start",
                              "Greeter.OnDestroy"};
    for (const char* e : expected)
    {
        bool found = false;
        for (const String& l : lines)
            found = found || StringView(l).Contains(e);
        if (!found)
            std::printf("    not in the log: %s\n", e);
        HX_CHECK(found);
    }
#endif
}
