#include <compression/ThreadPool.hpp>
#include <compression/core/Executor.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace compression;
using namespace compression::core;

TEST(InlineExecutorTest, RunsSynchronously) {
  InlineExecutor executor;
  bool ran = false;
  executor.execute([&ran]() { ran = true; });
  EXPECT_TRUE(ran);
}

TEST(InlineExecutorTest, SubmitReturnsResultImmediately) {
  InlineExecutor executor;
  auto future = executor.submit([]() { return 42; });
  EXPECT_EQ(future.get(), 42);
}

TEST(InlineExecutorTest, SubmitPropagatesExceptions) {
  InlineExecutor executor;
  auto future = executor.submit([]() -> int {
    throw std::runtime_error("boom");
    return 0;
  });
  EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ExecutorTest, ThreadPoolIsAnExecutor) {
  EXPECT_TRUE(
      (std::is_base_of<IExecutor, ThreadPool>::value));
}

TEST(ExecutorTest, ThreadPoolSubmitReturnsCorrectResults) {
  ThreadPool pool(4);
  std::vector<std::future<int>> futures;
  for (int i = 0; i < 32; ++i) {
    futures.push_back(pool.submit([i]() { return i * i; }));
  }
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(futures[i].get(), i * i);
  }
}

TEST(ExecutorTest, ThreadPoolExecuteRunsAllTasks) {
  ThreadPool pool(4);
  std::atomic<int> counter{0};
  for (int i = 0; i < 64; ++i) {
    pool.execute([&counter]() { counter.fetch_add(1); });
  }
  // Wait for quiescence via a barrier task.
  auto done = pool.submit([]() {});
  done.wait();
  EXPECT_EQ(counter.load(), 64);
}

TEST(ExecutorTest, ThreadPoolEnqueueStillWorks) {
  ThreadPool pool(2);
  auto future = pool.enqueue([]() { return 7; });
  EXPECT_EQ(future.get(), 7);
}

TEST(ExecutorTest, ThreadPoolExceptionPropagates) {
  ThreadPool pool(2);
  auto future = pool.submit([]() -> int {
    throw std::runtime_error("task failed");
    return 0;
  });
  EXPECT_THROW(future.get(), std::runtime_error);
}

TEST(ExecutorTest, ThreadPoolShutdownRejectsNewWork) {
  ThreadPool pool(2);
  pool.shutdown();
  EXPECT_THROW(pool.execute([]() {}), std::runtime_error);
  EXPECT_THROW(pool.enqueue([]() { return 1; }), std::runtime_error);
  // Shutdown is idempotent.
  EXPECT_NO_THROW(pool.shutdown());
}

TEST(ExecutorTest, ThreadPoolShutdownJoinsPendingTasks) {
  std::atomic<int> counter{0};
  {
    ThreadPool pool(2);
    for (int i = 0; i < 8; ++i) {
      pool.execute([&counter]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        counter.fetch_add(1);
      });
    }
  }
  EXPECT_EQ(counter.load(), 8);
}
