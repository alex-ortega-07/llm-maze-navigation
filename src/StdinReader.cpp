#include "StdinReader.hpp"

#include <chrono>
#include <iostream>
#include <thread>

void StdinReader::start(){
    std::shared_ptr<State> state = state_;

    std::thread([state]{
        std::string line;
        while(std::getline(std::cin, line)){
            // The supervisor may send "\r\n" line endings on Windows
            if(!line.empty() && line.back() == '\r') line.pop_back();

            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->lines.push_back(line);
            }
            state->line_available.notify_one();
        }

        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->closed = true;
        }
        state->line_available.notify_one();
    }).detach();
}

bool StdinReader::pop(std::string &line){
    std::lock_guard<std::mutex> lock(state_->mutex);
    if(state_->lines.empty()) return false;

    line = state_->lines.front();
    state_->lines.pop_front();
    return true;
}

bool StdinReader::wait_for_line(int timeout_ms){
    std::unique_lock<std::mutex> lock(state_->mutex);

    return state_->line_available.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this]{
        return !state_->lines.empty() || state_->closed;
    });
}

bool StdinReader::finished(){
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->closed && state_->lines.empty();
}
