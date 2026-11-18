#pragma once
// xt_vc2libm's inputs (Vc2LibMathCheck.cpp): the floats each routine's branches turn on, and draws aimed at them.
// XT_VC2LIBM_MAIN only.
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <random>
#include <vector>

inline constexpr uint32_t kLibSign = 0x80000000u, kLibInfinity = 0x7f800000u, kLibMantissa = 0x007fffffu;
inline constexpr uint32_t kLibQuiet = 0x7fc00000u;
inline constexpr float kLibLn2 = std::numbers::ln2_v<float>;
inline constexpr float kLibOverflow = 128.0f * kLibLn2, kLibUnderflow = -126.0f * kLibLn2;   // e^x's ends in a float
inline constexpr float kLibFlushed = -150.0f * kLibLn2;
inline constexpr float kLibExpLimit = 32768.0f;   // from 2^15 exp answers without its series

inline float libFloat(uint32_t bits) { return std::bit_cast<float>(bits); }
inline uint32_t libBits(float x) { return std::bit_cast<uint32_t>(x); }
inline float libUnit(std::mt19937& rng) { return (float)rng() / 4294967296.0f; }

// Every float pattern a branch turns on, of both signs: zeros, denormals, the normals' ends, infinities and NaNs of
// both kinds; 2^k for the exponents the routines test and either side of each; odd and half-whole numbers, the root of
// 2 logf divides by, and the arguments where e^x leaves a float
inline const std::vector<uint32_t>& libEdges() {
    static const std::vector<uint32_t> edges = [] {
        std::vector<uint32_t> all;
        const auto both = [&](uint32_t magnitude) {
            all.push_back(magnitude);
            all.push_back(magnitude | kLibSign);
        };
        const auto around = [&](float x) {
            for (uint32_t bits = libBits(x) - 1; bits <= libBits(x) + 1; ++bits) both(bits);
        };
        for (uint32_t bits : { 0u, 1u, kLibMantissa, kLibInfinity - 1, kLibInfinity, kLibInfinity + 1, kLibQuiet - 1,
                               kLibQuiet, kLibQuiet + 1, ~kLibSign })
            both(bits);
        for (int exponent : { -126, -2, -1, 0, 1, 2, 3, 14, 15, 16, 22, 23, 24, 25, 127 })
            around(std::ldexp(1.0f, exponent));
        for (float x : { 1.5f, 2.5f, 3.0f, 10.0f, std::numbers::sqrt2_v<float>, kLibOverflow, kLibUnderflow,
                         kLibFlushed })
            around(x);
        return all;
    }();
    return edges;
}

// Powers of two for ldexp: none, its neighbours, what takes an exponent to either end, and the ends of the word
inline const std::vector<int32_t>& libScales() {
    static const std::vector<int32_t> scales = [] {
        std::vector<int32_t> all { std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() };
        for (int32_t n : { 0, 1, 2, 23, 126, 127, 128, 253, 254, 255, 256, 1000 }) {
            all.push_back(n);
            all.push_back(-n);
        }
        return all;
    }();
    return scales;
}

struct LibScaled {
    float x;
    int32_t n;
};

struct LibPower {
    float x, y;
};

// A float a few units either side of x, by its pattern
inline float libNear(std::mt19937& rng, float x) { return libFloat(libBits(x) + rng() % 9 - 4); }

// An edge, or one a few units from it
inline float libDrawEdge(std::mt19937& rng) {
    const float edge = libFloat(libEdges()[rng() % libEdges().size()]);
    return rng() % 2 ? edge : libNear(rng, edge);
}

// A float of either sign, its exponent drawn evenly between two
inline float libDrawSized(std::mt19937& rng, int least, int most) {
    const uint32_t field = (uint32_t)(least + (int)(rng() % (uint32_t)(most - least + 1)) + 127);
    return libFloat((rng() & kLibSign) | field << 23 | (rng() & kLibMantissa));
}

inline float libSigned(std::mt19937& rng, float x) { return rng() % 2 ? x : -x; }

inline float libDrawLog(std::mt19937& rng) {
    switch (rng() % 6) {
    case 0: return libFloat(rng());
    case 1: return libDrawEdge(rng);
    case 2: return libNear(rng, 1.0f);   // the routine's own test for 1
    case 3: return libNear(rng, std::ldexp(std::numbers::sqrt2_v<float>, (int)(rng() % 40) - 20));   // z near 0
    case 4: return std::fabs(libDrawSized(rng, -126, 127));
    default: return 0.25f + 4.0f * libUnit(rng);
    }
}

inline float libDrawExp(std::mt19937& rng) {
    switch (rng() % 7) {
    case 0: return libFloat(rng());
    case 1: return libDrawEdge(rng);
    case 2: return libSigned(rng, libNear(rng, kLibExpLimit));
    case 3: return libNear(rng, 0.5f * kLibLn2 * (float)((int)(rng() % 512) - 256));   // the fraction's half an octave
    case 4: return libDrawSized(rng, -126, -20);                                       // the fraction's product flushed
    case 5: return libDrawSized(rng, -20, 14);
    default: return 200.0f * (libUnit(rng) - 0.5f);
    }
}

// A power of two that takes x's exponent field to around 0 or 255, or any
inline int32_t libDrawScale(std::mt19937& rng, float x) {
    const int field = (int)((libBits(x) >> 23) & 0xff);
    switch (rng() % 5) {
    case 0: return libScales()[rng() % libScales().size()];
    case 1: return -field + (int)(rng() % 5) - 2;
    case 2: return 255 - field + (int)(rng() % 5) - 2;
    case 3: return (int32_t)rng();
    default: return (int)(rng() % 64) - 32;
    }
}

inline LibScaled libDrawLdexp(std::mt19937& rng) {
    const float x = rng() % 3 ? libFloat(rng()) : libDrawEdge(rng);
    return { x, libDrawScale(rng, x) };
}

// A whole power: small, or around where a float stops holding odd numbers (2^23, 2^24), or a large one
inline float libDrawWhole(std::mt19937& rng) {
    switch (rng() % 3) {
    case 0: return libSigned(rng, (float)(int)(1 + rng() % 64));
    case 1: return libSigned(rng, libNear(rng, std::ldexp(1.0f, 22 + (int)(rng() % 4))));
    default: return libSigned(rng, std::nearbyint(libDrawSized(rng, 0, 30)));
    }
}

// A base of every kind, of a modest size mostly, either sign
inline float libDrawBase(std::mt19937& rng) {
    switch (rng() % 4) {
    case 0: return libDrawEdge(rng);
    case 1: return libFloat(rng());
    case 2: return libSigned(rng, libNear(rng, 1.0f));
    default: return libSigned(rng, 0.125f + 8.0f * libUnit(rng));
    }
}

// A power of every kind: whole, half-whole, or of any size
inline float libDrawPower(std::mt19937& rng) {
    switch (rng() % 5) {
    case 0: return libDrawEdge(rng);
    case 1: return libDrawWhole(rng);
    case 2: return (float)(int)(rng() % 128) - 63.5f;
    case 3: return libFloat(rng());
    default: return 16.0f * (libUnit(rng) - 0.5f);
    }
}

// The power that takes `base` to about e^target
inline float libPowerFor(float base, float target) { return target / (float)std::log((double)base); }

// x and y: bases of every kind against powers of every kind, then the answers' edges: a negative base's whole powers,
// a base near 1 against a huge power, and the powers that reach overflow, underflow and exp's exponent test
inline LibPower libDrawPow(std::mt19937& rng) {
    constexpr std::array<float, 6> kTargets { kLibOverflow, kLibUnderflow, kLibFlushed, kLibExpLimit, -kLibExpLimit,
                                              0.5f * kLibLn2 };
    switch (rng() % 6) {
    case 0: return { libDrawBase(rng), libDrawPower(rng) };
    case 1: return { -std::fabs(libDrawBase(rng)), libDrawWhole(rng) };
    case 2: return { libNear(rng, 1.0f), libDrawSized(rng, 20, 40) };
    case 3: return { libSigned(rng, libNear(rng, 1.0f)), libDrawWhole(rng) };
    case 4: {
        const float base = 0.125f + 8.0f * libUnit(rng);
        return { base, libNear(rng, libPowerFor(base, kTargets[rng() % kTargets.size()])) };
    }
    default: return { libFloat(rng()), libFloat(rng()) };
    }
}
