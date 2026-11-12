#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>

// The scripts of a data folder: the C# assemblies of Managed/ (the engine's and the game's: a game assembly starts the
// .NET host and the engine's C# side) and the C++ game modules of Modules/ (shared libraries on Windows and Linux; on
// Android and the Web the build names the modules: packaged into the APK, or linked into the player).
namespace helix::player
{
    inline constexpr StringView kManagedFolder = "Managed";
    inline constexpr StringView kModulesFolder = "Modules";
    // The engine's own assemblies in Managed/ (the rest are the game's).
    inline constexpr StringView kEngineAssemblyPrefixes[] = {"HelixEngine.", "Helix."};
    inline constexpr StringView kScriptingRuntimeAssembly = "Helix.Scripting.Runtime.dll";
    inline constexpr StringView kCoreModuleAssembly = "HelixEngine.CoreModule.dll";

    // A game module the build gives the player: its name, and for one linked in (the Web) the function that publishes
    // its exports by name.
    struct BuildGameModule
    {
        StringView name;
        void (*publish)() = nullptr;
    };
    // The build's game modules (written by the player's build).
    Span<const BuildGameModule> BuildGameModules();

    class PlayerScripts
    {
    public:
        PlayerScripts() = default;
        ~PlayerScripts() { Stop(); }
        PlayerScripts(const PlayerScripts&) = delete;
        PlayerScripts& operator=(const PlayerScripts&) = delete;

        // Loads the scripts of the data folder: the modules, then the C# side. A script that does not load is logged;
        // the player runs on without it.
        void Start(StringView dataRoot);
        // Unloads the modules and lets go of the C# side (once their objects are gone).
        void Stop();

        bool ManagedRunning() const { return managed_; }
        Span<const String> Modules() const { return modules_; }
        Span<const String> Assemblies() const { return assemblies_; }

    private:
        void LoadModules(StringView dataRoot);
        void StartManaged(StringView dataRoot);

        Array<String> modules_;
        Array<String> assemblies_;
        bool managed_ = false;
    };
}
