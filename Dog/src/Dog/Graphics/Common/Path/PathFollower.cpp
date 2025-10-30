#include <PCH/pch.h>
#include "PathFollower.h"

namespace Dog
{
    void PathFollower::BuildPath(const std::vector<glm::vec3>& controlPoints,
        double arcLengthEpsilon,
        double arcLengthDelta)
    {
        m_path.SetControlPoints(controlPoints);

        const int numSegments = m_path.GetNumSegments();
        m_segmentTables.resize(numSegments);
        m_segmentLengths.resize(numSegments);
        m_totalPathLength = 0.0;

        for (int i = 0; i < numSegments; ++i)
        {
            m_segmentTables[i].Build(m_path, i, arcLengthEpsilon, arcLengthDelta);

            const double segLen = m_segmentTables[i].GetTotalLength();
            m_segmentLengths[i] = segLen;
            m_totalPathLength += segLen;
        }

        // Set a default speed profile
        SetSpeedProfile(0.2, 0.8);
    }

    void PathFollower::SetSpeedProfile(double easeInTime, double easeOutTime)
    {
        m_speedControl.SetProfile(easeInTime, easeOutTime);
    }

    PathSample PathFollower::GetSampleAtTime(double t_norm) const
    {
        // Convert normalized time -> normalized *eased* distance
        const double s_norm_global = m_speedControl.GetNormalizedDistance(t_norm);

        // Convert normalized global distance -> actual global distance
        const double s_target = s_norm_global * m_totalPathLength;

        // Find which segment this distance is in
        double s_accum = 0.0;
        const int numSegments = m_path.GetNumSegments();
        for (int i = 0; i < numSegments; ++i)
        {
            const double segLen = m_segmentLengths[i];

            // Check if the target distance is within this segment
            // The "|| i == numSegments - 1" handles the very end of the path
            if (s_target <= s_accum + segLen || i == numSegments - 1)
            {
                const double s_local = s_target - s_accum;

                double s_local_norm = (segLen > 1e-8) ? (s_local / segLen) : 0.0;
                s_local_norm = std::max(0.0, std::min(1.0, s_local_norm));

                const double u_local = m_segmentTables[i].GetParameterForNormalizedLength(s_local_norm);

                return {
                    m_path.GetPointOnSegment(i, u_local),
                    glm::normalize(m_path.GetTangentOnSegment(i, u_local))
                };
            }

            // Move to the next segment
            s_accum += segLen;
        }

        // Fallback for an empty path
        return { m_path.GetPoint(0.0), glm::vec3(0, 0, 1) };
    }

    glm::mat4 PathFollower::GetTransformAtTime(double t_norm, const glm::vec3& worldUp) const
    {
        PathSample sample = GetSampleAtTime(t_norm);

        const glm::vec3& position = sample.position;
        const glm::vec3 F = glm::normalize(sample.tangent); // Forward
        const glm::vec3 R = glm::normalize(glm::cross(worldUp, F)); // Right
        const glm::vec3 U = glm::cross(F, R); // Up (re-orthogonalized)

        return glm::mat4(
            glm::vec4(R, 0.0f),
            glm::vec4(U, 0.0f),
            glm::vec4(F, 0.0f),
            glm::vec4(position, 1.0f)
        );
    }

    double PathFollower::GetCurrentWorldSpeed(double t_norm, double animationDuration) const
    {
        if (animationDuration < 1e-8)
        {
            return 0.0;
        }

        const double v_norm = m_speedControl.GetNormalizedVelocity(t_norm);
        return v_norm * (m_totalPathLength / animationDuration);
    }
}
