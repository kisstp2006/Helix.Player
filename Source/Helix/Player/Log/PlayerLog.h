#pragma once

#include <Helix/Core/Strings/String.h>

// The player's log: every log message of every thread into a file (Player.log in the persistent data path by
// default, the previous run's kept as Player-prev.log) or standard output, optionally with time and thread id.
namespace helix::player
{
    class PlayerLog
    {
    public:
        PlayerLog() = default;
        ~PlayerLog() { Close(); }
        PlayerLog(const PlayerLog&) = delete;
        PlayerLog& operator=(const PlayerLog&) = delete;

        // "-" writes to standard output; an empty path writes nothing. False when the file cannot be opened.
        bool Open(StringView path, bool timestamps);
        void Close();
        bool IsOpen() const { return open_; }
        StringView Path() const { return path_; }

        // The default file in a folder, keeping the previous one.
        static String DefaultPath(StringView folder);

        // What the log sink writes to (any thread).
        struct State;

    private:
        State* state_ = nullptr;
        String path_;
        bool open_ = false;
    };
}
