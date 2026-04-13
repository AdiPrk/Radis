/*****************************************************************//**
 * \file   ThreadPool.h
 * \brief  Priority-aware thread pool for asset load jobs.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#pragma once

#include "AssetTypes.h"

namespace Radis
{
    struct AssetJob
    {
        std::function<void()>   work;
        AssetPriority           priority = AssetPriority::Normal;

        bool operator<(const AssetJob& o) const
        {
            return static_cast<uint8_t>(priority) < static_cast<uint8_t>(o.priority);
        }
    };

    class ThreadPool
    {
    public:
        explicit ThreadPool(uint32_t threadCount = 0);
        ~ThreadPool();

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        void     submit(std::function<void()> work, AssetPriority priority = AssetPriority::Normal);
        void     waitAll();
        uint32_t threadCount() const { return static_cast<uint32_t>(mThreads.size()); }
        uint32_t activeCount() const { return mActiveCount.load(std::memory_order_relaxed); }
        uint32_t queueDepth();

    private:
        void workerLoop();

        std::vector<std::thread>        mThreads;
        std::priority_queue<AssetJob>   mQueue;
        std::mutex                      mQueueMutex;
        std::condition_variable         mWorkReady;
        std::condition_variable         mAllIdle;
        std::atomic<uint32_t>           mActiveCount{ 0 };
        bool                            mShutdown = false;
    };

} // namespace Radis