#include <PCH/pch.h>
#include "FrameRate.h"

using namespace std::chrono;

namespace Dog {

    FrameRateController::FrameRateController(unsigned int targetFrameRate)
        : mTargetFPS(targetFrameRate) 
        , mLastFrameTime(high_resolution_clock::now())
    {
        timeBeginPeriod(1); // Request 1ms timer resolution

        if (targetFrameRate == 0)
        {
            mTargetFrameDuration = duration<double>(0);
        }
        else
        {
            mTargetFrameDuration = duration<double>(1.0 / targetFrameRate);
        }
    }

    FrameRateController::~FrameRateController()
    {
        timeEndPeriod(1); // End 1ms timer resolution
    }

    float FrameRateController::WaitForNextFrame()
    {
        auto now = high_resolution_clock::now();
        auto timeSinceLastFrame = now - mLastFrameTime;

        if (mTargetFPS == 0) {
            mLastFrameTime = now;
            return duration<float>(timeSinceLastFrame).count();
        }

        // Sleep until approximately just before the targetFrameDuration
        if (timeSinceLastFrame < mTargetFrameDuration - 2ms) {
            std::this_thread::sleep_for(mTargetFrameDuration - timeSinceLastFrame - 2ms);
        }

        // Fine-tune with busy-waiting for the last small part
        while (timeSinceLastFrame < mTargetFrameDuration) {
            now = high_resolution_clock::now();
            timeSinceLastFrame = now - mLastFrameTime;
        }

        mLastFrameTime = high_resolution_clock::now();
        return duration<float>(timeSinceLastFrame).count();
    }

    void FrameRateController::SetTargetFPS(unsigned int targetFPS)
    {
        mTargetFPS = targetFPS;
        mTargetFrameDuration = duration<double>(1.0 / targetFPS);
    }

}
