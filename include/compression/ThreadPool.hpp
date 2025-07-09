#pragma once

#include <vector>
#include <thread>
#include <queue>
#include <functional>
#include <future>
#include <type_traits>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace compression {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t threads);
    ~ThreadPool();

    // Enqueue a task into the pool and get a future to the result
    template <class F>
    auto enqueue(F&& f) -> std::future<typename std::invoke_result_t<F>>;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;

    void worker();
};

template <class F>
auto ThreadPool::enqueue(F&& f) -> std::future<typename std::invoke_result_t<F>> {
    using return_type = typename std::invoke_result_t<F>;
    auto task = std::make_shared<std::packaged_task<return_type()>>(std::forward<F>(f));
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(mutex_);
        tasks_.emplace([task]() { (*task)(); });
    }
    cv_.notify_one();
    return res;
}

} // namespace compression
