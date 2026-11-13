# Helix.Player

The player of the Helix engine: runs a data folder (or one scene file) on Windows, Linux, Android and the Web. It
opens the window, creates the graphics device, reads the data folder's assets and settings, starts the first build
scene and runs every frame through the engine's player loop.

## Contents

| Path | Content |
|---|---|
| `Source/Helix/Player/CommandLine/CommandLine.h` | `PlayerOptions`, `ParseCommandLine`: the player's arguments |
| `Source/Helix/Player/Data/DataFolder.h` | `DataFolder` (index, settings, scenes), `DefaultDataFolder`, `PlayerSceneSource` |
| `Source/Helix/Player/Host/Player.h` | `Player` (`Start`, `Frame`, `Shutdown`), `RunPlayer` |
| `Source/Helix/Player/Log/PlayerLog.h` | `PlayerLog`: the log file or standard output, with optional timestamps |
| `Source/Helix/Player/Scripts/PlayerScripts.h` | `PlayerScripts`: the data folder's C++ game modules and C# assemblies, `BuildGameModules` |
| `Source/Helix/Player/Platform/<Os>/PlayerMain*.cpp` | The entry points: `wWinMain` (Windows), `main` (Linux, Web), `HelixMain` with the APK's assets as the data folder (Android) |
| `Web/Templates/` | The page (`Default.html`, published as `index.html`) and its `main.js`: the data folder fetched by the loader of Helix.Platform, then the .NET runtime started |
| `Android/CheckApk.cmake` | The packaged APK checked for the .NET runtime (its native libraries, the framework and the engine's assemblies) |
| `Tests/` | Command line, data folder and player runs on every device (in process and as the executable) against `Tests/Golden`; a data folder with a C# and a C++ script (`Tests/Scripts`) |

## Data folder

A data folder holds the files of the game and two more:

- `assets.index`: one line per file, its guid (32 hex digits), a space, its path. Scene files (`.helix`) are listed
  too, so scenes load by `LoadableSceneId`.
- `settings.asset`: the settings objects as YAML documents (PlayerSettings, TimeManager, TagManager, BuildSettings
  with the scene list). The first build scene starts.

It is found next to the executable as `<executable name>_Data` (`HelixPlayer_Data`), as `Data/` next to the page on
the Web (or `?data=<URL>`), and in the APK's assets as `Data/` on Android. Configure with
`-DHELIX_PLAYER_DATA=<folder>` to copy one there with the build (with the engine's assemblies in its `Managed/`).

## Scripts

Scripts load before the first scene, so its GameBehaviours find their classes:

- `Modules/`: C++ game modules, shared libraries against the shared engine (`HelixEngine.dll` / `libHelixEngine.so`
  next to the player, which the player links too), loaded on Windows and Linux. On Android and the Web the build
  names the modules (`-DHELIX_PLAYER_GAME_MODULES=<targets>`): the APK carries them with the engine and the C++
  runtime, the Web links them into the player.
- `Managed/`: the engine's assemblies (`HelixEngine.CoreModule.dll`, `Helix.Scripting.Runtime.dll` with its
  runtimeconfig) and the game's. A game assembly starts the .NET runtime (`dotnet/` next to the player, else
  `DOTNET_ROOT`, `HELIX_DOTNET_ROOT` or the installed one), the engine's C# side and the assembly's scripts. Release
  builds on Windows and Linux put ReadyToRun images of the engine's assemblies there (`HELIX_PLAYER_READY_TO_RUN`,
  on by default). The APK carries the android-arm64 runtime (its native libraries next to the player, the framework
  and the engine's assemblies as the assets' `Managed/`), extracted to the application's files once per installed
  version and started through `coreclr_initialize`. The Web player is a .NET web app with the engine's assemblies
  (and `-DHELIX_PLAYER_GAME_ASSEMBLIES=<dlls>`) built in; a data folder's own assemblies are listed in
  `Managed/assemblies.index` for the loader and loaded at run time.

Shutting down destroys the scripts' objects, unloads the modules and stops the C# side.

## Command line

The reference player's arguments that apply:

| Argument | Meaning |
|---|---|
| `-batchmode` | No window and no input; frames are drawn into a texture |
| `-nographics` | With `-batchmode`: no graphics device either |
| `-screen-width N`, `-screen-height N` | The window's (or the batch mode texture's) size |
| `-screen-fullscreen 0\|1`, `-window-mode exclusive\|borderless` | Full screen, borderless or exclusive (Windows) |
| `-popupwindow` | A window without a frame |
| `-monitor N` | The display to open on (1 based) |
| `-logFile <path>` | The log file (`-` for standard output); default `Player.log` in the persistent data path |
| `-nolog`, `-timestamps` | No log; time and thread id before every line |
| `-force-d3d11`, `-force-glcore[XY]` | The graphics API (Direct3D 11 is the default on Windows, OpenGL elsewhere) |
| `-force-device-index N`, `-force-driver-type-warp` | The GPU by index; the WARP software rasterizer |
| `-single-instance` | One instance; a second one brings the first to the front |
| `-parentHWND <HWND> [delayed]` | Embedded into another window (Windows) |
| `-wait-for-native-debugger` | Waits until a native debugger is attached |

Development arguments of Helix: `-dataFolder <path>`, `-scene <path>` (a scene file, from the catalog or disk),
`-frames N` (quit after N frames) and `-screenshot <png>` (of the last frame). Other arguments are left for scripts.
On Android the arguments come from the intent (`am start -n org.helix.player/org.helix.platform.HelixActivity -e args "-frames 100"`),
on the Web from the address (`index.html?args=-frames%20100`).

## The frame

Events go to the engine (quit, focus); the size of the window sets the default render target (the swap chain with its
depth buffer, or the batch mode textures); the Screen gets the size, mode, displays and window position; then
`Application::RunFrame` runs the player loop, which draws in `FinishFrameRendering`, and the frame is presented.
What scripts asked of the Screen (resolution, full screen mode, window moves, sleep timeout, orientation) is applied
at the end of the frame. Without focus the frames pause unless `runInBackground` is set (PlayerSettings, on by
default). On Android the graphics device follows the activity's surface: it goes when the surface goes and comes
back with the next one.

## Platforms

What belongs to the system comes from Helix.Platform: one instance per product, waiting for a debugger, keeping the
screen on, the orientation, and the applications of Android and the Web. On Android `helix_add_android_app` makes
libHelixPlayer.so an application (its activity runs `HelixMain`, the APK is built with the SDK's tools and carries
the data folder under its assets' `Data/`); on the Web the player is linked into the .NET runtime's module
(Helix.Scripting's web apps), published with the page in `helix_player.Web/publish/wwwroot`, and the loader fetches the
data folder before `main`; its frames run on the browser's loop after `main` returns (batch mode without graphics runs them in
`main`, so a run in node ends with the player's exit code).

## Build

```
git clone https://github.com/kisstp2006/Helix.Player
git -C Helix.Player submodule update --init
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The .NET SDK builds the engine's C# shaders and assemblies (set `HELIX_DOTNET_ROOT` or have `dotnet` on the path). Linux needs
`libx11-dev`. The APK is built when `ANDROID_HOME` has build tools and a platform and a JDK is found (`JAVA_HOME`).

## License

GPL-3.0, see `LICENSE`.
