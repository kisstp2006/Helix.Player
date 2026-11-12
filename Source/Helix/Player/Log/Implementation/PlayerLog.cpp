#include <Helix/Player/Log/PlayerLog.h>

#include <Helix/Core/Logging/Log.h>
#include <Helix/Platform/IO/FileSystem.h>
#include <Helix/Platform/Threading/Thread.h>
#include <Helix/Platform/Time/Time.h>

#include <cstdio>

namespace helix::player
{
    struct PlayerLog::State
    {
        platform::Mutex mutex;
        platform::File file;
        bool console = false;
        bool timestamps = false;
        platform::Stopwatch clock;
    };

    namespace
    {
        void Sink(const LogMessage& m, void* user)
        {
            auto& s = *static_cast<PlayerLog::State*>(user);
            char head[96];
            int n = 0;
            if (s.timestamps)
                n = std::snprintf(head, sizeof(head), "[%10.4f %llu] ", s.clock.ElapsedSeconds(), static_cast<unsigned long long>(platform::CurrentThreadId()));
            int h = std::snprintf(head + n, sizeof(head) - usize(n), "[%s] %s: ", LogLevelName(m.level), m.channel ? m.channel->name : "");
            usize headSize = usize(n + (h > 0 ? h : 0));
            if (headSize >= sizeof(head))
                headSize = sizeof(head) - 1;
            platform::ScopedLock lock(s.mutex);
            if (s.console)
            {
                std::fwrite(head, 1, headSize, stdout);
                std::fwrite(m.text.Data(), 1, m.text.Size(), stdout);
                std::fputc('\n', stdout);
                std::fflush(stdout);
                return;
            }
            s.file.Write(head, headSize);
            s.file.Write(m.text.Data(), m.text.Size());
            s.file.Write("\n", 1);
            // Errors reach the disk at once: a crash right after them must not lose them.
            if (m.level >= LogLevel::Error)
                s.file.Flush();
        }
    }

    bool PlayerLog::Open(StringView path, bool timestamps)
    {
        Close();
        if (path.Empty())
            return true;
        state_ = new State();
        state_->timestamps = timestamps;
        if (path == "-")
            state_->console = true;
        else
        {
            platform::CreateDirectories(String(platform::GetDirectoryName(path)));
            if (!state_->file.Open(path, platform::FileMode::Write))
            {
                delete state_;
                state_ = nullptr;
                return false;
            }
        }
        path_ = String(path);
        AddLogSink(&Sink, state_);
        // The file takes the place of the console; standard output takes it when asked for.
        SetConsoleLogging(false);
        open_ = true;
        return true;
    }

    void PlayerLog::Close()
    {
        if (!state_)
            return;
        RemoveLogSink(&Sink, state_);
        SetConsoleLogging(true);
        {
            platform::ScopedLock lock(state_->mutex);
            if (state_->file.IsOpen())
                state_->file.Flush();
        }
        delete state_;
        state_ = nullptr;
        open_ = false;
        path_ = String();
    }

    String PlayerLog::DefaultPath(StringView folder)
    {
        String path = platform::JoinPath(folder, "Player.log");
        String previous = platform::JoinPath(folder, "Player-prev.log");
        if (platform::FileExists(path))
        {
            platform::RemoveFile(previous);
            platform::RenameFile(path, previous);
        }
        return path;
    }
}
