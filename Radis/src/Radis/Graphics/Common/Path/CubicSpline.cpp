#include <PCH/pch.h>
#include "CubicSpline.h"

namespace Dog
{
    void CubicSpline::SetControlPoints(const std::vector<glm::vec3>& points)
    {
        if (points.size() < 4)
        {
            // Not enough points to make a spline segment
            m_controlPoints.clear();
            return;
        }
        m_controlPoints = points;
    }

    int CubicSpline::GetNumSegments() const
    {
        if (m_controlPoints.size() < 4) return 0;
        return static_cast<int>(m_controlPoints.size()) - 3;
    }

    glm::vec3 CubicSpline::GetPoint(double globalParam) const
    {
        const int numSegments = GetNumSegments();
        if (numSegments == 0) return { 0.0f, 0.0f, 0.0f };

        // Clamp globalParam to the valid range [0, numSegments]
        globalParam = std::max(0.0, std::min(static_cast<double>(numSegments), globalParam));

        int segmentIndex = static_cast<int>(std::floor(globalParam));
        if (segmentIndex >= numSegments)
        {
            // Handle edge case: globalParam is exactly numSegments
            segmentIndex = numSegments - 1;
        }
        const double t = globalParam - segmentIndex;

        return GetPointOnSegment(segmentIndex, t);
    }

    glm::vec3 CubicSpline::GetTangent(double globalParam) const
    {
        const int numSegments = GetNumSegments();
        if (numSegments == 0) return { 0.0f, 0.0f, 0.0f };

        globalParam = std::max(0.0, std::min(static_cast<double>(numSegments), globalParam));

        int segmentIndex = static_cast<int>(std::floor(globalParam));
        if (segmentIndex >= numSegments)
        {
            segmentIndex = numSegments - 1;
        }
        const double t = globalParam - segmentIndex;

        return GetTangentOnSegment(segmentIndex, t);
    }

    glm::vec3 CubicSpline::GetPointOnSegment(int i, double t) const
    {
        if (i < 0 || i >= GetNumSegments())
        {
            throw std::out_of_range("Spline segment index out of range.");
        }

        const glm::vec3& P0 = m_controlPoints[i];
        const glm::vec3& P1 = m_controlPoints[i + 1];
        const glm::vec3& P2 = m_controlPoints[i + 2];
        const glm::vec3& P3 = m_controlPoints[i + 3];

        const double t2 = t * t;
        const double t3 = t2 * t;

        // Catmull-Rom spline formula (tau = 0.5)
        glm::vec3 a = P1 * 2.0f;
        glm::vec3 b = (P2 - P0) * static_cast<float>(t);
        glm::vec3 c = (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * static_cast<float>(t2);
        glm::vec3 d = (-P0 + P1 * 3.0f - P2 * 3.0f + P3) * static_cast<float>(t3);

        return (a + b + c + d) * 0.5f;
    }

    glm::vec3 CubicSpline::GetTangentOnSegment(int i, double t) const
    {
        if (i < 0 || i >= GetNumSegments())
        {
            throw std::out_of_range("Spline segment index out of range.");
        }

        const glm::vec3& P0 = m_controlPoints[i];
        const glm::vec3& P1 = m_controlPoints[i + 1];
        const glm::vec3& P2 = m_controlPoints[i + 2];
        const glm::vec3& P3 = m_controlPoints[i + 3];

        const double t2 = t * t;

        // Derivative of the Catmull-Rom formula
        glm::vec3 b = (P2 - P0);
        glm::vec3 c = (P0 * 2.0f - P1 * 5.0f + P2 * 4.0f - P3) * 2.0f * static_cast<float>(t);
        glm::vec3 d = (-P0 + P1 * 3.0f - P2 * 3.0f + P3) * 3.0f * static_cast<float>(t2);

        return (b + c + d) * 0.5f;
    }
}
