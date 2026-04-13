/*****************************************************************//**
 * \file   ThreadPool.h
 * \brief  General-purpose thread pool with fire-and-forget and future-based submission.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

namespace Radis
{
    class ThreadPool
    {
    public:
        /// Spawns `threadCount` worker threads. Defaults to hardware concurrency.
        explicit ThreadPool(uint32_t threadCount = std::thread::hardware_concurrency());
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        /// Submit a callable for fire-and-forget execution.
        /// mActiveTasks is incremented before the task runs and decremented after,
        /// so WaitAll() correctly accounts for both queued and in-progress work.
        template<typename F>
        void Submit(F&& task);

        /// Submit a callable and get back a std::future for its return value.
        template<typename F>
        [[nodiscard]] auto SubmitWithResult(F&& task) -> std::future<std::invoke_result_t<F>>;

        /// Block until all submitted work has fully completed (queue empty + all workers idle).
        void WaitAll();

        [[nodiscard]] uint32_t ThreadCount() const { return static_cast<uint32_t>(mWorkers.size()); }

    private:
        void WorkerLoop();

        std::vector<std::thread>          mWorkers;
        std::queue<std::function<void()>> mTasks;
        std::mutex                        mMutex;
        std::condition_variable           mTaskCV;   // wakes workers when tasks arrive
        std::condition_variable           mIdleCV;   // wakes WaitAll() when work drains
        uint32_t                          mActiveTasks = 0; // queued + in-progress
        bool                              mStopping = false;
    };

    // -------------------------------------------------------------------------
    //  Template implementations (must live in the header)
    // -------------------------------------------------------------------------

    template<typename F>
    void ThreadPool::Submit(F&& task)
    {
        {
            std::lock_guard lock(mMutex);
            mTasks.emplace(std::forward<F>(task));
            ++mActiveTasks;
        }
        mTaskCV.notify_one();
    }

    template<typename F>
    auto ThreadPool::SubmitWithResult(F&& task) -> std::future<std::invoke_result_t<F>>
    {
        using R = std::invoke_result_t<F>;

        // Wrap the task in a shared_ptr<promise> so the future stays valid even if
        // this ThreadPool is destroyed before the caller collects the result.
        auto promise = std::make_shared<std::promise<R>>();
        auto future = promise->get_future();

        Submit([p = std::move(promise), t = std::forward<F>(task)]() mutable
            {
                if constexpr (std::is_void_v<R>)
                {
                    t();
                    p->set_value();
                }
                else
                {
                    p->set_value(t());
                }
            });

        return future;
    }

} // namespace Radis
