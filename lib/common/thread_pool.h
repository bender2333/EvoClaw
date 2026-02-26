#ifndef EVOCLAW_COMMON_THREAD_POOL_H_
#define EVOCLAW_COMMON_THREAD_POOL_H_

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace evoclaw {

/// 简单线程池，替代 std::async 的不确定行为
class ThreadPool {
 public:
  explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency()) {
    for (size_t i = 0; i < num_threads; ++i) {
      workers_.emplace_back([this] {
        while (true) {
          std::function<void()> task;
          {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
          }
          task();
        }
      });
    }
  }

  ~ThreadPool() {
    {
      std::unique_lock lock(mutex_);
      stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_) w.join();
  }

  /// 提交任务，返回 future
  template <typename F, typename... Args>
  auto Submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto result = task->get_future();
    {
      std::unique_lock lock(mutex_);
      tasks_.emplace([task] { (*task)(); });
    }
    cv_.notify_one();
    return result;
  }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

 private:
  std::vector<std::thread> workers_;
  std::queue<std::function<void()>> tasks_;
  std::mutex mutex_;
  std::condition_variable cv_;
  bool stop_ = false;
};

}  // namespace evoclaw

#endif  // EVOCLAW_COMMON_THREAD_POOL_H_
