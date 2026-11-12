#pragma once

#include <Helix/Core/Strings/Format.h>
#include <Helix/Image/Codecs/Png.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Player/Host/Player.h>
#include <Helix/RHI/Device/Device.h>

#include <cstdio>
#include <cstdlib>

// Runs of the player in tests: the devices of this machine as the command line chooses them, a run in this process,
// a run of the executable as a process of its own.
namespace helix::player::testing
{

    // The devices of this machine as the command line chooses them: every D3D11 adapter, OpenGL.
    struct DeviceArguments
    {
        const char* name;
        Array<String> args;
    };

#if HX_PLATFORM_WINDOWS || HX_PLATFORM_LINUX
    // A device of the backend and adapter can be made here (CI machines list OpenGL without a usable driver).
    inline bool CanCreate(rhi::Backend backend, i32 adapter, bool warp = false)
    {
        rhi::Device* d = rhi::Device::Create({.backend = backend, .adapter = adapter, .warp = warp, .vsync = false});
        if (!d)
        {
            std::printf("    no %s device here, skipped\n", backend == rhi::Backend::D3D11 ? "D3D11" : "OpenGL");
            return false;
        }
        rhi::Device::Destroy(d);
        return true;
    }
#endif

    inline Array<DeviceArguments> Devices()
    {
        Array<DeviceArguments> out;
#if HX_PLATFORM_WINDOWS
        char name[128];
        for (u32 i = 0; rhi::Device::AdapterName(rhi::Backend::D3D11, i, name, sizeof(name)); i++)
        {
            if (!CanCreate(rhi::Backend::D3D11, i32(i)))
                continue;
            DeviceArguments d{"D3D11", Array<String>()};
            d.args.Push(String("-force-d3d11"));
            d.args.Push(String("-force-device-index"));
            d.args.Push(Format("{}", i));
            out.Push(Move(d));
        }
        if (out.Empty() && CanCreate(rhi::Backend::D3D11, -1, true))
        {
            DeviceArguments d{"D3D11 WARP", Array<String>()};
            d.args.Push(String("-force-d3d11"));
            d.args.Push(String("-force-driver-type-warp"));
            out.Push(Move(d));
        }
        if (rhi::Device::IsBackendAvailable(rhi::Backend::OpenGL) && CanCreate(rhi::Backend::OpenGL, -1))
        {
            DeviceArguments d{"OpenGL", Array<String>()};
            d.args.Push(String("-force-glcore"));
            out.Push(Move(d));
        }
#elif HX_PLATFORM_LINUX
        if (rhi::Device::IsBackendAvailable(rhi::Backend::OpenGL) && CanCreate(rhi::Backend::OpenGL, -1))
            out.Push(DeviceArguments{"OpenGL", Array<String>()});
#endif
        return out;
    }

    // A run of the player in this process: the arguments, frames until it quits; the exit code, -1 when it did not start.
    inline i32 RunFrames(Span<const StringView> args, u64& frames, u32 limit = 1000)
    {
        PlayerOptions options;
        Array<String> errors;
        if (!ParseCommandLine(args, options, errors))
            return -2;
        Player player;
        if (!player.Start(options))
            return -1;
        for (u32 i = 0; i < limit && player.Frame(); i++)
        {
        }
        frames = player.FrameCount();
        i32 code = player.Running() ? -3 : player.ExitCode();
        player.Shutdown();
        return code;
    }

#if defined(HELIX_PLAYER_EXECUTABLE)
    // Runs the player executable with the arguments and waits for it; its exit code, -1 when it did not start.
    i32 RunProcess(Span<const StringView> args);
#endif

    inline bool ReadPng(StringView path, image::Image& out)
    {
        Array<u8> bytes;
        return platform::ReadFile(path, bytes) && image::DecodePng(Span<const u8>(bytes.Data(), bytes.Size()), out);
    }
}
