#include <Helix/Player/Host/Implementation/HostAndroid.h>
#include <Helix/Player/Host/Player.h>

#include <Helix/Core/Strings/Format.h>
#include <Helix/Platform/Threading/Thread.h>
#include <Helix/Platform/Window/Android.h>

#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>

// The Android player: libHelixPlayer.so with the native methods of org.helix.player.HelixActivity. The player runs
// on a thread of its own; the UI thread hands it the surface, focus, touches and keys.
namespace
{
    using namespace helix;

    // The data folder inside the APK's assets.
    class ApkReader final : public engine::DataFileReader
    {
    public:
        AAssetManager* manager = nullptr;

        bool Read(StringView root, StringView path, Array<u8>& out) override
        {
            if (!manager)
                return false;
            String full = root.Empty() ? String(path) : Format("{}/{}", root, path);
            AAsset* asset = AAssetManager_open(manager, full.CStr(), AASSET_MODE_BUFFER);
            if (!asset)
                return false;
            off64_t size = AAsset_getLength64(asset);
            out.Resize(usize(size));
            bool ok = size == 0 || AAsset_read(asset, out.Data(), usize(size)) == int(size);
            AAsset_close(asset);
            return ok;
        }
    };

    struct AndroidPlayer
    {
        JavaVM* vm = nullptr;
        jobject assets = nullptr;
        ApkReader reader;
        Array<String> args = Array<String>();
        platform::Thread thread;
        bool running = false;
    };

    AndroidPlayer& State()
    {
        static AndroidPlayer s;
        return s;
    }

    void RunOnThread(void*)
    {
        AndroidPlayer& s = State();
        player::host::android::AttachThread();
        Array<StringView> views;
        for (const String& a : s.args)
            views.Push(a);
        player::RunPlayer(Span<const StringView>(views.Data(), views.Size()), &s.reader);
        player::host::android::FinishActivity();
        player::host::android::DetachThread();
    }

    // The arguments of the intent's "args" extra, split at spaces.
    void Split(StringView text, Array<String>& out)
    {
        usize at = 0;
        while (at < text.Size())
        {
            while (at < text.Size() && text[at] == ' ')
                at++;
            usize end = at;
            while (end < text.Size() && text[end] != ' ')
                end++;
            if (end > at)
                out.Push(String(text.Substr(at, end - at)));
            at = end;
        }
    }

    platform::Event TouchEvent(jint action, jint id, jfloat x, jfloat y)
    {
        platform::Event e;
        // MotionEvent: ACTION_DOWN 0, UP 1, MOVE 2, CANCEL 3, POINTER_DOWN 5, POINTER_UP 6.
        e.type = action == 0 || action == 5 ? platform::EventType::TouchBegin
               : action == 1 || action == 6 ? platform::EventType::TouchEnd
               : action == 2                ? platform::EventType::TouchMove
                                            : platform::EventType::TouchCancel;
        e.touch = {i32(id), f32(x), f32(y)};
        return e;
    }
}

extern "C"
{
    JNIEXPORT void JNICALL Java_org_helix_player_HelixActivity_nativeCreate(JNIEnv* env, jobject activity, jobject assets, jstring args, jfloat density)
    {
        AndroidPlayer& s = State();
        if (s.running)
            return;
        env->GetJavaVM(&s.vm);
        player::host::android::SetActivity(s.vm, activity);
        s.assets = env->NewGlobalRef(assets);
        s.reader.manager = AAssetManager_fromJava(env, s.assets);
        platform::android::SetContentScale(f32(density));
        s.args.Clear();
        if (args)
        {
            const char* text = env->GetStringUTFChars(args, nullptr);
            Split(StringView(text), s.args);
            env->ReleaseStringUTFChars(args, text);
        }
        s.running = s.thread.Start({.function = &RunOnThread, .user = nullptr, .name = "HelixPlayer"});
    }

    JNIEXPORT void JNICALL Java_org_helix_player_HelixActivity_nativeSurface(JNIEnv* env, jobject, jobject surface)
    {
        ANativeWindow* next = surface ? ANativeWindow_fromSurface(env, surface) : nullptr;
        ANativeWindow* previous = platform::android::GetNativeWindow();
        platform::android::SetNativeWindow(next);
        if (previous && previous != next)
        {
            // The player's device lets the old surface go before the UI thread does.
            player::host::android::WaitUntilReleased(previous);
            ANativeWindow_release(previous);
        }
        else if (previous == next && next)
            ANativeWindow_release(next);
    }

    JNIEXPORT void JNICALL Java_org_helix_player_HelixActivity_nativeFocus(JNIEnv*, jobject, jboolean focused)
    {
        platform::android::SetFocused(focused == JNI_TRUE);
    }

    JNIEXPORT void JNICALL Java_org_helix_player_HelixActivity_nativeTouch(JNIEnv*, jobject, jint action, jint id, jfloat x, jfloat y)
    {
        platform::android::PushEvent(TouchEvent(action, id, x, y));
    }

    JNIEXPORT void JNICALL Java_org_helix_player_HelixActivity_nativeKey(JNIEnv*, jobject, jboolean down, jint keyCode)
    {
        platform::Event e;
        e.type = down == JNI_TRUE ? platform::EventType::KeyDown : platform::EventType::KeyUp;
        // KEYCODE_BACK is the escape key of the reference's Android player.
        e.key = {keyCode == 4 ? platform::KeyCode::Escape : platform::KeyCode::None, u32(keyCode)};
        platform::android::PushEvent(e);
    }

    JNIEXPORT void JNICALL Java_org_helix_player_HelixActivity_nativeDestroy(JNIEnv* env, jobject)
    {
        AndroidPlayer& s = State();
        if (!s.running)
            return;
        platform::Event quit;
        quit.type = platform::EventType::Quit;
        platform::android::PushEvent(quit);
        s.thread.Join();
        s.running = false;
        player::host::android::SetActivity(nullptr, nullptr);
        env->DeleteGlobalRef(s.assets);
        s.assets = nullptr;
        s.reader.manager = nullptr;
    }
}
