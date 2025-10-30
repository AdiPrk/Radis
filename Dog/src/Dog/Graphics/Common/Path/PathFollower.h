#pragma once

#include "CubicSpline.h"
#include "ArcLengthTable.h"
#include "SpeedControl.h"

namespace Dog
{
    /**
     * @struct PathSample
     * @brief A snapshot of the path's state at a point.
     */
    struct PathSample
    {
        glm::vec3 position{ 0.0f };
        glm::vec3 tangent{ 0.0f, 0.0f, 1.0f }; // Default forward
    };

    /**
     * @class PathFollower
     * @brief Combines a spline, arc length tables, and speed control.
     */
    class PathFollower
    {
    public:
        PathFollower() = default;

        /**
         * @brief Builds the complete path, including all segment tables.
         * @param controlPoints The vector of glm::vec3 control points.
         * @param arcLengthEpsilon Arc length error threshold (e).
         * @param arcLengthDelta Max parameter interval (d).
         */
        void BuildPath(const std::vector<glm::vec3>& controlPoints,
            double arcLengthEpsilon = 0.001,
            double arcLengthDelta = 0.01);

        /**
         * @brief Sets the speed profile for the path.
         * @param easeInTime Normalized time to finish ease-in [0, 1].
         * @param easeOutTime Normalized time to start ease-out [0, 1].
         */
        void SetSpeedProfile(double easeInTime, double easeOutTime);

        /**
         * @brief Gets a full position and tangent sample for a normalized time.
         * @param t_norm Normalized time [0, 1] for the *entire* animation.
         * @return A PathSample struct with position and tangent.
         */
        PathSample GetSampleAtTime(double t_norm) const;

        /**
         * @brief Gets the final 4x4 model matrix for your object.
         * @param t_norm Normalized time [0, 1].
         * @param worldUp The world's "up" vector (e.g., {0, 1, 0}).
         * @return A glm::mat4 model matrix for rendering.
         */
        glm::mat4 GetTransformAtTime(double t_norm, const glm::vec3& worldUp) const;

        /**
         * @return A const reference to the underlying spline (for rendering).
         */
        const CubicSpline& GetSpline() const { return m_path; }

        /**
         * @brief Gets the character's current speed in world units per second.
         * @param t_norm Normalized time [0, 1].
         * @param animationDuration The total duration of the animation cycle in seconds.
         * @return The speed in world units / sec.
         */
        double GetCurrentWorldSpeed(double t_norm, double animationDuration) const;

    private:
        CubicSpline m_path;
        SpeedControl m_speedControl;

        std::vector<ArcLengthTable> m_segmentTables;
        std::vector<double> m_segmentLengths;
        double m_totalPathLength = 0.0;
    };
}
