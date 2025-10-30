#include <PCH/pch.h>
#include "ArcLengthTable.h"

namespace Dog
{
    void ArcLengthTable::Build(const CubicSpline& spline, int segIdx, double epsilon, double delta)
    {
        m_table.clear();

        // Use a map to store the {u, s} pairs. 'u' is the key, ensuring
        // entries are sorted and unique by parameter.
        std::map<double, double> tempTable;
        tempTable[0.0] = 0.0; // G(u_a) = 0 at the start of the segment

        std::stack<std::pair<double, double>> segmentsToProcess;
        segmentsToProcess.push({ 0.0, 1.0 });

        while (!segmentsToProcess.empty())
        {
            // Get the next segment to process
            const std::pair<double, double> segment = segmentsToProcess.top();
            segmentsToProcess.pop();

            const double u_a = segment.first;
            const double u_b = segment.second;

            // (1) Find midpoint
            const double u_m = (u_a + u_b) / 2.0;

            // Get points
            const glm::vec3 P_a = spline.GetPointOnSegment(segIdx, u_a);
            const glm::vec3 P_m = spline.GetPointOnSegment(segIdx, u_m);
            const glm::vec3 P_b = spline.GetPointOnSegment(segIdx, u_b);

            // (2) Calculate lengths
            const double A = static_cast<double>(glm::length(P_m - P_a));
            const double B = static_cast<double>(glm::length(P_b - P_m));
            const double C = static_cast<double>(glm::length(P_b - P_a));
            const double d = A + B - C; // Arc length error

            // (3) Check subdivision condition (error 'd' > epsilon OR interval > delta)
            if (d > epsilon || (u_b - u_a) > delta)
            {
                // "replace [ua, ub] with [ua, um] and [um, ub]"
                segmentsToProcess.push({ u_m, u_b });
                segmentsToProcess.push({ u_a, u_m });
            }
            // (4) Otherwise: record the lengths (base case)
            else
            {
                // "sm = G(ua) + A"
                const double s_a = tempTable.at(u_a);
                const double s_m = s_a + A;

                // "sb = G(um) + B"
                // We use G(um) = sm, as G(um) isn't in the table yet.
                const double s_b = s_m + B;

                // "record <um, sm> and <ub, sb> in the table"
                tempTable[u_m] = s_m;
                tempTable[u_b] = s_b;
            }
        }

        // Copy the ordered map into the vector for fast binary search
        m_table.reserve(tempTable.size());
        for (const auto& pair : tempTable)
        {
            m_table.push_back({ pair.first, pair.second });
        }

        if (m_table.empty())
        {
            m_totalLength = 0.0;
            return;
        }

        // Normalize all 's' values to be [0, 1]
        m_totalLength = m_table.back().s;
        if (m_totalLength > 1e-8)
        {
            for (auto& entry : m_table)
            {
                entry.s /= m_totalLength;
            }
        }
    }

    double ArcLengthTable::GetParameterForNormalizedLength(double s_norm) const
    {
        if (m_table.empty()) return 0.0;

        // Clamp input
        s_norm = std::max(0.0, std::min(1.0, s_norm));

        // Binary search for the first entry with s >= s_norm
        auto it = std::lower_bound(m_table.begin(), m_table.end(), s_norm);

        if (it == m_table.begin()) return 0.0;
        if (it == m_table.end()) return 1.0;

        const TableEntry& high = *it;
        const TableEntry& low = *(--it);

        const double s_low = low.s;
        const double s_high = high.s;
        const double u_low = low.u;
        const double u_high = high.u;

        const double range = s_high - s_low;
        if (range < 1e-8)
        {
            return u_low;
        }

        // Find how far 's_norm' is into this 's' range
        const double t_interp = (s_norm - s_low) / range;

        // Apply that to the 'u' range
        return u_low + (u_high - u_low) * t_interp;
    }
}

