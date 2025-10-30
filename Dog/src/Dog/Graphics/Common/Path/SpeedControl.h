#pragma once

namespace Dog
{
    /**
     * @class SpeedControl
     * @brief Implements ease-in, constant, ease-out velocity control
     */
    class SpeedControl
    {
    public:
        SpeedControl() = default;

        /**
         * @brief Sets the time boundaries for easing.
         * @param t_easeIn Time to finish easing in [0, 1].
         * @param t_easeOut Time to start easing out [0, 1].
         */
        void SetProfile(double t_easeIn, double t_easeOut);

        /**
         * @brief Gets normalized distance 's' for a normalized time 't'.
         * @param t_norm Normalized time [0, 1].
         * @return Normalized distance 's' [0, 1].
         */
        double GetNormalizedDistance(double t_norm) const;

        /**
         * @brief Gets normalized velocity 'v' for a normalized time 't'.
         * @param t_norm Normalized time [0, 1].
         * @return Normalized velocity 'v' (derivative of distance w.r.t. time).
         */
        double GetNormalizedVelocity(double t_norm) const;

    private:
        double m_t1 = 0.2; // End of ease-in
        double m_t2 = 0.8; // Start of ease-out

        double m_vMax = 0.0; // Max speed (calculated)
        double m_s1 = 0.0;   // Distance at end of ease-in (calculated)
    };
}
