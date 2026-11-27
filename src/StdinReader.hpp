#pragma once

#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

// Reads stdin line by line in a background thread, so the SDL loop never blocks waiting for input.
// The state is shared with the thread through a shared_ptr, so it stays alive as long as the thread needs it.
class StdinReader {
    public:
        // Starts the background thread. Call it once.
        void start();

        // Takes the oldest pending line. Returns false if there is none.
        bool pop(std::string &line);

        // Waits up to `timeout_ms` milliseconds until a line is available or stdin is closed.
        // Returns true if there is something to handle (a line or the end of stdin).
        bool wait_for_line(int timeout_ms);

        // True once stdin has been closed and every line has been taken
        bool finished();

    private:
        struct State {
            std::mutex mutex;
            std::condition_variable line_available;
            std::deque<std::string> lines;
            bool closed = false;  // protected by `mutex`
        };

        std::shared_ptr<State> state_ = std::make_shared<State>();
};
