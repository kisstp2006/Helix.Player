#include <Helix/Player/Host/Implementation/HostPlatform.h>

#if !HX_PLATFORM_ANDROID

#include <Helix/Core/Strings/Format.h>
#include <Helix/Platform/IO/FileSystem.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace helix::player::host
{
    namespace
    {
        int instanceLock = -1;
    }

#if HX_PLATFORM_LINUX
    // A lock file in the temporary folder: held while the instance runs, released by the system when it exits.
    bool AcquireSingleInstance(StringView product)
    {
        String name("helix-player-");
        for (usize i = 0; i < product.Size(); i++)
            name.Append(product[i] == '/' ? '_' : product[i]);
        name.Append(".lock");
        String path = platform::JoinPath(platform::TempDirectory(), name);
        instanceLock = open(path.CStr(), O_CREAT | O_RDWR, 0600);
        if (instanceLock < 0)
            return true;
        if (flock(instanceLock, LOCK_EX | LOCK_NB) != 0)
        {
            close(instanceLock);
            instanceLock = -1;
            return false;
        }
        return true;
    }

    void WaitForNativeDebugger()
    {
        std::fprintf(stderr, "Attach a native debugger to process %d.\n", int(getpid()));
        for (;;)
        {
            // TracerPid in /proc/self/status is not zero once a debugger is attached.
            Array<u8> status;
            if (!platform::ReadFile("/proc/self/status", status))
                return;
            status.Push(0);
            const char* tracer = std::strstr(reinterpret_cast<const char*>(status.Data()), "TracerPid:");
            if (tracer && std::atoi(tracer + 10) != 0)
                return;
            usleep(100000);
        }
    }
#else
    // The Web runs one instance by nature.
    bool AcquireSingleInstance(StringView) { return true; }
    void WaitForNativeDebugger() {}
#endif

    void ReleaseSingleInstance()
    {
        if (instanceLock >= 0)
        {
            flock(instanceLock, LOCK_UN);
            close(instanceLock);
            instanceLock = -1;
        }
    }

    void SetKeepScreenOn(bool) {}
    void SetOrientation(i32) {}
    void AcknowledgeWindow(void*) {}
}
#endif
