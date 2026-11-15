// The VC-2's unvoiced detector: see Vc2Unvoiced.h (v208).  Every address is rom.4's; its check is Vc2UnvoicedCheck.cpp's
// (xt_vc2in).  The arithmetic is the SH-4's (XtFloat): an FMAC is the product rounded, then the sum.
#include "Vc2Unvoiced.h"
#include "Sh4Interp.h"
#include "XtFloat.h"
#include <atomic>
#include <bit>
#include <mutex>

namespace {
constexpr uint32_t kHighpass = 0x895613a0;   // [FW] two biquads: b0, b1, b2, a1, a2 each, the a's negated
constexpr uint32_t kNotch = 0x895613c8;      // [FW] the third, the same way
constexpr int kHighpassWords = 10, kNotchWords = 5;
// [FW 0x893ed1b8…0x893ed48c] the constants as rom.4 stores them
constexpr float kHysteresisLeast = std::bit_cast<float>(0x3b03126fu);   // 0.002: the reset's and the least; the rise
constexpr float kPeakShare = std::bit_cast<float>(0x3b800000u);         // 1/256 of the peak
constexpr float kDecay = std::bit_cast<float>(0x3c23d709u);             // 0.01: the decaying peak's, the level's attack
constexpr float kRelease = std::bit_cast<float>(0x3951b716u);           // 0.0002: the level's release
constexpr float kQuiet = 0.125f, kQuietGain = 0.25f, kHalf = 0.5f;      // the level's steps
constexpr float kGainShare = std::bit_cast<float>(0x3dcccccdu);         // 0.1
constexpr float kCountScale = std::bit_cast<float>(0x3f9c28f6u);        // 1.22
constexpr float kCarry = 0.75f;                                          // the last total's share
constexpr float kHeld = 29.0f;                                           // the threshold once unvoiced
constexpr float kFall = std::bit_cast<float>(0x3a83126eu);              // 0.001
constexpr float kResetThreshold = 50.0f, kResetLevel = std::bit_cast<float>(0x3e999999u);   // 0.3
constexpr int kResetDetect = 50, kLate = 5;

// The tables, the process's: read once by the first image to load, published by the flag, then read without a lock
class Coefficients {
public:
    bool load(const char* path);
    bool loaded() const { return loaded_.load(std::memory_order_acquire); }
    float highpass[kHighpassWords] {}, notch[kNotchWords] {};   // written before loaded_, never after

private:
    std::mutex lock_;
    std::atomic<bool> loaded_ { false };
};

bool Coefficients::load(const char* path) {
    const std::lock_guard<std::mutex> hold(lock_);
    if (loaded()) return true;
    Sh4 image;
    if (!image.loadRom(path)) return false;
    for (uint32_t i = 0; i < kHighpassWords; ++i) highpass[i] = std::bit_cast<float>(image.rd32(kHighpass + 4 * i));
    for (uint32_t i = 0; i < kNotchWords; ++i) notch[i] = std::bit_cast<float>(image.rd32(kNotch + 4 * i));
    loaded_.store(true, std::memory_order_release);
    return true;
}

constinit Coefficients g_coefficients;

// One transposed biquad's sample, as 0x893ed040 orders it: y = b0·x + s1; s1 = (b1·x + s2) + a1·y; s2 = b2·x + a2·y
float biquad(const float* c, float* s, float x) {
    using namespace xtf;
    const float y = add(mul(x, c[0]), s[0]);
    s[0] = add(mul(y, c[3]), add(mul(x, c[1]), s[1]));
    s[1] = add(mul(x, c[2]), mul(y, c[4]));
    return y;
}
} // namespace

bool vc2LoadUnvoiced(const char* path) { return g_coefficients.load(path); }
bool vc2UnvoicedLoaded() { return g_coefficients.loaded(); }

// 0x893ecf60: what it sets of this region and the flags' ring.  The second biquad's states and the smoothed decision it
// leaves as they were.
void Vc2Unvoiced::reset() {
    flags.fill(0);
    s.ringIndex = s.outputIndex = 0;
    s.detect = kResetDetect;
    s.spread = 0;
    s.soundLevel = 1.0f;
    s.outputWord99d8 = s.outputWords9a60[0] = s.outputWords9a60[1] = 0.0f;
    s.bq1[0] = s.bq1[1] = 0.0f;
    s.schmitt = s.schmittLast = s.hysteresis = kHysteresisLeast;
    for (float& p : s.peaks) p = 0.0f;
    for (float& c : s.counts) c = 0.0f;
    s.total = 0.0f;
    s.threshold = kResetThreshold;
    s.max = s.decaying = 0.0f;
    s.level = kResetLevel;
    s.bq3[0] = s.bq3[1] = 0.0f;
}

void Vc2Unvoiced::settings(int detectPlus24, int spread, float soundLevel) {
    s.detect = detectPlus24;
    s.spread = spread;
    s.soundLevel = soundLevel;
}

// The sample loop: the highpass's two biquads, the block's peak after them, the notch, and the Schmitt trigger at ± the
// hysteresis, a crossing counted whenever it flips
float Vc2Unvoiced::runSamples(const float* in) {
    const float* hp = g_coefficients.highpass;
    float peak = 0.0f;
    s.peaks[0] = 0.0f;
    s.counts[0] = 0.0f;
    for (int i = 0; i < kBlock; ++i) {
        const float y2 = biquad(hp + 5, s.bq2, biquad(hp, s.bq1, in[i]));
        const float magnitude = 0.0f > y2 ? -y2 : y2;
        if (magnitude > peak) s.peaks[0] = peak = magnitude;
        const float y3 = biquad(g_coefficients.notch, s.bq3, y2);
        const float next = y3 > s.schmitt ? -s.hysteresis : s.hysteresis;
        if (0.0f > xtf::mul(next, s.schmittLast)) s.counts[0] = xtf::add(s.counts[0], 1.0f);
        s.schmitt = s.schmittLast = next;
    }
    return peak;
}

// This block's crossings scaled by its level: the level by attack and release toward the peak, a gain by it, and the
// count by that gain over the peak when the decaying peak stands over it, else by 1.22
float Vc2Unvoiced::countScaled(float m) {
    using namespace xtf;
    float d = m > s.decaying ? m : s.decaying;
    d = sub(d, mul(sub(d, m), kDecay));
    s.decaying = d;
    s.level = add(mul(sub(m, s.level), m > s.level ? kDecay : kRelease), s.level);
    const float gain = kQuiet > s.level ? kQuietGain : (kHalf > s.level ? mul(s.level, 2.0f) : 1.0f);
    const float share = mul(gain, kGainShare);
    return d > div(share, kCountScale) ? mul(s.counts[0], div(share, m)) : mul(s.counts[0], kCountScale);
}

// The vote: four blocks' counts and 0.75 of the last total against the threshold, the flag into the ring, and the
// decision of the block kLate back smoothed a sample at a time (rising 0.002, falling 0.001)
void Vc2Unvoiced::vote() {
    using namespace xtf;
    float sum = s.counts[0];
    for (int k = 1; k < Vc2UnvoicedWords::kCounts; ++k) sum = add(sum, s.counts[k]);
    for (int k = Vc2UnvoicedWords::kCounts - 1; k > 0; --k) s.counts[k] = s.counts[k - 1];
    s.total = add(mul(s.total, kCarry), sum);
    const bool unvoiced = s.total > s.threshold;
    s.threshold = unvoiced ? kHeld : flt(s.detect);
    flags[(size_t)s.ringIndex] = unvoiced ? 1 : 0;
    s.ringIndex = s.ringIndex + 1 >= kRing ? 0 : s.ringIndex + 1;
    const int late = s.ringIndex - kLate < 0 ? s.ringIndex - kLate + kRing : s.ringIndex - kLate;
    const float flag = flt(flags[(size_t)late]);
    const float rate = flag > s.smooth[kBlock - 1] ? kHysteresisLeast : kFall;
    float at = s.smooth[kBlock - 1];
    for (float& v : s.smooth) v = at = add(mul(sub(flag, at), rate), at);
}

// 0x893ed040
void Vc2Unvoiced::block(const float* in) {
    float m = runSamples(in);
    for (int k = Vc2UnvoicedWords::kPeaks - 1; k > 0; --k) m = s.peaks[k] > m ? s.peaks[k] : m;
    for (int k = Vc2UnvoicedWords::kPeaks - 1; k > 0; --k) s.peaks[k] = s.peaks[k - 1];
    const float share = xtf::mul(m, kPeakShare);
    s.hysteresis = kHysteresisLeast > share ? kHysteresisLeast : share;
    s.max = m;
    s.counts[0] = countScaled(m);
    vote();
}

// 0x893ed3e0
bool Vc2Unvoiced::voiced() const {
    const int one = s.ringIndex - 1 < 0 ? s.ringIndex - 1 + kRing : s.ringIndex - 1;
    const int five = one - 4 < 0 ? one - 4 + kRing : one - 4;
    return (flags[(size_t)one] | flags[(size_t)five]) == 0;
}
