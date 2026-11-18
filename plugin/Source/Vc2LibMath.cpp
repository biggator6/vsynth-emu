// rom.4's logf, powf and the two routines under powf: see Vc2LibMath.h.  Read from the disassembly; each step is one
// FPU instruction's, in its order, an FMAC a product and then a sum.
#include "Vc2LibMath.h"
#include "Vc2LibMathTables.h"
#include "XtFloat.h"
#include <bit>
#include <span>

namespace {
constexpr uint32_t kSign = 0x80000000u, kExponent = 0x7f800000u, kMantissa = 0x007fffffu;   // a float's fields
constexpr uint32_t kOneExponent = 0x3f800000u;
constexpr uint32_t kWholeFrom = 0x4b000000u;   // [FW] from 2^23 on a float holds no fraction
constexpr int kExpTooLarge = 15, kFractionLarge = -1;   // [FW] 0x89509928's two exponent tests
constexpr int kExponentTop = 255;                       // [FW] 0x8950ae24's: the field that isn't a number

// [FW] What each routine asks of its argument first, the one test all three have inline: its sign, and one of zero
// (a denormal compares equal to it), infinite and not a number
enum FloatClass : uint32_t { kNotANumber = 1, kInfinite = 2, kZero = 4, kNegative = 8 };

uint32_t floatClassOf(float x) {
    const uint32_t bits = std::bit_cast<uint32_t>(x);
    const uint32_t sign = (bits & kSign) ? kNegative : 0;
    if (x == 0.0f) return sign | kZero;
    if ((bits & ~kSign) == kExponent) return sign | kInfinite;
    return x == x ? sign : sign | kNotANumber;
}

// A float's exponent, unbiased
int unbiasedExponent(uint32_t bits) { return (int)((bits & kExponent) >> 23) - 127; }

// The polynomial in x by FMACs, its highest power's coefficient first
float fmacSeries(std::span<const float> c, float x) {
    float sum = c[0];
    for (size_t i = 1; i < c.size(); ++i) sum = xtf::add(xtf::mul(sum, x), c[i]);
    return sum;
}

// [FW] A float's whole part toward zero, its sign kept (-0.5 gives -0): the library's inline truncf
float wholePart(float x) {
    const uint32_t bits = std::bit_cast<uint32_t>(x);
    if ((bits & kExponent) >= kWholeFrom) return x;
    return std::bit_cast<float>(std::bit_cast<uint32_t>(xtf::flt(xtf::ftrc(x))) | (bits & kSign));
}
} // namespace

float vc2LibLog(float x) {
    const Vc2LibMathTables& t = vc2LibMathTables();
    const uint32_t kind = floatClassOf(x);
    if (kind & kNotANumber) return t.notANumber;
    if (kind & kZero) return t.minusInfinity;
    if (kind & kNegative) return t.notANumber;
    if (kind & kInfinite) return t.infinity;
    if (x == t.one) return t.zero;
    const uint32_t bits = std::bit_cast<uint32_t>(x);
    const float mantissa = std::bit_cast<float>((bits & (kSign | kMantissa)) | kOneExponent);
    const float octaves = xtf::mul(xtf::add(xtf::flt(unbiasedExponent(bits)), t.half), t.ln2);
    const float z = xtf::div(xtf::add(mantissa, t.minusRoot2), xtf::add(mantissa, t.root2));
    return xtf::add(xtf::mul(z, fmacSeries(t.logSeries, xtf::mul(z, z))), octaves);
}

float vc2LibExp(float x) {
    const Vc2LibMathTables& t = vc2LibMathTables();
    const uint32_t kind = floatClassOf(x);
    if (kind & kNotANumber) return t.notANumber;
    if (kind & kZero) return t.one;
    if (unbiasedExponent(std::bit_cast<uint32_t>(x)) >= kExpTooLarge) return (kind & kNegative) ? t.zero : t.infinity;
    const float inOctaves = xtf::mul(x, t.perLn2);
    const float whole = wholePart(inOctaves);
    float fraction = xtf::sub(inOctaves, whole);
    int32_t octaves = xtf::ftrc(whole);
    const uint32_t bits = std::bit_cast<uint32_t>(fraction);
    if (unbiasedExponent(bits) >= kFractionLarge) {   // half an octave or more: the nearer whole one
        const bool below = (bits & kSign) != 0;
        fraction = xtf::add(fraction, below ? t.one : t.minusOne);
        octaves += below ? -1 : 1;
    }
    const float scaled = xtf::mul(fraction, t.ln2);
    return vc2LibLdexp(xtf::div(fmacSeries(t.expOver, scaled), fmacSeries(t.expUnder, scaled)), octaves);
}

float vc2LibLdexp(float x, int32_t n) {
    const Vc2LibMathTables& t = vc2LibMathTables();
    const uint32_t bits = std::bit_cast<uint32_t>(x), field = bits & kExponent;
    const float infinite = (bits & kSign) ? t.minusInfinity : t.infinity;
    const float zero = (bits & kSign) ? t.minusZero : t.zero;
    if (field == kExponent) return (bits & kMantissa) ? t.notANumber : infinite;
    if (field == 0) return zero;   // a denormal is a zero here
    const int32_t exponent = (int32_t)((field >> 23) + (uint32_t)n);   // the SH-4's sum wraps
    if (exponent > 0 && exponent < kExponentTop)
        return std::bit_cast<float>((bits & (kSign | kMantissa)) | ((uint32_t)exponent << 23));
    return n >= 0 ? infinite : zero;
}

float vc2LibPow(float x, float y) {
    const Vc2LibMathTables& t = vc2LibMathTables();
    const uint32_t ofX = floatClassOf(x), ofY = floatClassOf(y);
    if ((ofX | ofY) & kNotANumber) return t.notANumber;
    if (ofX & kZero) return (ofY & (kZero | kNegative)) ? t.notANumber : t.zero;
    if (ofY & kZero) return t.one;
    if (!(ofX & kNegative)) return vc2LibExp(xtf::mul(vc2LibLog(x), y));
    // Below zero only a whole, finite power has an answer; an odd one's is negative
    if (wholePart(y) != y || (ofY & kInfinite)) return t.notANumber;
    const float power = vc2LibExp(xtf::mul(vc2LibLog(-x), y));
    const float half = vc2LibLdexp(y, -1);
    return wholePart(half) == half ? power : -power;
}
