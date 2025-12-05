#include <PCH/pch.h>
#include "SpeedControl.h"

namespace Radis
{
    void SpeedControl::SetProfile(double t_easeIn, double t_easeOut)
    {
        if (t_easeIn < 0.0 || t_easeOut > 1.0 || t_easeIn >= t_easeOut)
        {
            DOG_CRITICAL("Invalid ease-in/out times: t_easeIn = {}, t_easeOut = {}", t_easeIn, t_easeOut);
            throw std::invalid_argument("Invalid ease-in/out times.");
        }

        m_t1 = t_easeIn;
        m_t2 = t_easeOut;

        m_vMax = 2.0 / (1.0 + m_t2 - m_t1);
        m_s1 = 0.5 * m_vMax * m_t1;
    }

    double SpeedControl::GetNormalizedDistance(double t_norm) const
    {
        t_norm = std::max(0.0, std::min(1.0, t_norm));

        // Ease-in phase [0, t1]
        if (t_norm < m_t1)
        {
            // s(t) = 0.5 * a * t^2
            const double a = m_vMax / m_t1;
            return 0.5 * a * t_norm * t_norm;
        }
        // Constant speed phase [t1, t2]
        else if (t_norm < m_t2)
        {
            // s(t) = s1 + vMax * (t - t1)
            return m_s1 + (t_norm - m_t1) * m_vMax;
        }
        // Ease-out phase [t2, 1]
        else
        {
            // t_rem = (1.0 - t_norm)
            // s_rem = 0.5 * a * t_rem^2
            // s(t) = 1.0 - s_rem
            const double t_rem = 1.0 - t_norm;
            const double a = m_vMax / (1.0 - m_t2); // deceleration
            return 1.0 - (0.5 * a * t_rem * t_rem);
        }
    }

    double SpeedControl::GetNormalizedVelocity(double t_norm) const
    {
        t_norm = std::max(0.0, std::min(1.0, t_norm));

        // Ease-in phase [0, t1]
        if (t_norm < m_t1)
        {
            // v(t) = a * t (where a = vMax / t1)
            const double a = m_vMax / m_t1;
            return a * t_norm;
        }
        // Constant speed phase [t1, t2]
        else if (t_norm < m_t2)
        {
            // v(t) = vMax
            return m_vMax;
        }
        // Ease-out phase [t2, 1]
        else
        {
            // v(t) = vMax - a * (t - t2)
            const double t_from_ease = t_norm - m_t2;
            const double a = m_vMax / (1.0 - m_t2); // deceleration
            return std::max(0.0, m_vMax - a * t_from_ease);
        }
    }
}
