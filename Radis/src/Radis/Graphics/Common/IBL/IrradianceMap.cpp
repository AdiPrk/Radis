#include <PCH/pch.h>
#include "IrradianceMap.h"
#include "rgbe.h"

#ifdef _OPENMP
#   include <omp.h>
#endif

namespace Radis::IBL {

    // ─────────────────────────────────────────────────────────────────────────────
    // Internal constants
    // ─────────────────────────────────────────────────────────────────────────────

    static constexpr double kPI = 3.14159265358979323846;
    static constexpr double kTwoPI = 2.0 * kPI;

    // SH basis normalisation constants — exact closed forms, double precision.
    // Index order: Y00, Y1-1, Y10, Y11, Y2-2, Y2-1, Y20, Y21, Y22
    static constexpr double kBasisConst[9] = {
        0.28209479177387814,   // 0.5  * sqrt(1  / π)
        0.48860251190291992,   // 0.5  * sqrt(3  / π)
        0.48860251190291992,
        0.48860251190291992,
        1.09254843059207907,   // 0.5  * sqrt(15 / π)
        1.09254843059207907,
        0.31539156525252000,   // 0.25 * sqrt(5  / π)
        1.09254843059207907,
        0.54627421529603959,   // 0.25 * sqrt(15 / π)
    };

    // Â_l clamped-cosine factors (PDF §The Plan, Step 1).
    // Baked into the projection so evalSH() needs no extra multiply.
    //   band 0 (l=0): Â0 = π
    //   band 1 (l=1): Â1 = 2π/3
    //   band 2 (l=2): Â2 = π/4
    static constexpr double kAhat[9] = {
        kPI,                  // l=0
        kTwoPI / 3.0,         // l=1
        kTwoPI / 3.0,
        kTwoPI / 3.0,
        kPI / 4.0,            // l=2
        kPI / 4.0,
        kPI / 4.0,
        kPI / 4.0,
        kPI / 4.0,
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // SH basis evaluation
    // ─────────────────────────────────────────────────────────────────────────────

    /// Evaluate all 9 SH basis functions at direction (x, y, z) → out[9].
    /// (x,y,z) must be a unit vector in the Z-up convention.
    static inline void evalBasis(double x, double y, double z,
        double out[9]) noexcept
    {
        out[0] = kBasisConst[0];
        out[1] = kBasisConst[1] * y;
        out[2] = kBasisConst[2] * z;
        out[3] = kBasisConst[3] * x;
        out[4] = kBasisConst[4] * x * y;
        out[5] = kBasisConst[5] * y * z;
        out[6] = kBasisConst[6] * (3.0 * z * z - 1.0);
        out[7] = kBasisConst[7] * x * z;
        out[8] = kBasisConst[8] * (x * x - y * y);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // HDR image — thin RAII wrapper around the rgbe I/O library
    // ─────────────────────────────────────────────────────────────────────────────

    struct HDRImage
    {
        std::vector<float> pixels; // interleaved RGB, row-major, linear light
        int width = 0;
        int height = 0;

        [[nodiscard]] const float* row(int j) const noexcept
        {
            return pixels.data() + static_cast<std::ptrdiff_t>(j) * width * 3;
        }
    };

    static bool loadHDR(const char* path, HDRImage& img)
    {
        FILE* fp = std::fopen(path, "rb");
        if (!fp) return false;

        int w = 0, h = 0;
        if (RGBE_ReadHeader(fp, &w, &h, nullptr) != RGBE_RETURN_SUCCESS) {
            std::fclose(fp);
            return false;
        }

        img.width = w;
        img.height = h;
        img.pixels.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);

        const bool ok =
            RGBE_ReadPixels_RLE(fp, img.pixels.data(), w, h) == RGBE_RETURN_SUCCESS;

        std::fclose(fp);
        return ok;
    }

    static bool saveHDR(const char* path, float* pixels, int w, int h)
    {
        FILE* fp = std::fopen(path, "wb");
        if (!fp) return false;

        const bool ok =
            RGBE_WriteHeader(fp, w, h, nullptr) == RGBE_RETURN_SUCCESS &&
            RGBE_WritePixels_RLE(fp, pixels, w, h) == RGBE_RETURN_SUCCESS;

        std::fclose(fp);
        return ok;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // SHCoefficients
    // ─────────────────────────────────────────────────────────────────────────────

    glm::vec3 SHCoefficients::eval(const glm::vec3& N) const noexcept
    {
        double basis[9];
        evalBasis(
            static_cast<double>(N.x),
            static_cast<double>(N.y),
            static_cast<double>(N.z),
            basis
        );

        double r = 0.0, g = 0.0, b = 0.0;
        for (int k = 0; k < 9; ++k) {
            r += static_cast<double>(E[k].r) * basis[k];
            g += static_cast<double>(E[k].g) * basis[k];
            b += static_cast<double>(E[k].b) * basis[k];
        }

        return {
            static_cast<float>(r),
            static_cast<float>(g),
            static_cast<float>(b),
        };
    }

    bool SHCoefficients::empty() const noexcept
    {
        for (const auto& c : E)
            if (c.r != 0.0f || c.g != 0.0f || c.b != 0.0f)
                return false;
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // projectToSH — core projection loop
    // ─────────────────────────────────────────────────────────────────────────────

    bool projectToSH(const std::string& hdrPath, SHCoefficients& outSH)
    {
        HDRImage img;
        if (!loadHDR(hdrPath.c_str(), img)) return false;

        const int W = img.width;
        const int H = img.height;

        // Step sizes for the Riemann sum approximation (PDF §Step 2).
        const double dTheta = kPI / static_cast<double>(H);
        const double dPhi = kTwoPI / static_cast<double>(W);

        // Accumulate into double-precision per-channel arrays.
        // With OpenMP each thread owns a private copy; they reduce under a critical.
        double acc[9][3] = {};  // [coefficient][RGB]

#ifdef _OPENMP
#pragma omp parallel
        {
            double local[9][3] = {};

            // Dynamic scheduling because sinθ weight causes uneven work per row.
#pragma omp for schedule(dynamic, 8) nowait
            for (int j = 0; j < H; ++j)
            {
                const double theta = kPI * (static_cast<double>(j) + 0.5) / H;
                const double sinT = std::sin(theta);
                const double cosT = std::cos(theta);
                const double wRow = sinT * dTheta * dPhi;   // sin θ Δθ Δφ

                const float* rowPtr = img.row(j);

                for (int i = 0; i < W; ++i)
                {
                    const double phi = kTwoPI * (static_cast<double>(i) + 0.5) / W;
                    const double x = sinT * std::cos(phi);
                    const double y = sinT * std::sin(phi);
                    const double z = cosT;

                    double basis[9];
                    evalBasis(x, y, z, basis);

                    const double r = static_cast<double>(rowPtr[i * 3 + 0]);
                    const double g = static_cast<double>(rowPtr[i * 3 + 1]);
                    const double b = static_cast<double>(rowPtr[i * 3 + 2]);

                    for (int k = 0; k < 9; ++k) {
                        const double bw = basis[k] * wRow;
                        local[k][0] += r * bw;
                        local[k][1] += g * bw;
                        local[k][2] += b * bw;
                    }
                }
            }

#pragma omp critical
            for (int k = 0; k < 9; ++k) {
                acc[k][0] += local[k][0];
                acc[k][1] += local[k][1];
                acc[k][2] += local[k][2];
            }
        }

#else  // single-threaded fallback

        for (int j = 0; j < H; ++j)
        {
            const double theta = kPI * (static_cast<double>(j) + 0.5) / H;
            const double sinT = std::sin(theta);
            const double cosT = std::cos(theta);
            const double wRow = sinT * dTheta * dPhi;

            const float* rowPtr = img.row(j);

            for (int i = 0; i < W; ++i)
            {
                const double phi = kTwoPI * (static_cast<double>(i) + 0.5) / W;
                const double x = sinT * std::cos(phi);
                const double y = sinT * std::sin(phi);
                const double z = cosT;

                double basis[9];
                evalBasis(x, y, z, basis);

                const double r = static_cast<double>(rowPtr[i * 3 + 0]);
                const double g = static_cast<double>(rowPtr[i * 3 + 1]);
                const double b = static_cast<double>(rowPtr[i * 3 + 2]);

                for (int k = 0; k < 9; ++k) {
                    const double bw = basis[k] * wRow;
                    acc[k][0] += r * bw;
                    acc[k][1] += g * bw;
                    acc[k][2] += b * bw;
                }
            }
        }
#endif

        // Elm = Â_l · L_lm (PDF §Final set of SH coefficients)
        for (int k = 0; k < 9; ++k) {
            outSH.E[k] = glm::vec3(
                static_cast<float>(acc[k][0] * kAhat[k]),
                static_cast<float>(acc[k][1] * kAhat[k]),
                static_cast<float>(acc[k][2] * kAhat[k])
            );
        }

        return true;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // bakeToHDR — evaluate SH on a grid and write irradiance map
    // ─────────────────────────────────────────────────────────────────────────────

    bool bakeToHDR(const SHCoefficients& sh,
        const std::string& outPath,
        int outW,
        int outH)
    {
        assert(outW > 0 && outH > 0);

        std::vector<float> pixels(static_cast<size_t>(outW) * outH * 3u);

#ifdef _OPENMP
#pragma omp parallel for schedule(static)
#endif
        for (int j = 0; j < outH; ++j)
        {
            const double theta = kPI * (static_cast<double>(j) + 0.5) / outH;
            const double sinT = std::sin(theta);
            const double cosT = std::cos(theta);

            float* rowPtr = pixels.data() + static_cast<std::ptrdiff_t>(j) * outW * 3;

            for (int i = 0; i < outW; ++i)
            {
                const double phi = kTwoPI * (static_cast<double>(i) + 0.5) / outW;
                const glm::vec3 dir = {
                    static_cast<float>(sinT * std::cos(phi)),
                    static_cast<float>(sinT * std::sin(phi)),
                    static_cast<float>(cosT),
                };

                const glm::vec3 irr = sh.eval(dir);

                // Clamp: SH reconstruction can produce small negatives (ringing).
                rowPtr[i * 3 + 0] = std::max(0.0f, irr.r);
                rowPtr[i * 3 + 1] = std::max(0.0f, irr.g);
                rowPtr[i * 3 + 2] = std::max(0.0f, irr.b);
            }
        }

        return saveHDR(outPath.c_str(), pixels.data(), outW, outH);
    }

    // ─────────────────────────────────────────────────────────────────────────────
    // generateIrradianceMap — convenience wrapper
    // ─────────────────────────────────────────────────────────────────────────────

    bool generateIrradianceMap(const std::string& hdrPath,
        const std::string& outPath,
        SHCoefficients* outSH,
        int outW,
        int outH)
    {
        SHCoefficients sh;
        if (!projectToSH(hdrPath, sh)) return false;
        if (outSH) *outSH = sh;
        return bakeToHDR(sh, outPath, outW, outH);
    }

} // namespace IBL