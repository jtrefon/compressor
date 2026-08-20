#include "compression/ThreadPool.hpp"
#include <stdexcept>

namespace compression {

ThreadPool::ThreadPool(std::size_t threads) {
    if (threads == 0) {
        throw std::invalid_argument("ThreadPool requires at least one worker");
    }
    for (std::size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] { worker(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::shutdown() {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& t : workers_) {
        if (t.joinable()) {
            t.join();
        }
    }
    workers_.clear();
}

void ThreadPool::execute(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (stop_) {
            throw std::runtime_error("ThreadPool: cannot execute after shutdown");
        }
        tasks_.emplace(std::move(task));
    }
    cv_.notify_one();
}


void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) {
                return;
            }
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

} // namespace compression
