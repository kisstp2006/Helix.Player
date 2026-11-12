#pragma once

#include <Helix/Core/Strings/StringView.h>

// What the player host needs from the operating system beyond Helix.Platform.
namespace helix::player::host
{
    // -single-instance: false when another instance of the product runs (it is brought to the front).
    bool AcquireSingleInstance(StringView product);
    void ReleaseSingleInstance();
    // -wait-for-native-debugger: tells the process id and waits until a debugger is attached.
    void WaitForNativeDebugger();
    // Screen.sleepTimeout: true keeps the display on.
    void SetKeepScreenOn(bool keepOn);
    // Screen.orientation on mobile (a ScreenOrientation value).
    void SetOrientation(i32 orientation);
    // Each frame: the native window the graphics device uses now (null for none). Android waits for it before it
    // lets a surface go.
    void AcknowledgeWindow(void* nativeWindow);
}
