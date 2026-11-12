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
| `Source/Helix/Player/Platform/<Os>/PlayerMain*.cpp` | The entry points: `wWinMain` (Windows), `main` (Linux, Web), `HelixMain` with the APK's assets as the data folder (Android) |
| `Web/Templates/Default.html` | The page: the canvas and the data folder the loader fetches (`Module.helixPreload`) |
| `Tests/` | Command line, data folder and player runs on every device (in process and as the executable) against `Tests/Golden` |

## Data folder

A data folder holds the files of the game and two more:

- `assets.index`: one line per file, its guid (32 hex digits), a space, its path. Scene files (`.helix`) are listed
  too, so scenes load by `LoadableSceneId`.
- `settings.asset`: the settings objects as YAML documents (PlayerSettings, TimeManager, TagManager, BuildSettings
  with the scene list). The first build scene starts.

It is found next to the executable as `<executable name>_Data` (`HelixPlayer_Data`), as `Data/` next to the page on
the Web (or `?data=<URL>`), and in the APK's assets as `Data/` on Android. Configure with
`-DHELIX_PLAYER_DATA=<folder>` to copy one there with the build.

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
on the Web from the address (`HelixPlayer.html?args=-frames%20100`).

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
the data folder under its assets' `Data/`); on the Web `helix_add_web_app` makes the page from the template, and the
loader fetches the data folder before `main`.

## Build

```
git clone https://github.com/kisstp2006/Helix.Player
git -C Helix.Player submodule update --init
cmake --preset windows-debug
cmake --build --preset windows-debug
ctest --preset windows-debug
```

The .NET SDK builds the engine's C# shaders (set `HELIX_DOTNET_ROOT` or have `dotnet` on the path). Linux needs
`libx11-dev`. The APK is built when `ANDROID_HOME` has build tools and a platform and a JDK is found (`JAVA_HOME`).

## License

GPL-3.0, see `LICENSE`.
