#include <Helix/Player/Host/Implementation/HostAndroid.h>
#include <Helix/Player/Host/Implementation/HostPlatform.h>

#include <Helix/Platform/Time/Time.h>

#include <atomic>

namespace helix::player::host
{
    namespace
    {
        JavaVM* vm = nullptr;
        jobject activity = nullptr;
        thread_local JNIEnv* threadEnv = nullptr;
        std::atomic<void*> usedWindow{nullptr};

        JNIEnv* Env()
        {
            if (threadEnv)
                return threadEnv;
            JNIEnv* env = nullptr;
            if (vm && vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK)
                return env;
            return nullptr;
        }

        void CallActivity(const char* method, const char* signature, jvalue value, bool hasValue)
        {
            JNIEnv* env = Env();
            if (!env || !activity)
                return;
            jclass c = env->GetObjectClass(activity);
            jmethodID m = env->GetMethodID(c, method, signature);
            if (m)
            {
                if (hasValue)
                    env->CallVoidMethodA(activity, m, &value);
                else
                    env->CallVoidMethod(activity, m);
            }
            if (env->ExceptionCheck())
                env->ExceptionClear();
            env->DeleteLocalRef(c);
        }
    }

    namespace android
    {
        void SetActivity(JavaVM* machine, jobject object)
        {
            JNIEnv* env = nullptr;
            if (machine)
                machine->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            else if (vm)
                vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
            if (activity && env)
                env->DeleteGlobalRef(activity);
            activity = object && env ? env->NewGlobalRef(object) : nullptr;
            vm = machine;
        }

        void AttachThread()
        {
            if (vm && !threadEnv)
                vm->AttachCurrentThread(&threadEnv, nullptr);
        }

        void DetachThread()
        {
            if (vm && threadEnv)
            {
                vm->DetachCurrentThread();
                threadEnv = nullptr;
            }
        }

        void FinishActivity() { CallActivity("finishFromPlayer", "()V", jvalue{}, false); }

        void WaitUntilReleased(void* nativeWindow)
        {
            for (int i = 0; i < 100 && nativeWindow && usedWindow.load() == nativeWindow; i++)
                platform::SleepSeconds(0.01);
        }
    }

    bool AcquireSingleInstance(StringView) { return true; }
    void ReleaseSingleInstance() {}
    void WaitForNativeDebugger() {}

    void SetKeepScreenOn(bool keepOn)
    {
        jvalue v;
        v.z = keepOn ? JNI_TRUE : JNI_FALSE;
        CallActivity("setKeepScreenOn", "(Z)V", v, true);
    }

    void SetOrientation(i32 orientation)
    {
        jvalue v;
        v.i = orientation;
        CallActivity("setOrientation", "(I)V", v, true);
    }

    void AcknowledgeWindow(void* nativeWindow) { usedWindow.store(nativeWindow); }
}
