#pragma once

#include <Helix/Core/Basics/Span.h>
#include <Helix/Core/Containers/Array.h>
#include <Helix/Core/Strings/String.h>

// The player's command line: the reference player's arguments that apply to Helix, and the development arguments
// (-dataFolder, -scene, -frames, -screenshot). Arguments the player does not know are kept for scripts.
namespace helix::player
{
    enum class GraphicsApi : u8
    {
        // D3D11 on Windows, OpenGL on Linux, OpenGL ES on Android, WebGL 2 on the Web.
        Default,
        D3D11,
        OpenGL,
    };

    enum class WindowModeArgument : u8
    {
        Default,
        Exclusive,
        Borderless,
    };

    struct PlayerOptions
    {
        // -batchmode: no window and no input; frames are drawn into a texture. -nographics: no graphics device either.
        bool batchMode = false;
        bool noGraphics = false;
        // -screen-width, -screen-height, -screen-fullscreen 0|1 (-1 when not given), -window-mode, -popupwindow,
        // -monitor N (1 based, 0 when not given).
        i32 screenWidth = 0;
        i32 screenHeight = 0;
        i32 screenFullscreen = -1;
        WindowModeArgument windowMode = WindowModeArgument::Default;
        bool popupWindow = false;
        i32 monitor = 0;
        // -logFile <path> ("-" is standard output), -nolog, -timestamps.
        String logFile;
        bool logFileGiven = false;
        bool noLog = false;
        bool timestamps = false;
        // -force-d3d11, -force-glcore[XY] (glVersion XY, 0 when not given), -force-clamped, -force-device-index N,
        // -force-driver-type-warp; -force-gfx-direct (no render thread), -force-gfx-mt (a render thread, the default
        // where there are threads).
        GraphicsApi graphicsApi = GraphicsApi::Default;
        i32 glVersion = 0;
        bool forceClamped = false;
        i32 deviceIndex = -1;
        bool warp = false;
        bool renderThread = true;
        // -single-instance, -parentHWND <HWND> [delayed], -wait-for-native-debugger.
        bool singleInstance = false;
        u64 parentWindow = 0;
        bool parentDelayed = false;
        bool waitForNativeDebugger = false;
        // Development: -dataFolder <path>, -scene <path>, -frames <n> (quit after n frames), -screenshot <png> (of the
        // last frame).
        String dataFolder;
        String scene;
        i32 frames = 0;
        String screenshot;
        // Arguments the player does not take.
        Array<String> other = Array<String>();
    };

    // The arguments after the executable's path; false with errors for arguments of the player given wrong values.
    bool ParseCommandLine(Span<const StringView> args, PlayerOptions& out, Array<String>& errors);
}
