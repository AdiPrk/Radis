/*****************************************************************//**
 * \file   ThreadPool.cpp
 * \brief  Implementation of the asset system thread pool.
 *
 * \author Aditya Prakash
 * \date   January 2026
 *********************************************************************/

#include <PCH/pch.h>
#include "ThreadPool.h"

namespace Radis
{
    ThreadPool::ThreadPool(uint32_t threadCount)
    {
        if (threadCount == 0)
            threadCount = std::max(1u, std::thread::hardware_concurrency());

        mThreads.reserve(threadCount);
        for (uint32_t i = 0; i < threadCount; ++i)
            mThreads.emplace_back(&ThreadPool::workerLoop, this);
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::lock_guard lock(mQueueMutex);
            mShutdown = true;
        }
        mWorkReady.notify_all();
        for (auto& t : mThreads)
            t.join();
    }

    void ThreadPool::submit(std::function<void()> work, AssetPriority priority)
    {
        {
            std::lock_guard lock(mQueueMutex);
            mQueue.push({ std::move(work), priority });
        }
        mWorkReady.notify_one();
    }

    void ThreadPool::waitAll()
    {
        std::unique_lock lock(mQueueMutex);
        mAllIdle.wait(lock, [this]
        {
            return mQueue.empty() && mActiveCount.load(std::memory_order_relaxed) == 0;
        });
    }

    uint32_t ThreadPool::queueDepth()
    {
        std::lock_guard lock(mQueueMutex);
        return static_cast<uint32_t>(mQueue.size());
    }

    void ThreadPool::workerLoop()
    {
        while (true)
        {
            AssetJob job;
            {
                std::unique_lock lock(mQueueMutex);
                mWorkReady.wait(lock, [this] { return !mQueue.empty() || mShutdown; });
                if (mShutdown && mQueue.empty()) return;
                job = std::move(const_cast<AssetJob&>(mQueue.top()));
                mQueue.pop();
                mActiveCount.fetch_add(1, std::memory_order_relaxed);
            }

            job.work();

            {
                std::lock_guard lock(mQueueMutex);
                mActiveCount.fetch_sub(1, std::memory_order_relaxed);
            }
            mAllIdle.notify_all();
        }
    }

} // namespace Radis