#pragma once

namespace Dog
{
    /**
     * @class CubicSpline
     * @brief Implements a C1-continuous spline.
     *
     * This class generates a 3D path from a series of control points.
     * It requires at least 4 control points to generate any segments.
     */
    class CubicSpline
    {
    public:
        CubicSpline() = default;

        /**
         * @brief Sets the control points for the spline.
         * @param points A vector of glm::vec3 control points.
         */
        void SetControlPoints(const std::vector<glm::vec3>& points);

        /**
         * @brief Gets a position on the spline.
         * @param globalParam A parameter from [0, GetNumSegments()].
         * @return The 3D position (glm::vec3) on the curve.
         */
        glm::vec3 GetPoint(double globalParam) const;

        /**
         * @brief Gets the tangent (derivative) on the spline.
         * @param globalParam A parameter from [0, GetNumSegments()].
         * @return The 3D tangent vector (glm::vec3). Not normalized.
         */
        glm::vec3 GetTangent(double globalParam) const;

        /**
         * @brief Gets a position within a specific spline segment.
         * @param segmentIndex The index of the segment [0, GetNumSegments()-1].
         * @param t The local parameter within the segment [0, 1].
         * @return The 3D position (glm::vec3).
         */
        glm::vec3 GetPointOnSegment(int segmentIndex, double t) const;

        /**
         * @brief Gets the tangent within a specific spline segment.
         * @param segmentIndex The index of the segment [0, GetNumSegments()-1].
         * @param t The local parameter within the segment [0, 1].
         * @return The 3D tangent vector (glm::vec3). Not normalized.
         */
        glm::vec3 GetTangentOnSegment(int segmentIndex, double t) const;

        /**
         * @return The number of drawable segments (requires 4 points for 1 segment).
         */
        int GetNumSegments() const;

        /**
         * @return A const reference to the control point list.
         */
        const std::vector<glm::vec3>& GetControlPoints() const
        {
            return m_controlPoints;
        }

    private:
        std::vector<glm::vec3> m_controlPoints;
    };
}
