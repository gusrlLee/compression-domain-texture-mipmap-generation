#include "task_dispatch.h"

#include <cassert>

static TaskDispatch* s_instance = nullptr;

TaskDispatch::TaskDispatch(size_t workers) : exit_(false), jobs_(0) {
    assert(!s_instance);
    s_instance = this;

    assert(workers >= 1);
    workers--; // Main thread acts as one worker, so create (N-1) workers

    workers_.reserve(workers);
    for (size_t i = 0; i < workers; ++i) {
        workers_.emplace_back([this]() { Worker(); });
    }
}

TaskDispatch::~TaskDispatch() {
    exit_ = true;
    queue_lock_.lock();
    cv_work_.notify_all(); // Wake up all sleeping workers for termination
    queue_lock_.unlock();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    assert(s_instance);
    s_instance = nullptr;
}

void TaskDispatch::Queue(const std::function<void()>& f) {
    std::unique_lock<std::mutex> lock(s_instance->queue_lock_);
    s_instance->queue_.emplace_back(f);
    const auto size = s_instance->queue_.size();
    lock.unlock();
    
    // Wake up a worker only if there are 2 or more tasks (1 task is reserved for the main thread)
    if (size > 1) {
        s_instance->cv_work_.notify_one();
    }
}

void TaskDispatch::Queue(std::function<void()>&& f) {
    std::unique_lock<std::mutex> lock(s_instance->queue_lock_);
    s_instance->queue_.emplace_back(std::move(f));
    const auto size = s_instance->queue_.size();
    lock.unlock();

    if (size > 1) {
        s_instance->cv_work_.notify_one();
    }
}

void TaskDispatch::Sync() {
    std::unique_lock<std::mutex> lock(s_instance->queue_lock_);
    
    // Main thread actively takes tasks from the queue and executes them
    while (!s_instance->queue_.empty()) {
        auto f = std::move(s_instance->queue_.back());
        s_instance->queue_.pop_back();
        lock.unlock();
        
        f(); // Main thread execution
        
        lock.lock();
    }
    
    // Wait safely until all background workers finish their current jobs (jobs_ == 0)
    s_instance->cv_jobs_.wait(lock, [] { return s_instance->jobs_ == 0; });
}

void TaskDispatch::Worker() {
    for (;;) {
        std::unique_lock<std::mutex> lock(queue_lock_);
        
        // Sleep until there is a task or an exit command is issued
        cv_work_.wait(lock, [this] { return !queue_.empty() || exit_; });
        if (exit_) return;

        auto f = std::move(queue_.back());
        queue_.pop_back();
        jobs_++;
        lock.unlock();
        
        f(); // Worker thread execution
        
        lock.lock();
        jobs_--;
        bool notify = (jobs_ == 0 && queue_.empty());
        lock.unlock();
        
        // The last worker finishing its job wakes up the main thread waiting in Sync()
        if (notify) {
            cv_jobs_.notify_all();
        }
    }
}