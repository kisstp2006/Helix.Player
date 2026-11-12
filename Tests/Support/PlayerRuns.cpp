#include <Support/PlayerRuns.h>

#if HX_PLATFORM_WINDOWS
#include <windows.h>
#endif

namespace helix::player::testing
{
#if defined(HELIX_PLAYER_EXECUTABLE)
    // Runs the player executable with the arguments and waits for it; its exit code, -1 when it did not start.
    i32 RunProcess(Span<const StringView> args)
    {
#if HX_PLATFORM_WINDOWS
        String line = Format("\"{}\"", HELIX_PLAYER_EXECUTABLE);
        for (StringView a : args)
            line.Append(Format(" \"{}\"", a));
        wchar_t wide[4096];
        int n = MultiByteToWideChar(CP_UTF8, 0, line.Data(), int(line.Size()), wide, 4095);
        wide[n > 0 ? n : 0] = 0;
        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (!CreateProcessW(nullptr, wide, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
            return -1;
        WaitForSingleObject(pi.hProcess, 120000);
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return i32(code);
#else
        String line = Format("'{}'", HELIX_PLAYER_EXECUTABLE);
        for (StringView a : args)
            line.Append(Format(" '{}'", a));
        int status = std::system(line.CStr());
        return status < 0 ? -1 : (status >> 8) & 0xff;
#endif
    }
#endif
}
