/*****************************************************************//**
 * \file   IrradianceMap.h
 * \brief  IBL Spherical-harmonic irradiance map generator
 * 
 * \author Aditya Prakash
 * \date   March 2026
 *********************************************************************/

#pragma once

namespace Radis::IBL {

    // ─────────────────────────────────────────────────────────────────────────────
    // SH coefficient set
    // ─────────────────────────────────────────────────────────────────────────────

    /// Nine RGB spherical-harmonic coefficients, bands 0–2.
    ///
    /// After projectToSH() these already incorporate the Â_l clamped-cosine factors
    /// (Â0=π, Â1=2π/3, Â2=π/4), so irradiance is a plain dot product with the
    /// basis — no further scaling needed at runtime.
    ///
    /// GPU usage: upload E[9] as a UBO (vec3 array, 9 × 16 bytes with std140 padding)
    /// and evaluate with evalSH() in the fragment shader (see bottom of this header).
    struct SHCoefficients
    {
        std::array<glm::vec3, 9> E{};

        /// Evaluate irradiance at unit normal N (Z-up convention, see above).
        [[nodiscard]] glm::vec3 eval(const glm::vec3& N) const noexcept;

        /// True iff all coefficients are zero (i.e. default-constructed / load failed).
        [[nodiscard]] bool empty() const noexcept;
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // Main API
    // ─────────────────────────────────────────────────────────────────────────────

    /// Project an equirectangular RGBE (.hdr) environment map to SH coefficients.
    ///
    /// Internally single-pass with OpenMP row-level parallelism.
    /// Accumulation uses double precision to suppress floating-point error across
    /// multi-megapixel images.
    ///
    /// @param hdrPath   Input equirectangular RGBE file.
    /// @param outSH     Receives the 9 Elm = Â_l · L_lm coefficients (RGB each).
    /// @returns         true on success; false on I/O or format error.
    [[nodiscard]] bool projectToSH(const std::string& hdrPath,
        SHCoefficients& outSH);

    /// Evaluate SH coefficients over a spherical grid and write an RGBE irradiance map.
    ///
    /// The output resolution is intentionally tiny (default 400×200) — the SH
    /// representation has at most 9 degrees of freedom, so there is no benefit to a
    /// larger image.  The file can be loaded and sampled like any other HDR env map.
    ///
    /// @param sh        Coefficients from projectToSH().
    /// @param outPath   Destination .hdr file path.
    /// @param outW      Output width  (default 400).
    /// @param outH      Output height (default 200).
    /// @returns         true on success.
    [[nodiscard]] bool bakeToHDR(const SHCoefficients& sh,
        const std::string& outPath,
        int outW = 400,
        int outH = 200);

    /// Convenience — project and bake in one call.
    ///
    /// @param outSH     Optional; receives the intermediate SH coefficients so the
    ///                  caller can also upload them directly to the GPU.
    [[nodiscard]] bool generateIrradianceMap(const std::string& hdrPath,
        const std::string& outPath,
        SHCoefficients* outSH = nullptr,
        int outW = 400,
        int outH = 200);

} // namespace IBL

// ─────────────────────────────────────────────────────────────────────────────
// GLSL reference (paste into your lighting fragment shader)
// ─────────────────────────────────────────────────────────────────────────────
//
//  layout(set=0, binding=N, std140) uniform IrradianceBlock {
//      vec3 E[9];          // SHCoefficients::E[], each padded to vec4 by std140
//  } irr;
//
//  vec3 evalIrradiance(vec3 N)  // N must be Z-up unit vector
//  {
//      return irr.E[0] * 0.28209479f
//           + irr.E[1] * 0.48860251f * N.y
//           + irr.E[2] * 0.48860251f * N.z
//           + irr.E[3] * 0.48860251f * N.x
//           + irr.E[4] * 1.09254843f * N.x * N.y
//           + irr.E[5] * 1.09254843f * N.y * N.z
//           + irr.E[6] * 0.31539157f * (3.0f * N.z * N.z - 1.0f)
//           + irr.E[7] * 1.09254843f * N.x * N.z
//           + irr.E[8] * 0.54627422f * (N.x * N.x - N.y * N.y);
//  }
//
//  // In computePBRLight / IBL diffuse term:
//  vec3 irradiance = evalIrradiance(N);           // or texture(irradianceMap, uvOf(N)).rgb
//  vec3 iblDiffuse = kD * albedo * INV_PI * irradiance * ao;