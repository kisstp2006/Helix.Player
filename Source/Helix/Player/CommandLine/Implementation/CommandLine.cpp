#include <Helix/Player/CommandLine/CommandLine.h>

#include <Helix/Core/Strings/Format.h>

namespace helix::player
{
    namespace
    {
        bool SameText(StringView a, StringView b)
        {
            if (a.Size() != b.Size())
                return false;
            for (usize i = 0; i < a.Size(); i++)
            {
                char x = a[i], y = b[i];
                x = x >= 'A' && x <= 'Z' ? char(x - 'A' + 'a') : x;
                y = y >= 'A' && y <= 'Z' ? char(y - 'A' + 'a') : y;
                if (x != y)
                    return false;
            }
            return true;
        }

        bool ParseInt(StringView s, i64& out)
        {
            if (s.Empty())
                return false;
            usize i = 0;
            bool negative = false;
            if (s[0] == '-' || s[0] == '+')
            {
                negative = s[0] == '-';
                i = 1;
            }
            i32 base = 10;
            if (s.Size() > i + 2 && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X'))
            {
                base = 16;
                i += 2;
            }
            if (i >= s.Size())
                return false;
            u64 v = 0;
            for (; i < s.Size(); i++)
            {
                char c = s[i];
                u32 d;
                if (c >= '0' && c <= '9')
                    d = u32(c - '0');
                else if (base == 16 && c >= 'a' && c <= 'f')
                    d = u32(c - 'a' + 10);
                else if (base == 16 && c >= 'A' && c <= 'F')
                    d = u32(c - 'A' + 10);
                else
                    return false;
                v = v * u64(base) + d;
            }
            out = negative ? -i64(v) : i64(v);
            return true;
        }

        // The arguments by name: what each takes and where it goes. One table, so a new argument is one row.
        struct Cursor
        {
            Span<const StringView> args;
            usize at;
            Array<String>& errors;

            bool Value(StringView name, StringView& out)
            {
                if (at + 1 >= args.Size())
                {
                    errors.Push(Format("{} needs a value", name));
                    return false;
                }
                out = args[++at];
                return true;
            }

            bool Int(StringView name, i64 min, i64 max, i64& out)
            {
                StringView v;
                if (!Value(name, v))
                    return false;
                if (!ParseInt(v, out) || out < min || out > max)
                {
                    errors.Push(Format("{} takes a number from {} to {}, not '{}'", name, min, max, v));
                    return false;
                }
                return true;
            }
        };

        using Handler = void (*)(Cursor& c, StringView name, PlayerOptions& o);

        struct Argument
        {
            StringView name;
            Handler handler;
        };

        void IntInto(Cursor& c, StringView name, i64 min, i64 max, i32& field)
        {
            i64 v = 0;
            if (c.Int(name, min, max, v))
                field = i32(v);
        }

        void StringInto(Cursor& c, StringView name, String& field)
        {
            StringView v;
            if (c.Value(name, v))
                field = String(v);
        }

        const Argument kArguments[] = {
            {"-batchmode", [](Cursor&, StringView, PlayerOptions& o) { o.batchMode = true; }},
            {"-nographics", [](Cursor&, StringView, PlayerOptions& o) { o.noGraphics = true; }},
            {"-screen-width", [](Cursor& c, StringView n, PlayerOptions& o) { IntInto(c, n, 1, 65536, o.screenWidth); }},
            {"-screen-height", [](Cursor& c, StringView n, PlayerOptions& o) { IntInto(c, n, 1, 65536, o.screenHeight); }},
            {"-screen-fullscreen", [](Cursor& c, StringView n, PlayerOptions& o) { IntInto(c, n, 0, 1, o.screenFullscreen); }},
            {"-window-mode",
             [](Cursor& c, StringView n, PlayerOptions& o) {
                 StringView v;
                 if (!c.Value(n, v))
                     return;
                 if (SameText(v, "exclusive"))
                     o.windowMode = WindowModeArgument::Exclusive;
                 else if (SameText(v, "borderless"))
                     o.windowMode = WindowModeArgument::Borderless;
                 else
                     c.errors.Push(Format("-window-mode takes exclusive or borderless, not '{}'", v));
             }},
            {"-popupwindow", [](Cursor&, StringView, PlayerOptions& o) { o.popupWindow = true; }},
            {"-monitor", [](Cursor& c, StringView n, PlayerOptions& o) { IntInto(c, n, 1, 64, o.monitor); }},
            {"-logFile",
             [](Cursor& c, StringView n, PlayerOptions& o) {
                 o.logFileGiven = true;
                 // The value is optional: a following argument that starts with '-' (other than "-" itself) is the
                 // next argument, and the log goes to the default file.
                 if (c.at + 1 < c.args.Size() && (c.args[c.at + 1] == "-" || !c.args[c.at + 1].StartsWith("-")))
                     StringInto(c, n, o.logFile);
             }},
            {"-nolog", [](Cursor&, StringView, PlayerOptions& o) { o.noLog = true; }},
            {"-timestamps", [](Cursor&, StringView, PlayerOptions& o) { o.timestamps = true; }},
            {"-force-d3d11", [](Cursor&, StringView, PlayerOptions& o) { o.graphicsApi = GraphicsApi::D3D11; }},
            {"-force-clamped", [](Cursor&, StringView, PlayerOptions& o) { o.forceClamped = true; }},
            {"-force-device-index", [](Cursor& c, StringView n, PlayerOptions& o) { IntInto(c, n, 0, 64, o.deviceIndex); }},
            {"-force-driver-type-warp", [](Cursor&, StringView, PlayerOptions& o) { o.warp = true; }},
            {"-single-instance", [](Cursor&, StringView, PlayerOptions& o) { o.singleInstance = true; }},
            {"-parentHWND",
             [](Cursor& c, StringView n, PlayerOptions& o) {
                 i64 v = 0;
                 if (!c.Int(n, 1, i64(0x7fffffffffffffff), v))
                     return;
                 o.parentWindow = u64(v);
                 if (c.at + 1 < c.args.Size() && SameText(c.args[c.at + 1], "delayed"))
                 {
                     o.parentDelayed = true;
                     c.at++;
                 }
             }},
            {"-wait-for-native-debugger", [](Cursor&, StringView, PlayerOptions& o) { o.waitForNativeDebugger = true; }},
            {"-dataFolder", [](Cursor& c, StringView n, PlayerOptions& o) { StringInto(c, n, o.dataFolder); }},
            {"-scene", [](Cursor& c, StringView n, PlayerOptions& o) { StringInto(c, n, o.scene); }},
            {"-frames", [](Cursor& c, StringView n, PlayerOptions& o) { IntInto(c, n, 1, 0x7fffffff, o.frames); }},
            {"-screenshot", [](Cursor& c, StringView n, PlayerOptions& o) { StringInto(c, n, o.screenshot); }},
        };

        // -force-glcore and -force-glcoreXY with XY one of the core versions.
        bool ForceGl(StringView arg, PlayerOptions& o, Array<String>& errors)
        {
            constexpr StringView prefix = "-force-glcore";
            if (!arg.StartsWith(prefix))
                return false;
            StringView version = arg.Substr(prefix.Size());
            o.graphicsApi = GraphicsApi::OpenGL;
            if (version.Empty())
                return true;
            i64 v = 0;
            constexpr i32 kVersions[] = {32, 33, 40, 41, 42, 43, 44, 45};
            bool known = false;
            if (ParseInt(version, v))
                for (i32 k : kVersions)
                    known = known || v == k;
            if (!known)
                errors.Push(Format("{}: the version is one of 32, 33, 40, 41, 42, 43, 44, 45", arg));
            else
                o.glVersion = i32(v);
            return true;
        }
    }

    bool ParseCommandLine(Span<const StringView> args, PlayerOptions& out, Array<String>& errors)
    {
        usize before = errors.Size();
        Cursor c{args, 0, errors};
        for (; c.at < args.Size(); c.at++)
        {
            StringView arg = args[c.at];
            const Argument* found = nullptr;
            for (const Argument& a : kArguments)
                if (SameText(a.name, arg))
                    found = &a;
            if (found)
                found->handler(c, found->name, out);
            else if (!ForceGl(arg, out, errors))
                out.other.Push(String(arg));
        }
        return errors.Size() == before;
    }
}
