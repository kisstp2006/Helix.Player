#include <Helix/Player/Host/Player.h>

#include <windows.h>
#include <shellapi.h>

// The Windows player: a windows subsystem executable; the command line as UTF-8.
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    helix::Array<helix::String> args;
    for (int i = 1; i < count; i++)
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        helix::Array<char> utf8;
        utf8.Resize(helix::usize(n > 0 ? n : 1));
        WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, utf8.Data(), n, nullptr, nullptr);
        args.Push(helix::String(helix::StringView(utf8.Data(), helix::usize(n > 0 ? n - 1 : 0))));
    }
    LocalFree(wide);
    helix::Array<helix::StringView> views;
    for (const helix::String& a : args)
        views.Push(a);
    return helix::player::RunPlayer(helix::Span<const helix::StringView>(views.Data(), views.Size()));
}
