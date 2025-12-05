#pragma once

// Simple frame rate controller.

namespace Radis {

    class FrameRateController {
    public:
        explicit FrameRateController(unsigned int targetFrameRate);
        ~FrameRateController();

        // Returns the time in seconds since the last frame (dt)
        float WaitForNextFrame();

        // Set the target frame rate
        void SetTargetFPS(unsigned int targetFPS);

    private:
        unsigned int mTargetFPS;
        std::chrono::time_point<std::chrono::high_resolution_clock> mLastFrameTime;
        std::chrono::duration<double> mTargetFrameDuration;
    };

} // namespace Radis