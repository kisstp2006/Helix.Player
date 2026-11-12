#include <Helix/Player/Host/Player.h>

#include <Helix/Core/Strings/Format.h>
#include <Helix/Platform/Window/Android.h>

// The Android player: libHelixPlayer.so, started by the activity of Helix.Platform; the data folder is read from the
// APK's assets.
namespace
{
    using namespace helix;

    class ApkReader final : public engine::DataFileReader
    {
    public:
        bool Read(StringView root, StringView path, Array<u8>& out) override
        {
            return platform::android::ReadAsset(root.Empty() ? String(path) : Format("{}/{}", root, path), out);
        }
    };
}

int HelixMain(int argc, char** argv)
{
    static ApkReader reader;
    Array<StringView> args;
    for (int i = 1; i < argc; i++)
        args.Push(StringView(argv[i]));
    return player::RunPlayer(Span<const StringView>(args.Data(), args.Size()), &reader);
}
