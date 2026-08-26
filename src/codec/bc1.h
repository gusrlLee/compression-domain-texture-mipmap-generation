#pragma once

// ============================================================================
// [Cross-Platform Polyglot Macro]
// ============================================================================

#ifdef __SLANG__
// ----------------------------------------------------
// 1. GPU (Slang/HLSL) Environment
// ----------------------------------------------------
#define INOUT(type) inout type
#define OUT(type) out type
#define SHARED_INLINE
#else
// ----------------------------------------------------
// 2. CPU (C++) Environment
// ----------------------------------------------------
#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>

typedef uint32_t uint;
typedef uint16_t ushort;

#define INOUT(type) type &
#define OUT(type) type &
#define SHARED_INLINE inline

struct uint4
{
    uint x, y, z, w;
    SHARED_INLINE uint4() : x(0), y(0), z(0), w(0) {}
    SHARED_INLINE uint4(uint fill) : x(fill), y(fill), z(fill), w(fill) {}
    SHARED_INLINE uint4(uint _x, uint _y, uint _z, uint _w) : x(_x), y(_y), z(_z), w(_w) {}

    SHARED_INLINE uint4 operator&(const uint4 &o) const { return uint4(x & o.x, y & o.y, z & o.z, w & o.w); }
    SHARED_INLINE uint4 operator|(const uint4 &o) const { return uint4(x | o.x, y | o.y, z | o.z, w | o.w); }
    SHARED_INLINE uint4 operator^(const uint4 &o) const { return uint4(x ^ o.x, y ^ o.y, z ^ o.z, w ^ o.w); }
    SHARED_INLINE uint4 operator~() const { return uint4(~x, ~y, ~z, ~w); }
    SHARED_INLINE uint4 operator<<(int s) const { return uint4(x << s, y << s, z << s, w << s); }
    SHARED_INLINE uint4 operator>>(int s) const { return uint4(x >> s, y >> s, z >> s, w >> s); }
    SHARED_INLINE uint4 operator+(const uint4 &o) const { return uint4(x + o.x, y + o.y, z + o.z, w + o.w); }
    SHARED_INLINE uint4 operator-(const uint4 &o) const { return uint4(x - o.x, y - o.y, z - o.z, w - o.w); }
    SHARED_INLINE uint4 operator*(uint s) const { return uint4(x * s, y * s, z * s, w * s); }
    SHARED_INLINE uint4 operator/(uint s) const { return uint4(x / s, y / s, z / s, w / s); }
};

struct float4
{
    float x, y, z, w;

    SHARED_INLINE float4() : x(0), y(0), z(0), w(0) {}
    SHARED_INLINE float4(float fill) : x(fill), y(fill), z(fill), w(fill) {}
    SHARED_INLINE float4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

    SHARED_INLINE float4 operator+(const float4 &o) const { return float4(x + o.x, y + o.y, z + o.z, w + o.w); }
    SHARED_INLINE float4 operator-(const float4 &o) const { return float4(x - o.x, y - o.y, z - o.z, w - o.w); }
    SHARED_INLINE float4 operator*(const float4 &o) const { return float4(x * o.x, y * o.y, z * o.z, w * o.w); }
    SHARED_INLINE float4 operator/(const float4 &o) const { return float4(x / o.x, y / o.y, z / o.z, w / o.w); }
    SHARED_INLINE float4 operator*(float s) const { return float4(x * s, y * s, z * s, w * s); }
    SHARED_INLINE float4 operator/(float s) const { return float4(x / s, y / s, z / s, w / s); }
};

SHARED_INLINE float SrgbToLinear(float srgb) noexcept
{
    constexpr float kSrgbThreshold = 0.04045f;
    if (srgb <= kSrgbThreshold)
        return srgb / 12.92f;
    return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
}

inline const std::array<float, 256> kSrgb8ToLinear = []
{
    std::array<float, 256> lookup{};
    constexpr float kInverseChannelMaximum = 1.0f / 255.0f;
    for (uint32_t value = 0; value < lookup.size(); ++value)
    {
        lookup[value] = SrgbToLinear(static_cast<float>(value) * kInverseChannelMaximum);
    }
    return lookup;
}();

#endif

// ============================================================================
// Data Structures
// ============================================================================
struct SymbolicDataBC1
{
    uint16_t color_0;
    uint16_t color_1;
    uint selectors;
};

// Four independent BC1 blocks processed in parallel. Each vector lane owns one block.
struct SymbolicDataBC1x4
{
    uint4 color_0;
    uint4 color_1;
    uint4 selectors;
};

// Structure to hold the RGB components of 4 child texels
struct QuadrantMeans
{
    float4 q0_r, q0_g, q0_b;
    float4 q1_r, q1_g, q1_b;
    float4 q2_r, q2_g, q2_b;
    float4 q3_r, q3_g, q3_b;
};

// Structure to hold the symmetric 3x3 covariance matrix components
struct CovarianceMatrix
{
    float4 rr, gg, bb;
    float4 rg, rb, gb;
};

struct LinearPaletteBC1
{
    float4 c0_r, c0_g, c0_b;
    float4 c1_r, c1_g, c1_b;
    float4 c2_r, c2_g, c2_b;
    float4 c3_r, c3_g, c3_b;
};

struct ParentStatistics
{
    float4 mean_r, mean_g, mean_b;
    CovarianceMatrix within_covariance;
};

struct ProjectionContext
{
    float4 axis_r, axis_g, axis_b;
    float4 mean_r, mean_g, mean_b;
};

struct LeastSquaresContext
{
    float4 p0_r, p0_g, p0_b;
    float4 direction_r, direction_g, direction_b;
    float4 inverse_length_squared;
};

struct LeastSquaresAccumulator
{
    float4 weight_sum;
    float4 weight_squared_sum;
    float4 weighted_r, weighted_g, weighted_b;
};

// ============================================================================
// Cross-platform vector helpers.
// ============================================================================
SHARED_INLINE float4 Max(float4 a, float4 b)
{
#ifndef __SLANG__
    return float4(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));
#else
    return max(a, b);
#endif
}

SHARED_INLINE float4 Min(float4 a, float4 b)
{
#ifndef __SLANG__
    return float4(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));
#else
    return min(a, b);
#endif
}

SHARED_INLINE float4 Rsqrt(float4 v)
{
#ifndef __SLANG__
    return float4(1.0f / std::sqrt(v.x), 1.0f / std::sqrt(v.y), 1.0f / std::sqrt(v.z), 1.0f / std::sqrt(v.w));
#else
    return rsqrt(v);
#endif
}

SHARED_INLINE float4 Round(float4 v)
{
#ifndef __SLANG__
    return float4(std::round(v.x), std::round(v.y), std::round(v.z), std::round(v.w));
#else
    return round(v);
#endif
}

SHARED_INLINE float4 Clamp(float4 v, float min_val, float max_val)
{
#ifndef __SLANG__
    return float4(std::clamp(v.x, min_val, max_val), std::clamp(v.y, min_val, max_val),
                  std::clamp(v.z, min_val, max_val), std::clamp(v.w, min_val, max_val));
#else
    return clamp(v, min_val, max_val);
#endif
}

SHARED_INLINE float4 SelectLtZero(float4 cond, float4 true_val, float4 false_val)
{
#ifndef __SLANG__
    return float4(
        cond.x < 0.0f ? true_val.x : false_val.x,
        cond.y < 0.0f ? true_val.y : false_val.y,
        cond.z < 0.0f ? true_val.z : false_val.z,
        cond.w < 0.0f ? true_val.w : false_val.w);
#else
    return select(cond < 0.0f, true_val, false_val);
#endif
}

// ============================================================================
// Cross-platform conversion and selection helpers.
// ============================================================================
SHARED_INLINE uint4 Float4ToUint4(float4 v)
{
#ifndef __SLANG__
    return uint4(static_cast<uint>(v.x), static_cast<uint>(v.y),
                 static_cast<uint>(v.z), static_cast<uint>(v.w));
#else
    return uint4(v);
#endif
}

// Selects true_val where cond is negative.
SHARED_INLINE uint4 SelectLtZeroUint(float4 cond, uint4 true_val, uint4 false_val)
{
#ifndef __SLANG__
    return uint4(
        cond.x < 0.0f ? true_val.x : false_val.x,
        cond.y < 0.0f ? true_val.y : false_val.y,
        cond.z < 0.0f ? true_val.z : false_val.z,
        cond.w < 0.0f ? true_val.w : false_val.w);
#else
    return select(cond < 0.0f, true_val, false_val);
#endif
}

// Returns the index of the smallest distance in each lane.
SHARED_INLINE uint4 FindBestSelector(float4 d0, float4 d1, float4 d2, float4 d3)
{
    float4 cond10 = d1 - d0;
    uint4 idx01 = SelectLtZeroUint(cond10, uint4(1), uint4(0));
    float4 min01 = SelectLtZero(cond10, d1, d0);

    float4 cond32 = d3 - d2;
    uint4 idx23 = SelectLtZeroUint(cond32, uint4(3), uint4(2));
    float4 min23 = SelectLtZero(cond32, d3, d2);

    float4 cond_final = min23 - min01;
    return SelectLtZeroUint(cond_final, idx23, idx01);
}

// ============================================================================
// Core Math Operations for Compression Domain
// ============================================================================

// SWAR Helper: Counts set bits in 2x2 quadrant regions in parallel (SIMD friendly)
SHARED_INLINE uint4 Count2x2Regions(uint4 flags)
{
    // Step 1: Sum adjacent horizontal pixels (X-axis)
    uint4 x1 = (flags & uint4(0x11111111u)) + ((flags >> 2) & uint4(0x11111111u));

    // Step 2: Sum adjacent vertical pixels (Y-axis) to complete the 2x2 quadrants
    uint4 x2 = (x1 & uint4(0x00FF00FFu)) + ((x1 >> 8) & uint4(0x00FF00FFu));

    // Result layout in the 32-bit integer for each lane:
    // Bits [0~3]   : Quadrant 0 count
    // Bits [4~7]   : Quadrant 1 count
    // Bits [16~19] : Quadrant 2 count
    // Bits [20~23] : Quadrant 3 count
    return x2;
}

// [Theorem 2] Extracts 2x2 quadrant histograms for 4 blocks simultaneously
// Replaces the old ExtractHistogramSWAR placeholder.
SHARED_INLINE void Extract2x2SelectorHistograms(
    uint4 packed_indices,
    OUT(uint4) hist_00, OUT(uint4) hist_01,
    OUT(uint4) hist_10, OUT(uint4) hist_11)
{
    uint4 kLowBitMask = uint4(0x55555555u);

    // Separate low and high bits of the 2-bit selectors
    uint4 low_bits = packed_indices & kLowBitMask;
    uint4 high_bits = (packed_indices >> 1) & kLowBitMask;

    // Isolate locations where specific selectors (00, 01, 10, 11) are used
    uint4 flag_00 = (~low_bits & ~high_bits) & kLowBitMask;
    uint4 flag_01 = (low_bits & ~high_bits) & kLowBitMask;
    uint4 flag_10 = (~low_bits & high_bits) & kLowBitMask;
    uint4 flag_11 = (low_bits & high_bits) & kLowBitMask;

    // Apply the SWAR 2x2 region counter to get the quadrant histograms
    // GPU(Slang) and CPU(C++) will process 4 blocks simultaneously here!
    hist_00 = Count2x2Regions(flag_00);
    hist_01 = Count2x2Regions(flag_01);
    hist_10 = Count2x2Regions(flag_10);
    hist_11 = Count2x2Regions(flag_11);
}

// ============================================================================
// sRGB -> Linear
// ============================================================================
SHARED_INLINE void DecodeRgb565(uint4 packed, OUT(uint4) r, OUT(uint4) g, OUT(uint4) b)
{
    uint4 r5 = (packed >> 11) & uint4(0x1F);
    uint4 g6 = (packed >> 5) & uint4(0x3F);
    uint4 b5 = packed & uint4(0x1F);

    r = (r5 << 3) | (r5 >> 2);
    g = (g6 << 2) | (g6 >> 4);
    b = (b5 << 3) | (b5 >> 2);
}

// Step 2 & 3: Linear Palette Constructive and Quadrant Mean Calculation
SHARED_INLINE float4 Uint4ToFloat4(uint4 v)
{
#ifndef __SLANG__
    return float4(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z), static_cast<float>(v.w));
#else
    return float4(v);
#endif
}

SHARED_INLINE void Srgb8ToLinear(uint4 srgb, OUT(float4) linear)
{
#ifndef __SLANG__
    linear.x = kSrgb8ToLinear[srgb.x];
    linear.y = kSrgb8ToLinear[srgb.y];
    linear.z = kSrgb8ToLinear[srgb.z];
    linear.w = kSrgb8ToLinear[srgb.w];
#else
    float4 value = float4(srgb) * (1.0f / 255.0f);
    linear = select(value <= 0.04045f, value / 12.92f, pow((value + 0.055f) / 1.055f, 2.4f));
#endif
}

SHARED_INLINE LinearPaletteBC1 BuildOpaqueLinearPaletteBC1(
    uint4 color_0, uint4 color_1)
{
    uint4 c0_r, c0_g, c0_b;
    uint4 c1_r, c1_g, c1_b;
    DecodeRgb565(color_0, c0_r, c0_g, c0_b);
    DecodeRgb565(color_1, c1_r, c1_g, c1_b);

    uint4 c2_r = (c0_r * 2u + c1_r) / 3u;
    uint4 c2_g = (c0_g * 2u + c1_g) / 3u;
    uint4 c2_b = (c0_b * 2u + c1_b) / 3u;
    uint4 c3_r = (c0_r + c1_r * 2u) / 3u;
    uint4 c3_g = (c0_g + c1_g * 2u) / 3u;
    uint4 c3_b = (c0_b + c1_b * 2u) / 3u;

    LinearPaletteBC1 palette;
    Srgb8ToLinear(c0_r, palette.c0_r);
    Srgb8ToLinear(c0_g, palette.c0_g);
    Srgb8ToLinear(c0_b, palette.c0_b);
    Srgb8ToLinear(c1_r, palette.c1_r);
    Srgb8ToLinear(c1_g, palette.c1_g);
    Srgb8ToLinear(c1_b, palette.c1_b);
    Srgb8ToLinear(c2_r, palette.c2_r);
    Srgb8ToLinear(c2_g, palette.c2_g);
    Srgb8ToLinear(c2_b, palette.c2_b);
    Srgb8ToLinear(c3_r, palette.c3_r);
    Srgb8ToLinear(c3_g, palette.c3_g);
    Srgb8ToLinear(c3_b, palette.c3_b);
    return palette;
}

// Reconstructs the opaque BC1 palette, converts it to linear space, and
// computes the exact linear mean for each 2x2 quadrant.
SHARED_INLINE void ComputeParentQuadrantMeans(
    uint4 color_0, uint4 color_1,
    uint4 hist_00, uint4 hist_01, uint4 hist_10, uint4 hist_11,
    OUT(QuadrantMeans) out_means)
{
    LinearPaletteBC1 palette = BuildOpaqueLinearPaletteBC1(color_0, color_1);

    // 4. Extract Quadrant Selector Counts (n_g,k) and convert to weights (gamma = n / 4)
    // Note: Count2x2Regions stores counts at specific bit offsets

    // Quadrant 0 (Bits 0~3)
    float4 q0_w0 = Uint4ToFloat4(hist_00 & uint4(0xF)) * 0.25f;
    float4 q0_w1 = Uint4ToFloat4(hist_01 & uint4(0xF)) * 0.25f;
    float4 q0_w2 = Uint4ToFloat4(hist_10 & uint4(0xF)) * 0.25f;
    float4 q0_w3 = Uint4ToFloat4(hist_11 & uint4(0xF)) * 0.25f;

    // Quadrant 1 (Bits 4~7)
    float4 q1_w0 = Uint4ToFloat4((hist_00 >> 4) & uint4(0xF)) * 0.25f;
    float4 q1_w1 = Uint4ToFloat4((hist_01 >> 4) & uint4(0xF)) * 0.25f;
    float4 q1_w2 = Uint4ToFloat4((hist_10 >> 4) & uint4(0xF)) * 0.25f;
    float4 q1_w3 = Uint4ToFloat4((hist_11 >> 4) & uint4(0xF)) * 0.25f;

    // Quadrant 2 (Bits 16~19)
    float4 q2_w0 = Uint4ToFloat4((hist_00 >> 16) & uint4(0xF)) * 0.25f;
    float4 q2_w1 = Uint4ToFloat4((hist_01 >> 16) & uint4(0xF)) * 0.25f;
    float4 q2_w2 = Uint4ToFloat4((hist_10 >> 16) & uint4(0xF)) * 0.25f;
    float4 q2_w3 = Uint4ToFloat4((hist_11 >> 16) & uint4(0xF)) * 0.25f;

    // Quadrant 3 (Bits 20~23)
    float4 q3_w0 = Uint4ToFloat4((hist_00 >> 20) & uint4(0xF)) * 0.25f;
    float4 q3_w1 = Uint4ToFloat4((hist_01 >> 20) & uint4(0xF)) * 0.25f;
    float4 q3_w2 = Uint4ToFloat4((hist_10 >> 20) & uint4(0xF)) * 0.25f;
    float4 q3_w3 = Uint4ToFloat4((hist_11 >> 20) & uint4(0xF)) * 0.25f;

    // 5. Compute Exact Linear Color (y_g) for each quadrant
    out_means.q0_r = palette.c0_r * q0_w0 + palette.c1_r * q0_w1 + palette.c2_r * q0_w2 + palette.c3_r * q0_w3;
    out_means.q0_g = palette.c0_g * q0_w0 + palette.c1_g * q0_w1 + palette.c2_g * q0_w2 + palette.c3_g * q0_w3;
    out_means.q0_b = palette.c0_b * q0_w0 + palette.c1_b * q0_w1 + palette.c2_b * q0_w2 + palette.c3_b * q0_w3;

    out_means.q1_r = palette.c0_r * q1_w0 + palette.c1_r * q1_w1 + palette.c2_r * q1_w2 + palette.c3_r * q1_w3;
    out_means.q1_g = palette.c0_g * q1_w0 + palette.c1_g * q1_w1 + palette.c2_g * q1_w2 + palette.c3_g * q1_w3;
    out_means.q1_b = palette.c0_b * q1_w0 + palette.c1_b * q1_w1 + palette.c2_b * q1_w2 + palette.c3_b * q1_w3;

    out_means.q2_r = palette.c0_r * q2_w0 + palette.c1_r * q2_w1 + palette.c2_r * q2_w2 + palette.c3_r * q2_w3;
    out_means.q2_g = palette.c0_g * q2_w0 + palette.c1_g * q2_w1 + palette.c2_g * q2_w2 + palette.c3_g * q2_w3;
    out_means.q2_b = palette.c0_b * q2_w0 + palette.c1_b * q2_w1 + palette.c2_b * q2_w2 + palette.c3_b * q2_w3;

    out_means.q3_r = palette.c0_r * q3_w0 + palette.c1_r * q3_w1 + palette.c2_r * q3_w2 + palette.c3_r * q3_w3;
    out_means.q3_g = palette.c0_g * q3_w0 + palette.c1_g * q3_w1 + palette.c2_g * q3_w2 + palette.c3_g * q3_w3;
    out_means.q3_b = palette.c0_b * q3_w0 + palette.c1_b * q3_w1 + palette.c2_b * q3_w2 + palette.c3_b * q3_w3;
}

// ============================================================================
// Step 3: Moment Aggregation (Mean & Covariance Matrix)
// ============================================================================

SHARED_INLINE ParentStatistics ComputeParentStatistics(QuadrantMeans parent)
{
    ParentStatistics statistics;
    statistics.mean_r = (parent.q0_r + parent.q1_r + parent.q2_r + parent.q3_r) * 0.25f;
    statistics.mean_g = (parent.q0_g + parent.q1_g + parent.q2_g + parent.q3_g) * 0.25f;
    statistics.mean_b = (parent.q0_b + parent.q1_b + parent.q2_b + parent.q3_b) * 0.25f;

    float4 d0_r = parent.q0_r - statistics.mean_r;
    float4 d0_g = parent.q0_g - statistics.mean_g;
    float4 d0_b = parent.q0_b - statistics.mean_b;
    float4 d1_r = parent.q1_r - statistics.mean_r;
    float4 d1_g = parent.q1_g - statistics.mean_g;
    float4 d1_b = parent.q1_b - statistics.mean_b;
    float4 d2_r = parent.q2_r - statistics.mean_r;
    float4 d2_g = parent.q2_g - statistics.mean_g;
    float4 d2_b = parent.q2_b - statistics.mean_b;
    float4 d3_r = parent.q3_r - statistics.mean_r;
    float4 d3_g = parent.q3_g - statistics.mean_g;
    float4 d3_b = parent.q3_b - statistics.mean_b;

    statistics.within_covariance.rr = (d0_r * d0_r + d1_r * d1_r + d2_r * d2_r + d3_r * d3_r) * 0.25f;
    statistics.within_covariance.gg = (d0_g * d0_g + d1_g * d1_g + d2_g * d2_g + d3_g * d3_g) * 0.25f;
    statistics.within_covariance.bb = (d0_b * d0_b + d1_b * d1_b + d2_b * d2_b + d3_b * d3_b) * 0.25f;
    statistics.within_covariance.rg = (d0_r * d0_g + d1_r * d1_g + d2_r * d2_g + d3_r * d3_g) * 0.25f;
    statistics.within_covariance.rb = (d0_r * d0_b + d1_r * d1_b + d2_r * d2_b + d3_r * d3_b) * 0.25f;
    statistics.within_covariance.gb = (d0_g * d0_b + d1_g * d1_b + d2_g * d2_b + d3_g * d3_b) * 0.25f;
    return statistics;
}

SHARED_INLINE void AccumulateBetweenParentCovariance(
    ParentStatistics parent,
    float4 mean_r, float4 mean_g, float4 mean_b,
    INOUT(CovarianceMatrix) covariance)
{
    float4 delta_r = parent.mean_r - mean_r;
    float4 delta_g = parent.mean_g - mean_g;
    float4 delta_b = parent.mean_b - mean_b;
    covariance.rr = covariance.rr + (delta_r * delta_r) * 0.25f;
    covariance.gg = covariance.gg + (delta_g * delta_g) * 0.25f;
    covariance.bb = covariance.bb + (delta_b * delta_b) * 0.25f;
    covariance.rg = covariance.rg + (delta_r * delta_g) * 0.25f;
    covariance.rb = covariance.rb + (delta_r * delta_b) * 0.25f;
    covariance.gb = covariance.gb + (delta_g * delta_b) * 0.25f;
}

// Computes total covariance as ANOVA between-parent plus within-parent terms.
SHARED_INLINE void ComputeChildBlockMoments(
    QuadrantMeans p00, QuadrantMeans p10,
    QuadrantMeans p01, QuadrantMeans p11,
    OUT(float4) mean_r, OUT(float4) mean_g, OUT(float4) mean_b,
    OUT(CovarianceMatrix) cov)
{
    ParentStatistics stats00 = ComputeParentStatistics(p00);
    ParentStatistics stats10 = ComputeParentStatistics(p10);
    ParentStatistics stats01 = ComputeParentStatistics(p01);
    ParentStatistics stats11 = ComputeParentStatistics(p11);

    mean_r = (stats00.mean_r + stats10.mean_r + stats01.mean_r + stats11.mean_r) * 0.25f;
    mean_g = (stats00.mean_g + stats10.mean_g + stats01.mean_g + stats11.mean_g) * 0.25f;
    mean_b = (stats00.mean_b + stats10.mean_b + stats01.mean_b + stats11.mean_b) * 0.25f;

    CovarianceMatrix within;
    within.rr = (stats00.within_covariance.rr + stats10.within_covariance.rr +
                 stats01.within_covariance.rr + stats11.within_covariance.rr) *
                0.25f;
    within.gg = (stats00.within_covariance.gg + stats10.within_covariance.gg +
                 stats01.within_covariance.gg + stats11.within_covariance.gg) *
                0.25f;
    within.bb = (stats00.within_covariance.bb + stats10.within_covariance.bb +
                 stats01.within_covariance.bb + stats11.within_covariance.bb) *
                0.25f;
    within.rg = (stats00.within_covariance.rg + stats10.within_covariance.rg +
                 stats01.within_covariance.rg + stats11.within_covariance.rg) *
                0.25f;
    within.rb = (stats00.within_covariance.rb + stats10.within_covariance.rb +
                 stats01.within_covariance.rb + stats11.within_covariance.rb) *
                0.25f;
    within.gb = (stats00.within_covariance.gb + stats10.within_covariance.gb +
                 stats01.within_covariance.gb + stats11.within_covariance.gb) *
                0.25f;

    CovarianceMatrix between;
    between.rr = float4(0.0f);
    between.gg = float4(0.0f);
    between.bb = float4(0.0f);
    between.rg = float4(0.0f);
    between.rb = float4(0.0f);
    between.gb = float4(0.0f);
    AccumulateBetweenParentCovariance(stats00, mean_r, mean_g, mean_b, between);
    AccumulateBetweenParentCovariance(stats10, mean_r, mean_g, mean_b, between);
    AccumulateBetweenParentCovariance(stats01, mean_r, mean_g, mean_b, between);
    AccumulateBetweenParentCovariance(stats11, mean_r, mean_g, mean_b, between);

    cov.rr = between.rr + within.rr;
    cov.gg = between.gg + within.gg;
    cov.bb = between.bb + within.bb;
    cov.rg = between.rg + within.rg;
    cov.rb = between.rb + within.rb;
    cov.gb = between.gb + within.gb;
}

SHARED_INLINE void ExpandProjectionRange(
    float4 r, float4 g, float4 b,
    ProjectionContext context,
    INOUT(float4) minimum,
    INOUT(float4) maximum)
{
    float4 projection = context.axis_r * (r - context.mean_r) +
                        context.axis_g * (g - context.mean_g) +
                        context.axis_b * (b - context.mean_b);
    minimum = Min(minimum, projection);
    maximum = Max(maximum, projection);
}

SHARED_INLINE void ExpandParentProjectionRange(
    QuadrantMeans parent,
    ProjectionContext context,
    INOUT(float4) minimum,
    INOUT(float4) maximum)
{
    ExpandProjectionRange(parent.q0_r, parent.q0_g, parent.q0_b, context, minimum, maximum);
    ExpandProjectionRange(parent.q1_r, parent.q1_g, parent.q1_b, context, minimum, maximum);
    ExpandProjectionRange(parent.q2_r, parent.q2_g, parent.q2_b, context, minimum, maximum);
    ExpandProjectionRange(parent.q3_r, parent.q3_g, parent.q3_b, context, minimum, maximum);
}

SHARED_INLINE void ComputeInitialEndpointsPCA(
    CovarianceMatrix cov, float4 mean_r, float4 mean_g, float4 mean_b,
    QuadrantMeans p00, QuadrantMeans p10, QuadrantMeans p01, QuadrantMeans p11,
    OUT(float4) p0_r, OUT(float4) p0_g, OUT(float4) p0_b,
    OUT(float4) p1_r, OUT(float4) p1_g, OUT(float4) p1_b)
{
    // Approximate the principal axis with two power iterations.
    float4 v_r = float4(0.57735f), v_g = float4(0.57735f), v_b = float4(0.57735f);

    for (int i = 0; i < 1; ++i)
    {
        float4 new_r = cov.rr * v_r + cov.rg * v_g + cov.rb * v_b;
        float4 new_g = cov.rg * v_r + cov.gg * v_g + cov.gb * v_b;
        float4 new_b = cov.rb * v_r + cov.gb * v_g + cov.bb * v_b;

        float4 len_sq = (new_r * new_r) + (new_g * new_g) +
                        (new_b * new_b) + float4(1e-20f);
        float4 inv_len = Rsqrt(len_sq);

        v_r = new_r * inv_len;
        v_g = new_g * inv_len;
        v_b = new_b * inv_len;
    }

    ProjectionContext context;
    context.axis_r = v_r;
    context.axis_g = v_g;
    context.axis_b = v_b;
    context.mean_r = mean_r;
    context.mean_g = mean_g;
    context.mean_b = mean_b;

    float4 t_min = float4(10000.0f);
    float4 t_max = float4(-10000.0f);
    ExpandParentProjectionRange(p00, context, t_min, t_max);
    ExpandParentProjectionRange(p10, context, t_min, t_max);
    ExpandParentProjectionRange(p01, context, t_min, t_max);
    ExpandParentProjectionRange(p11, context, t_min, t_max);

    p0_r = mean_r + (v_r * t_min);
    p0_g = mean_g + (v_g * t_min);
    p0_b = mean_b + (v_b * t_min);

    p1_r = mean_r + (v_r * t_max);
    p1_g = mean_g + (v_g * t_max);
    p1_b = mean_b + (v_b * t_max);
}

SHARED_INLINE void AccumulateLeastSquaresSample(
    float4 r, float4 g, float4 b,
    LeastSquaresContext context,
    INOUT(LeastSquaresAccumulator) accumulator)
{
    float4 projection = ((r - context.p0_r) * context.direction_r +
                         (g - context.p0_g) * context.direction_g +
                         (b - context.p0_b) * context.direction_b) *
                        context.inverse_length_squared;
    float4 weight = Round(Clamp(projection, 0.0f, 1.0f) * 3.0f) *
                    (1.0f / 3.0f);

    accumulator.weight_sum = accumulator.weight_sum + weight;
    accumulator.weight_squared_sum = accumulator.weight_squared_sum + (weight * weight);
    accumulator.weighted_r = accumulator.weighted_r + (weight * r);
    accumulator.weighted_g = accumulator.weighted_g + (weight * g);
    accumulator.weighted_b = accumulator.weighted_b + (weight * b);
}

SHARED_INLINE void AccumulateParentLeastSquares(
    QuadrantMeans parent,
    LeastSquaresContext context,
    INOUT(LeastSquaresAccumulator) accumulator)
{
    AccumulateLeastSquaresSample(parent.q0_r, parent.q0_g, parent.q0_b, context, accumulator);
    AccumulateLeastSquaresSample(parent.q1_r, parent.q1_g, parent.q1_b, context, accumulator);
    AccumulateLeastSquaresSample(parent.q2_r, parent.q2_g, parent.q2_b, context, accumulator);
    AccumulateLeastSquaresSample(parent.q3_r, parent.q3_g, parent.q3_b, context, accumulator);
}

SHARED_INLINE void OptimizeEndpointsLeastSquares(
    QuadrantMeans p00, QuadrantMeans p10, QuadrantMeans p01, QuadrantMeans p11,
    float4 mean_r, float4 mean_g, float4 mean_b,
    float4 p0_r, float4 p0_g, float4 p0_b,
    float4 p1_r, float4 p1_g, float4 p1_b,
    OUT(float4) opt_p0_r, OUT(float4) opt_p0_g, OUT(float4) opt_p0_b,
    OUT(float4) opt_p1_r, OUT(float4) opt_p1_g, OUT(float4) opt_p1_b)
{
    float4 d_r = p1_r - p0_r;
    float4 d_g = p1_g - p0_g;
    float4 d_b = p1_b - p0_b;
    float4 d_len_sq = (d_r * d_r) + (d_g * d_g) + (d_b * d_b) + float4(1e-12f);

    LeastSquaresContext context;
    context.p0_r = p0_r;
    context.p0_g = p0_g;
    context.p0_b = p0_b;
    context.direction_r = d_r;
    context.direction_g = d_g;
    context.direction_b = d_b;
    context.inverse_length_squared = float4(1.0f) / d_len_sq;

    LeastSquaresAccumulator accumulator;
    accumulator.weight_sum = float4(0.0f);
    accumulator.weight_squared_sum = float4(0.0f);
    accumulator.weighted_r = float4(0.0f);
    accumulator.weighted_g = float4(0.0f);
    accumulator.weighted_b = float4(0.0f);

    AccumulateParentLeastSquares(p00, context, accumulator);
    AccumulateParentLeastSquares(p10, context, accumulator);
    AccumulateParentLeastSquares(p01, context, accumulator);
    AccumulateParentLeastSquares(p11, context, accumulator);

    float4 S1 = accumulator.weight_sum;
    float4 S2 = accumulator.weight_squared_sum;
    float4 T1_r = accumulator.weighted_r;
    float4 T1_g = accumulator.weighted_g;
    float4 T1_b = accumulator.weighted_b;

    // Solve the normal equations, falling back to the mean for singular blocks.
    float4 det = (float4(16.0f) * S2) - (S1 * S1);
    float4 det_mask = det - float4(1e-6f);
    float4 safe_det = SelectLtZero(det_mask, float4(1.0f), det);
    float4 inv_det = float4(1.0f) / safe_det;

    // T0 = 16 * mean
    float4 T0_r = mean_r * 16.0f;
    float4 T0_g = mean_g * 16.0f;
    float4 T0_b = mean_b * 16.0f;

    // a = (S2 * T0 - S1 * T1) / det
    float4 a_r = (S2 * T0_r - S1 * T1_r) * inv_det;
    float4 a_g = (S2 * T0_g - S1 * T1_g) * inv_det;
    float4 a_b = (S2 * T0_b - S1 * T1_b) * inv_det;

    // d = (16 * T1 - S1 * T0) / det
    float4 opt_d_r = (float4(16.0f) * T1_r - S1 * T0_r) * inv_det;
    float4 opt_d_g = (float4(16.0f) * T1_g - S1 * T0_g) * inv_det;
    float4 opt_d_b = (float4(16.0f) * T1_b - S1 * T0_b) * inv_det;

    opt_p0_r = SelectLtZero(det_mask, mean_r, a_r);
    opt_p0_g = SelectLtZero(det_mask, mean_g, a_g);
    opt_p0_b = SelectLtZero(det_mask, mean_b, a_b);

    opt_p1_r = SelectLtZero(det_mask, mean_r, a_r + opt_d_r);
    opt_p1_g = SelectLtZero(det_mask, mean_g, a_g + opt_d_g);
    opt_p1_b = SelectLtZero(det_mask, mean_b, a_b + opt_d_b);
}

// ============================================================================
// Step 4-3: Chord-Curve Gap Correction (Theorem 6)
// ============================================================================

// Linear to sRGB.
SHARED_INLINE float4 LinearToSrgb(float4 lin)
{
#ifndef __SLANG__
    auto cvt = [](float c)
    {
        c = std::clamp(c, 0.0f, 1.0f);
        if (c <= 0.0031308f)
            return c * 12.92f;
        return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
    };
    return float4(cvt(lin.x), cvt(lin.y), cvt(lin.z), cvt(lin.w));
#else
    return select(lin <= 0.0031308f, lin * 12.92f, 1.055f * pow(max(lin, 0.0f), 1.0f / 2.4f) - 0.055f);
#endif
}

// sRGB to linear.
SHARED_INLINE float4 SrgbToLinearFloat4(float4 srgb)
{
#ifndef __SLANG__
    auto cvt = [](float c)
    {
        c = std::clamp(c, 0.0f, 1.0f);
        if (c <= 0.04045f)
            return c / 12.92f;
        return std::pow((c + 0.055f) / 1.055f, 2.4f);
    };
    return float4(cvt(srgb.x), cvt(srgb.y), cvt(srgb.z), cvt(srgb.w));
#else
    return select(srgb <= 0.04045f, srgb / 12.92f, pow((max(srgb, 0.0f) + 0.055f) / 1.055f, 2.4f));
#endif
}

// Corrects the chord-curve gap and returns sRGB endpoints.
SHARED_INLINE void CorrectChordCurveGap(
    float4 p0_r, float4 p0_g, float4 p0_b,
    float4 p1_r, float4 p1_g, float4 p1_b,
    OUT(float4) q0_r, OUT(float4) q0_g, OUT(float4) q0_b,
    OUT(float4) q1_r, OUT(float4) q1_g, OUT(float4) q1_b)
{
    float4 mid_lin_r = (p0_r + p1_r) * 0.5f;
    float4 mid_lin_g = (p0_g + p1_g) * 0.5f;
    float4 mid_lin_b = (p0_b + p1_b) * 0.5f;

    float4 s0_r = LinearToSrgb(p0_r);
    float4 s0_g = LinearToSrgb(p0_g);
    float4 s0_b = LinearToSrgb(p0_b);

    float4 s1_r = LinearToSrgb(p1_r);
    float4 s1_g = LinearToSrgb(p1_g);
    float4 s1_b = LinearToSrgb(p1_b);

    float4 mid_srgb_r = (s0_r + s1_r) * 0.5f;
    float4 mid_srgb_g = (s0_g + s1_g) * 0.5f;
    float4 mid_srgb_b = (s0_b + s1_b) * 0.5f;

    float4 L_mid_srgb_r = SrgbToLinearFloat4(mid_srgb_r);
    float4 L_mid_srgb_g = SrgbToLinearFloat4(mid_srgb_g);
    float4 L_mid_srgb_b = SrgbToLinearFloat4(mid_srgb_b);

    float4 sigma_r = mid_lin_r - L_mid_srgb_r;
    float4 sigma_g = mid_lin_g - L_mid_srgb_g;
    float4 sigma_b = mid_lin_b - L_mid_srgb_b;

    float4 corr = 4.0f / 9.0f;
    q0_r = LinearToSrgb(p0_r + (sigma_r * corr));
    q0_g = LinearToSrgb(p0_g + (sigma_g * corr));
    q0_b = LinearToSrgb(p0_b + (sigma_b * corr));

    q1_r = LinearToSrgb(p1_r + (sigma_r * corr));
    q1_g = LinearToSrgb(p1_g + (sigma_g * corr));
    q1_b = LinearToSrgb(p1_b + (sigma_b * corr));
}

// ============================================================================
// Step 5: Selector Reallocation (Theorem 7)
// ============================================================================
SHARED_INLINE void AssignNearestSelector(
    uint texel_index,
    float4 r, float4 g, float4 b,
    LinearPaletteBC1 palette,
    INOUT(uint4) packed_selectors)
{
    float4 d0 = (r - palette.c0_r) * (r - palette.c0_r) +
                (g - palette.c0_g) * (g - palette.c0_g) +
                (b - palette.c0_b) * (b - palette.c0_b);
    float4 d1 = (r - palette.c1_r) * (r - palette.c1_r) +
                (g - palette.c1_g) * (g - palette.c1_g) +
                (b - palette.c1_b) * (b - palette.c1_b);
    float4 d2 = (r - palette.c2_r) * (r - palette.c2_r) +
                (g - palette.c2_g) * (g - palette.c2_g) +
                (b - palette.c2_b) * (b - palette.c2_b);
    float4 d3 = (r - palette.c3_r) * (r - palette.c3_r) +
                (g - palette.c3_g) * (g - palette.c3_g) +
                (b - palette.c3_b) * (b - palette.c3_b);
    uint4 selector = FindBestSelector(d0, d1, d2, d3);
    packed_selectors = packed_selectors | (selector << (texel_index * 2));
}

SHARED_INLINE void PackAndReallocateSelectors(
    float4 q0_r, float4 q0_g, float4 q0_b,
    float4 q1_r, float4 q1_g, float4 q1_b,
    QuadrantMeans p00, QuadrantMeans p10, QuadrantMeans p01, QuadrantMeans p11,
    OUT(uint4) out_color0, OUT(uint4) out_color1, OUT(uint4) out_selectors)
{
    // Quantize the endpoints to opaque BC1's RGB565 representation.
    uint4 r0_5 = Float4ToUint4(Round(Clamp(q0_r, 0.0f, 1.0f) * 31.0f));
    uint4 g0_6 = Float4ToUint4(Round(Clamp(q0_g, 0.0f, 1.0f) * 63.0f));
    uint4 b0_5 = Float4ToUint4(Round(Clamp(q0_b, 0.0f, 1.0f) * 31.0f));

    uint4 r1_5 = Float4ToUint4(Round(Clamp(q1_r, 0.0f, 1.0f) * 31.0f));
    uint4 g1_6 = Float4ToUint4(Round(Clamp(q1_g, 0.0f, 1.0f) * 63.0f));
    uint4 b1_5 = Float4ToUint4(Round(Clamp(q1_b, 0.0f, 1.0f) * 31.0f));

    uint4 packed0 = (r0_5 << 11) | (g0_6 << 5) | b0_5;
    uint4 packed1 = (r1_5 << 11) | (g1_6 << 5) | b1_5;

#ifndef __SLANG__
    auto enforce_opaque_order = [](uint &color0, uint &color1)
    {
        if (color0 == color1)
        {
            // Change only one blue-channel LSB to guarantee 4-color mode.
            if ((color1 & 0x1Fu) > 0)
            {
                color1 -= 1u;
            }
            else
            {
                color0 += 1u;
            }
        }
        else if (color0 < color1)
        {
            std::swap(color0, color1);
        }
    };
    enforce_opaque_order(packed0.x, packed1.x);
    enforce_opaque_order(packed0.y, packed1.y);
    enforce_opaque_order(packed0.z, packed1.z);
    enforce_opaque_order(packed0.w, packed1.w);
#else
    bool4 equal_endpoints = packed0 == packed1;
    bool4 can_decrease_blue =
        (packed1 & uint4(0x1Fu)) > uint4(0u);

    // If blue is nonzero, lower color1 by one blue LSB.
    packed1 = select(
        equal_endpoints && can_decrease_blue,
        packed1 - uint4(1u),
        packed1);

    // Otherwise raise color0 by one blue LSB.
    packed0 = select(
        equal_endpoints && !can_decrease_blue,
        packed0 + uint4(1u),
        packed0);

    bool4 swap_endpoints = packed0 < packed1;
    uint4 unswapped0 = packed0;
    packed0 = select(swap_endpoints, packed1, packed0);
    packed1 = select(swap_endpoints, unswapped0, packed1);
#endif

    out_color0 = packed0;
    out_color1 = packed1;

    // Rebuild the quantized hardware palette before selector assignment.
    LinearPaletteBC1 palette = BuildOpaqueLinearPaletteBC1(out_color0, out_color1);

    out_selectors = uint4(0);

    // Top-left parent quadrants.
    AssignNearestSelector(0, p00.q0_r, p00.q0_g, p00.q0_b, palette, out_selectors);
    AssignNearestSelector(1, p00.q1_r, p00.q1_g, p00.q1_b, palette, out_selectors);
    AssignNearestSelector(4, p00.q2_r, p00.q2_g, p00.q2_b, palette, out_selectors);
    AssignNearestSelector(5, p00.q3_r, p00.q3_g, p00.q3_b, palette, out_selectors);

    // Top-right parent quadrants.
    AssignNearestSelector(2, p10.q0_r, p10.q0_g, p10.q0_b, palette, out_selectors);
    AssignNearestSelector(3, p10.q1_r, p10.q1_g, p10.q1_b, palette, out_selectors);
    AssignNearestSelector(6, p10.q2_r, p10.q2_g, p10.q2_b, palette, out_selectors);
    AssignNearestSelector(7, p10.q3_r, p10.q3_g, p10.q3_b, palette, out_selectors);

    // Bottom-left parent quadrants.
    AssignNearestSelector(8, p01.q0_r, p01.q0_g, p01.q0_b, palette, out_selectors);
    AssignNearestSelector(9, p01.q1_r, p01.q1_g, p01.q1_b, palette, out_selectors);
    AssignNearestSelector(12, p01.q2_r, p01.q2_g, p01.q2_b, palette, out_selectors);
    AssignNearestSelector(13, p01.q3_r, p01.q3_g, p01.q3_b, palette, out_selectors);

    // Bottom-right parent quadrants.
    AssignNearestSelector(10, p11.q0_r, p11.q0_g, p11.q0_b, palette, out_selectors);
    AssignNearestSelector(11, p11.q1_r, p11.q1_g, p11.q1_b, palette, out_selectors);
    AssignNearestSelector(14, p11.q2_r, p11.q2_g, p11.q2_b, palette, out_selectors);
    AssignNearestSelector(15, p11.q3_r, p11.q3_g, p11.q3_b, palette, out_selectors);
}

// Encodes four destination blocks in parallel. Every lane consumes one 2x2
// group of parent BC1 blocks.
SHARED_INLINE SymbolicDataBC1x4 EncodeMipBlocksBC1x4(
    SymbolicDataBC1x4 p00, SymbolicDataBC1x4 p10,
    SymbolicDataBC1x4 p01, SymbolicDataBC1x4 p11)
{
    uint4 p00_hist0, p00_hist1, p00_hist2, p00_hist3;
    uint4 p10_hist0, p10_hist1, p10_hist2, p10_hist3;
    uint4 p01_hist0, p01_hist1, p01_hist2, p01_hist3;
    uint4 p11_hist0, p11_hist1, p11_hist2, p11_hist3;

    Extract2x2SelectorHistograms(p00.selectors, p00_hist0, p00_hist1, p00_hist2, p00_hist3);
    Extract2x2SelectorHistograms(p10.selectors, p10_hist0, p10_hist1, p10_hist2, p10_hist3);
    Extract2x2SelectorHistograms(p01.selectors, p01_hist0, p01_hist1, p01_hist2, p01_hist3);
    Extract2x2SelectorHistograms(p11.selectors, p11_hist0, p11_hist1, p11_hist2, p11_hist3);

    QuadrantMeans p00_means, p10_means, p01_means, p11_means;
    ComputeParentQuadrantMeans(p00.color_0, p00.color_1, p00_hist0, p00_hist1, p00_hist2, p00_hist3, p00_means);
    ComputeParentQuadrantMeans(p10.color_0, p10.color_1, p10_hist0, p10_hist1, p10_hist2, p10_hist3, p10_means);
    ComputeParentQuadrantMeans(p01.color_0, p01.color_1, p01_hist0, p01_hist1, p01_hist2, p01_hist3, p01_means);
    ComputeParentQuadrantMeans(p11.color_0, p11.color_1, p11_hist0, p11_hist1, p11_hist2, p11_hist3, p11_means);

    float4 mean_r, mean_g, mean_b;
    CovarianceMatrix covariance;
    ComputeChildBlockMoments(p00_means, p10_means, p01_means, p11_means, mean_r, mean_g, mean_b, covariance);

    float4 p0_r, p0_g, p0_b, p1_r, p1_g, p1_b;
    ComputeInitialEndpointsPCA(covariance, mean_r, mean_g, mean_b,
                               p00_means, p10_means, p01_means, p11_means,
                               p0_r, p0_g, p0_b, p1_r, p1_g, p1_b);

    float4 optimized_p0_r, optimized_p0_g, optimized_p0_b;
    float4 optimized_p1_r, optimized_p1_g, optimized_p1_b;
    OptimizeEndpointsLeastSquares(
        p00_means, p10_means, p01_means, p11_means,
        mean_r, mean_g, mean_b,
        p0_r, p0_g, p0_b, p1_r, p1_g, p1_b,
        optimized_p0_r, optimized_p0_g, optimized_p0_b,
        optimized_p1_r, optimized_p1_g, optimized_p1_b);

    float4 corrected_p0_r, corrected_p0_g, corrected_p0_b;
    float4 corrected_p1_r, corrected_p1_g, corrected_p1_b;
    CorrectChordCurveGap(
        optimized_p0_r, optimized_p0_g, optimized_p0_b,
        optimized_p1_r, optimized_p1_g, optimized_p1_b,
        corrected_p0_r, corrected_p0_g, corrected_p0_b,
        corrected_p1_r, corrected_p1_g, corrected_p1_b);

    SymbolicDataBC1x4 result;
    PackAndReallocateSelectors(
        corrected_p0_r, corrected_p0_g, corrected_p0_b,
        corrected_p1_r, corrected_p1_g, corrected_p1_b,
        p00_means, p10_means, p01_means, p11_means,
        result.color_0, result.color_1, result.selectors);
    return result;
}

#ifndef __SLANG__
void ProcessRowBC1(
    const uint8_t *src_blocks, uint32_t src_block_width, uint32_t src_block_height,
    uint8_t *dst_blocks, uint32_t dst_block_width, uint32_t dst_row_y);
#endif
