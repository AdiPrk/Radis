/*****************************************************************//**
 * \file   ThreadPool.cpp
 * \brief  Implementation of the ThreadPool class.
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
        {
            threadCount = 2;// std::thread::hardware_concurrency();
        }

        mWorkers.reserve(threadCount);
        for (uint32_t i = 0; i < threadCount; ++i)
            mWorkers.emplace_back([this] { WorkerLoop(); });
    }

    ThreadPool::~ThreadPool()
    {
        {
            std::lock_guard lock(mMutex);
            mStopping = true;
        }
        mTaskCV.notify_all(); // wake every worker so they can exit
        for (auto& worker : mWorkers)
            worker.join();
    }

    void ThreadPool::WaitAll()
    {
        std::unique_lock lock(mMutex);
        // mActiveTasks counts both tasks still in the queue and tasks currently
        // executing on a worker, so zero means truly all work is done.
        mIdleCV.wait(lock, [this] { return mActiveTasks == 0; });
    }

    void ThreadPool::WorkerLoop()
    {
        while (true)
        {
            std::function<void()> task;

            {
                std::unique_lock lock(mMutex);
                mTaskCV.wait(lock, [this] { return mStopping || !mTasks.empty(); });

                if (mStopping && mTasks.empty())
                    break;

                task = std::move(mTasks.front());
                mTasks.pop();
                // mActiveTasks is NOT decremented here — the task is still "active"
                // until it finishes executing below.
            }

            task(); // execute outside the lock so other workers aren't blocked

            {
                std::lock_guard lock(mMutex);
                --mActiveTasks;
            }
            mIdleCV.notify_all(); // wake WaitAll() in case we just hit zero
        }
    }

} // namespace Radis
