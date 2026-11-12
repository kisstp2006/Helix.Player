#include <Helix/Player/CommandLine/CommandLine.h>
#include <Helix/Testing/Test.h>

using namespace helix;
using namespace helix::player;

namespace
{
    bool Parse(std::initializer_list<StringView> list, PlayerOptions& out, Array<String>& errors)
    {
        Array<StringView> args;
        for (StringView a : list)
            args.Push(a);
        return ParseCommandLine(Span<const StringView>(args.Data(), args.Size()), out, errors);
    }
}

HX_TEST(CommandLine, ReferenceArguments)
{
    PlayerOptions o;
    Array<String> errors;
    HX_REQUIRE(Parse({"-batchmode", "-nographics", "-screen-width", "800", "-screen-height", "600", "-screen-fullscreen", "1", "-window-mode",
                      "Exclusive", "-popupwindow", "-monitor", "2", "-logFile", "out.log", "-timestamps", "-force-glcore45", "-force-clamped",
                      "-force-device-index", "1", "-force-driver-type-warp", "-single-instance", "-parentHWND", "0x1a2b", "delayed",
                      "-wait-for-native-debugger"},
                     o, errors));
    HX_CHECK(errors.Empty());
    HX_CHECK(o.batchMode && o.noGraphics && o.screenWidth == 800 && o.screenHeight == 600 && o.screenFullscreen == 1);
    HX_CHECK(o.windowMode == WindowModeArgument::Exclusive && o.popupWindow && o.monitor == 2);
    HX_CHECK(o.logFileGiven && o.logFile == StringView("out.log") && o.timestamps && !o.noLog);
    HX_CHECK(o.graphicsApi == GraphicsApi::OpenGL && o.glVersion == 45 && o.forceClamped && o.deviceIndex == 1 && o.warp);
    HX_CHECK(o.singleInstance && o.parentWindow == 0x1a2b && o.parentDelayed && o.waitForNativeDebugger);
}

HX_TEST(CommandLine, DevelopmentArgumentsAndOthers)
{
    PlayerOptions o;
    Array<String> errors;
    HX_REQUIRE(Parse({"-dataFolder", "C:/Game_Data", "-scene", "Scenes/Level.helix", "-frames", "3", "-screenshot", "shot.png", "-myFlag",
                      "value", "-force-d3d11", "-logFile", "-nolog"},
                     o, errors));
    HX_CHECK(o.dataFolder == StringView("C:/Game_Data") && o.scene == StringView("Scenes/Level.helix") && o.frames == 3);
    HX_CHECK(o.screenshot == StringView("shot.png") && o.graphicsApi == GraphicsApi::D3D11);
    // Arguments of scripts stay in order; -logFile without a value means the default file.
    HX_CHECK(o.other.Size() == 2 && o.other[0] == StringView("-myFlag") && o.other[1] == StringView("value"));
    HX_CHECK(o.logFileGiven && o.logFile.Empty() && o.noLog);
    PlayerOptions console;
    HX_REQUIRE(Parse({"-logFile", "-"}, console, errors));
    HX_CHECK(console.logFile == StringView("-"));
}

HX_TEST(CommandLine, WrongValuesAreErrors)
{
    PlayerOptions o;
    Array<String> errors;
    HX_CHECK(!Parse({"-screen-width", "wide", "-screen-fullscreen", "2", "-window-mode", "tiled", "-force-glcore39", "-frames"}, o, errors));
    HX_CHECK(errors.Size() == 5);
    HX_CHECK(o.screenWidth == 0 && o.screenFullscreen == -1 && o.graphicsApi == GraphicsApi::OpenGL && o.glVersion == 0);
}
