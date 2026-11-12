#include <Support/PlayerRuns.h>
#include <Support/TestData.h>

#include <Helix/Engine/Script/MonoScript.h>
#include <Helix/Player/Scripts/PlayerScripts.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::player;
using namespace helix::player::testing;

namespace
{
#if defined(HELIX_PLAYER_EXECUTABLE) && defined(HELIX_PLAYER_TEST_MODULE)
    // A data folder whose scene has a GameObject with a C# script and a C++ script: the engine's assemblies and the
    // test assembly in Managed/, the test game module in Modules/, the two script assets.
    struct ScriptedDataFolder
    {
        String root;

        ScriptedDataFolder()
        {
            using namespace engine;
            root = platform::JoinPath(platform::TempDirectory(), Format("helix-player-scripts-{}", Guid::Generate().ToString()));
            platform::CreateDirectories(root);
            StringView assemblies = HELIX_PLAYER_ENGINE_ASSEMBLIES;
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
