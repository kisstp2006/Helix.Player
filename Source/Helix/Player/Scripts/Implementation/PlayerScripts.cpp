#include <Helix/Player/Scripts/PlayerScripts.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Platform/IO/FileSystem.h>
#if HX_PLATFORM_ANDROID
#include <Helix/Platform/Window/Android.h>
#endif
#include <Helix/Scripting/Host/DotnetHost.h>
#include <Helix/Scripting/Types/Type.h>

#include <bit>

namespace helix::player
{
    namespace
    {
        LogChannel ScriptsLog{"Player"};

#if HX_PLATFORM_WINDOWS
        constexpr StringView kModuleExtension = ".dll";
#else
        constexpr StringView kModuleExtension = ".so";
#endif

        // The files of a folder (none when it is not there), in name order.
        Array<String> FilesOf(StringView folder)
        {
            Array<String> files;
            platform::ListDirectory(
                folder,
                [](const platform::DirectoryEntry& e, void* user) {
                    if (!e.isDirectory)
                        static_cast<Array<String>*>(user)->Push(String(e.name));
                },
                &files);
            for (usize i = 1; i < files.Size(); i++)
                for (usize j = i; j > 0 && StringView(files[j]) < StringView(files[j - 1]); j--)
                    Swap(files[j], files[j - 1]);
            return files;
        }

        bool IsEngineAssembly(StringView file)
        {
            for (StringView prefix : kEngineAssemblyPrefixes)
                if (file.StartsWith(prefix))
                    return true;
            return false;
        }

        // The module name of a library file: its name without the extension and the "lib" of POSIX systems.
        StringView ModuleName(StringView file)
        {
            StringView stem = file.Substr(0, file.Size() - platform::GetExtension(file).Size());
            return HX_PLATFORM_POSIX && stem.StartsWith("lib") ? stem.Substr(3) : stem;
        }
    }

    void PlayerScripts::Start(StringView dataRoot)
    {
        Stop();
        LoadModules(dataRoot);
        StartManaged(dataRoot);
    }

    void PlayerScripts::LoadModules(StringView dataRoot)
    {
        String error;
        for (const BuildGameModule& m : BuildGameModules())
        {
            bool loaded = false;
            if (m.publish)
            {
                m.publish();
                loaded = engine::RegisterLinkedGameModule(m.name, error);
            }
            else
                loaded = engine::LoadGameModule(Format("lib{}.so", m.name), error);
            if (loaded)
                modules_.Push(String(m.name));
            else
                HX_LOG_ERROR(ScriptsLog, "The game module {} did not load: {}", m.name, error);
        }
        String folder = platform::JoinPath(dataRoot, kModulesFolder);
        for (const String& file : FilesOf(folder))
        {
            if (platform::GetExtension(file) != kModuleExtension)
                continue;
            if (engine::LoadGameModule(platform::JoinPath(folder, file), error))
            {
                modules_.Push(String(ModuleName(file)));
                HX_LOG_INFO(ScriptsLog, "Game module {} loaded.", ModuleName(file));
            }
            else
                HX_LOG_ERROR(ScriptsLog, "The game module {} did not load: {}", file, error);
        }
    }

    namespace
    {
        // Where the C# side comes from: the game's assemblies, the engine's, and how the runtime starts.
        struct ManagedSetup
        {
            String gameFolder;
            String engineFolder;
            String runtime;
            String nativeFolder;
            String frameworkFolder;
            // The engine's assemblies are files of engineFolder (the Web's app carries them instead).
            bool engineFiles = true;
        };

        bool Prepare(StringView dataRoot, ManagedSetup& out)
        {
#if HX_PLATFORM_ANDROID
            // The APK's assets: the runtime's framework and the engine's assemblies in Managed/, the game's in the
            // data folder's Managed/; the runtime takes files, so they are extracted to the application's files once
            // per installed version. The runtime's native libraries are the APK's.
            String files(platform::android::FilesDirectory());
            out.engineFolder = platform::JoinPath(files, kManagedFolder);
            out.gameFolder = platform::JoinPath(platform::JoinPath(files, dataRoot), kManagedFolder);
            if (!platform::android::ExtractAssets(kManagedFolder, out.engineFolder) ||
                !platform::android::ExtractAssets(platform::JoinPath(dataRoot, kManagedFolder), out.gameFolder))
            {
                HX_LOG_ERROR(ScriptsLog, "The managed assemblies could not be extracted to {}.", files);
                return false;
            }
            out.runtime = String("coreclr");
            out.nativeFolder = String(platform::android::NativeLibraryDirectory());
            out.frameworkFolder = out.engineFolder;
#else
            out.gameFolder = platform::JoinPath(dataRoot, kManagedFolder);
            out.engineFolder = out.gameFolder;
            out.engineFiles = !HX_PLATFORM_WEB;
#endif
            return true;
        }
    }

    void PlayerScripts::StartManaged(StringView dataRoot)
    {
#if HX_PLATFORM_ANDROID
        Array<String> listed;
        bool any = false;
        if (platform::android::ListAssets(platform::JoinPath(dataRoot, kManagedFolder), listed))
            for (const String& file : listed)
                any = any || (platform::GetExtension(file) == ".dll" && !IsEngineAssembly(file));
        if (!any)
            return;
#endif
        ManagedSetup setup;
        if (!Prepare(dataRoot, setup))
            return;
        Array<String> game;
        for (const String& file : FilesOf(setup.gameFolder))
            if (platform::GetExtension(file) == ".dll" && !IsEngineAssembly(file))
                game.Push(platform::JoinPath(setup.gameFolder, file));
        if (game.Empty())
            return;
        using namespace scripting;
        String runtime = platform::JoinPath(setup.engineFolder, kScriptingRuntimeAssembly);
        String core = platform::JoinPath(setup.engineFolder, kCoreModuleAssembly);
        if (setup.engineFiles && (!platform::FileExists(runtime) || !platform::FileExists(core)))
        {
            HX_LOG_ERROR(ScriptsLog, "C# scripts need {} and {} in {}.", kScriptingRuntimeAssembly, kCoreModuleAssembly, setup.engineFolder);
            return;
        }
        String error;
        DotnetHostDesc desc;
        desc.runtimeAssembly = runtime;
        desc.runtime = setup.runtime;
        desc.nativeFolder = setup.nativeFolder;
        desc.frameworkFolder = setup.frameworkFolder;
        if (!DotnetHost::Start(desc, error))
        {
            HX_LOG_ERROR(ScriptsLog, "The .NET runtime did not start: {}", error);
            return;
        }
        managed_ = true;
        // The engine's types, which register on their first lookup, before the C# side takes the engine's bindings.
        (void)FindType("HelixEngine.Object");
        AssemblyHandle assembly = DotnetHost::LoadAssembly(core, error);
        i32 (*start)() = nullptr;
        i32 (*load)(const char* path, i32 size) = nullptr;
        if (assembly.IsValid())
        {
            start = std::bit_cast<i32 (*)()>(DotnetHost::GetFunction(assembly, "HelixEngine.Internal.ScriptBridge", "Start", error));
            load = std::bit_cast<i32 (*)(const char*, i32)>(DotnetHost::GetFunction(assembly, "HelixEngine.Internal.ScriptBridge", "LoadScripts", error));
        }
        if (!start || !load || start() != 0)
        {
            HX_LOG_ERROR(ScriptsLog, "The engine's C# side did not start: {}", error);
            return;
        }
        for (const String& path : game)
        {
            i32 classes = load(path.Data(), i32(path.Size()));
            if (classes < 0)
                HX_LOG_ERROR(ScriptsLog, "The C# assembly {} did not load.", platform::GetFileName(path));
            else
            {
                assemblies_.Push(String(platform::GetFileName(path)));
                HX_LOG_INFO(ScriptsLog, "C# assembly {} loaded: {} script classes.", platform::GetFileName(path), classes);
            }
        }
    }

    void PlayerScripts::Stop()
    {
        for (usize i = modules_.Size(); i-- > 0;)
            engine::UnloadGameModule(modules_[i]);
        modules_.Clear();
        assemblies_.Clear();
        if (managed_)
            scripting::DotnetHost::Stop();
        managed_ = false;
    }
}
