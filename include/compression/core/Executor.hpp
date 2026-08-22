#pragma once

#include <functional>
#include <future>
#include <memory>
#include <type_traits>

namespace compression {
namespace core {

/**
 * @brief Port: asynchronous task execution.
 *
 * All concurrency in the library flows through this interface so callers
 * can inject deterministic executors in tests and switch policies
 * (thread pool, work-stealing, inline) without touching domain code.
 */
class IExecutor {
public:
  virtual ~IExecutor() = default;

  /**
   * @brief Submits a fire-and-forget task for asynchronous execution.
   */
  virtual void execute(std::function<void()> task) = 0;

  /**
   * @brief Submits a task and returns a future to its result.
   */
  template <class F>
  std::future<typename std::invoke_result_t<F>> submit(F &&f) {
    using Result = typename std::invoke_result_t<F>;
    auto task =
        std::make_shared<std::packaged_task<Result()>>(std::forward<F>(f));
    std::future<Result> result = task->get_future();
    execute([task]() mutable { (*task)(); });
    return result;
  }
};

/**
 * @brief Executor that runs every task synchronously on the calling thread.
 *
 * Used in tests for determinism and in single-threaded configurations.
 */
class InlineExecutor : public IExecutor {
public:
  void execute(std::function<void()> task) override { task(); }
};

} // namespace core
} // namespace compression
