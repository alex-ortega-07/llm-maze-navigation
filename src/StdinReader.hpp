#pragma once

#include <atomic>
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

        // True once stdin has been closed and every line has been taken
        bool finished();

    private:
        struct State {
            std::mutex mutex;
            std::deque<std::string> lines;
            std::atomic<bool> closed{false};
        };

        std::shared_ptr<State> state_ = std::make_shared<State>();
};
