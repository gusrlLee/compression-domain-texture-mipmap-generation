#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

// 100% standalone Task Dispatcher utilizing etcpak's optimized thread pool structure
class TaskDispatch {
public:
    // workers: Total number of threads to use (including the main thread)
    explicit TaskDispatch(size_t workers);
    ~TaskDispatch();

    // Prevent copying and assignment
    TaskDispatch(const TaskDispatch&) = delete;
    TaskDispatch& operator=(const TaskDispatch&) = delete;

    // Enqueues a task
    static void Queue(const std::function<void()>& f);
    static void Queue(std::function<void()>&& f);

    // Waits until all tasks are finished.
    // Core logic: The main thread does not idle but actively processes remaining tasks in the queue.
    static void Sync();

private:
    void Worker();

    std::vector<std::function<void()>> queue_;
    std::mutex queue_lock_;
    std::condition_variable cv_work_;
    std::condition_variable cv_jobs_;
    std::atomic<bool> exit_;
    size_t jobs_;

    std::vector<std::thread> workers_;
};