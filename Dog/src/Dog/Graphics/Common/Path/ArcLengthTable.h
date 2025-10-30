#pragma once

#include "CubicSpline.h"

namespace Dog
{
    /**
     * @class ArcLengthTable
     * @brief Builds a normalized arc-length lookup table for a single spline segment.
     */
    class ArcLengthTable
    {
    public:
        ArcLengthTable() = default;

        /**
         * @brief Builds the lookup table for a specific segment of a spline.
         * @param spline The spline containing the segment.
         * @param segmentIndex The index of the segment to analyze.
         * @param epsilon The arc length error threshold (e).
         * @param delta The maximum parameter interval (d).
         */
        void Build(const CubicSpline& spline, int segmentIndex, double epsilon, double delta);

        /**
         * @brief Gets the parameter 'u' for a normalized arc length 's_norm'.
         * @param s_norm A normalized arc length [0, 1].
         * @return The corresponding curve parameter 'u' [0, 1].
         */
        double GetParameterForNormalizedLength(double s_norm) const;

        /**
         * @return The total arc length of this segment.
         */
        double GetTotalLength() const { return m_totalLength; }

    private:
        /**
         * @struct TableEntry
         * @brief A single entry in the lookup table, mapping 'u' to 's'.
         */
        struct TableEntry
        {
            double u; // Parameter [0, 1]
            double s; // Normalized arc length [0, 1]

            // Comparison operator for std::lower_bound
            bool operator<(double s_val) const { return s < s_val; }
        };

        std::vector<TableEntry> m_table;
        double m_totalLength = 0.0;
    };
}
