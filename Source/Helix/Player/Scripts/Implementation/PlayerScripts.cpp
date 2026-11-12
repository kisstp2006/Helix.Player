#include <Helix/Player/Scripts/PlayerScripts.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Engine/Script/ScriptRuntime.h>
#include <Helix/Platform/IO/FileSystem.h>
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

    void PlayerScripts::StartManaged(StringView dataRoot)
    {
        String folder = platform::JoinPath(dataRoot, kManagedFolder);
        Array<String> game;
        for (const String& file : FilesOf(folder))
            if (platform::GetExtension(file) == ".dll" && !IsEngineAssembly(file))
                game.Push(platform::JoinPath(folder, file));
        if (game.Empty())
            return;
#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
        using namespace scripting;
        String runtime = platform::JoinPath(folder, kScriptingRuntimeAssembly);
        String core = platform::JoinPath(folder, kCoreModuleAssembly);
        if (!platform::FileExists(runtime) || !platform::FileExists(core))
        {
            HX_LOG_ERROR(ScriptsLog, "C# scripts need {} and {} in {}.", kScriptingRuntimeAssembly, kCoreModuleAssembly, folder);
            return;
        }
        String error;
        DotnetHostDesc desc;
        desc.runtimeAssembly = runtime;
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
#else
        HX_LOG_WARNING(ScriptsLog, "{} C# assemblies in {}: C# scripts wait for this platform's .NET host.", game.Size(), folder);
#endif
    }

    void PlayerScripts::Stop()
    {
        for (usize i = modules_.Size(); i-- > 0;)
            engine::UnloadGameModule(modules_[i]);
        modules_.Clear();
        assemblies_.Clear();
#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
        if (managed_)
            scripting::DotnetHost::Stop();
#endif
        managed_ = false;
    }
}
