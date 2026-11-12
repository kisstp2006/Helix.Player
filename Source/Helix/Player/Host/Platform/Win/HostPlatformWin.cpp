#include <Helix/Player/Host/Implementation/HostPlatform.h>

#include <Helix/Core/Strings/Format.h>

#include <windows.h>

namespace helix::player::host
{
    namespace
    {
        HANDLE instanceMutex = nullptr;
    }

    bool AcquireSingleInstance(StringView product)
    {
        String name = Format("Local\\Helix.Player.{}", product);
        wchar_t wide[300];
        int n = MultiByteToWideChar(CP_UTF8, 0, name.Data(), int(name.Size()), wide, 299);
        wide[n > 0 ? n : 0] = 0;
        // Backslashes other than the namespace's are not allowed in object names.
        for (int i = 6; i < n; i++)
            if (wide[i] == L'\\')
                wide[i] = L'_';
        instanceMutex = CreateMutexW(nullptr, TRUE, wide);
        if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(instanceMutex);
            instanceMutex = nullptr;
            // The other instance's window comes to the front.
            if (HWND other = FindWindowW(L"HelixWindow", nullptr))
            {
                ShowWindow(other, SW_RESTORE);
                SetForegroundWindow(other);
            }
            return false;
        }
        return true;
    }

    void ReleaseSingleInstance()
    {
        if (instanceMutex)
        {
            ReleaseMutex(instanceMutex);
            CloseHandle(instanceMutex);
            instanceMutex = nullptr;
        }
    }

    void WaitForNativeDebugger()
    {
        if (IsDebuggerPresent())
            return;
        wchar_t text[128];
        wsprintfW(text, L"Attach a native debugger to process %lu, then press OK.", GetCurrentProcessId());
        MessageBoxW(nullptr, text, L"Helix Player", MB_OK | MB_ICONINFORMATION);
        while (!IsDebuggerPresent())
            Sleep(100);
    }

    void SetKeepScreenOn(bool keepOn) { SetThreadExecutionState(keepOn ? ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED : ES_CONTINUOUS); }

    void SetOrientation(i32) {}

    void AcknowledgeWindow(void*) {}
}
