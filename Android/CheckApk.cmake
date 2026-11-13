# Checks a packaged player APK (cmake -P): the .NET runtime's native libraries next to the player's, the framework and
# the engine's assemblies in the assets' Managed/.
#   -DAPK=<file> -DSTAMP=<file written when it passes>
cmake_minimum_required(VERSION 3.28)

execute_process(COMMAND "${CMAKE_COMMAND}" -E tar tf "${APK}" RESULT_VARIABLE result OUTPUT_VARIABLE listing ERROR_VARIABLE error)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "CheckApk: ${APK} cannot be listed: ${error}")
endif()
# The listing shows Windows separators on Windows (the entries have forward slashes).
string(REPLACE "\\" "/" listing "${listing}")
set(missing "")
foreach(entry IN ITEMS
        lib/arm64-v8a/libHelixPlayer.so lib/arm64-v8a/libHelixEngine.so lib/arm64-v8a/libcoreclr.so lib/arm64-v8a/libclrjit.so
        lib/arm64-v8a/libSystem.Native.so assets/Managed/System.Private.CoreLib.dll assets/Managed/System.Runtime.dll
        assets/Managed/Helix.Scripting.Runtime.dll assets/Managed/HelixEngine.CoreModule.dll assets/Managed/Helix.Mathematics.dll)
    string(FIND "${listing}" "${entry}" at)
    if(at EQUAL -1)
        list(APPEND missing "${entry}")
    endif()
endforeach()
if(missing)
    message(FATAL_ERROR "CheckApk: ${APK} lacks ${missing}")
endif()
string(REGEX MATCHALL "assets/Managed/[^\n]*\\.dll" assemblies "${listing}")
list(LENGTH assemblies count)
file(SIZE "${APK}" size)
math(EXPR mb "${size} / 1048576")
message(STATUS "CheckApk: the .NET runtime is in the APK (${count} assemblies, ${mb} MB)")
file(WRITE "${STAMP}" "${count}\n")
