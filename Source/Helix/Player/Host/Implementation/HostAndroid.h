#pragma once

#include <Helix/Core/Basics/Types.h>

#include <jni.h>

// The Java side of the Android player: the activity the host calls back (keep the screen on, orientation, finish) and
// the surface hand over between the UI thread and the player's thread.
namespace helix::player::host::android
{
    // The activity (a global reference is taken) and the virtual machine; null clears them.
    void SetActivity(JavaVM* vm, jobject activity);
    // The player's thread attaches to the virtual machine (and detaches at its end).
    void AttachThread();
    void DetachThread();
    // Asks the activity to finish (the player's run ended).
    void FinishActivity();
    // Blocks the UI thread until the player's thread no longer uses the window (or a second passed).
    void WaitUntilReleased(void* nativeWindow);
}
