// vc2run <rom.4> fxreverb: the VC-2's Reverb Types on XtFxChip, rom.4's own driver running them (vc2fxdriver.h), measured
// as reverbs (research/VC2_VOCAL_DESIGNER.md, "The new reverbs on XtFxChip").  Each type with its typed defaults and its
// Pre Delay at 0: an impulse into the reverb's left input word (0x73, what program A wrote to 0x72 the sample before),
// then into its right (0x75), and the wet outs alone (0xf4, 0xf6, before the mix) for kSeconds.  Said: the decay
// (vc2decay.h: T30, or T20 where the floor leaves no room, the range over the floor and the floor); the first sample an
// out moves; each out's level for each input; the left input's response against the right's, as it is and mirrored.
// envelope <type> [time]: the left out's RMS, mean and AC every quarter second.  Then, for each type with one time
// parameter (Rev Time, Reverb Time), that time over its range against the panel's text for it (rom.4's 0x894fa730(id,
// value)) and, where its range is one of the driver's two tables (0…63, 0…79), the table's time.  Fails on a fault.
#include "vc2decay.h"
#include "vc2fxdriver.h"
#include "vc2patches.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace vc2 {

namespace {

constexpr int kReverbSwitch = 0x13f, kReverbType = 0x270;      // [FW] rom.4's IDs
constexpr uint32_t kTypeIds = 0x895165e4;                      // [FW] each type's first and last typed ID
constexpr uint32_t kText = 0x894fa730;                         // [FW] (id, value) → the panel's text with its unit
constexpr uint32_t kTable64 = 0x89366350, kTable80 = 0x893663a0;   // [FW] (value) → samples at 44.1 kHz
constexpr int kTable64Max = 63, kTable80Max = 79;
constexpr unsigned kInL = 0x73, kInR = 0x75, kOutL = 0xf4, kOutR = 0xf6;
constexpr double kRate = 44100;
constexpr int kSeconds = 12;
constexpr int kSweepSteps = 6;   // the time parameter's range in this many steps, with its default
constexpr int32_t kMoved = 64;   // an out has moved past this
constexpr int32_t kHeight = 4 * kImpulse;   // the impulse here: half the chip's full scale

std::string trimmed(std::string s) {
    s.erase(0, s.find_first_not_of(' '));
    return s;
}

// A typed ID of `type` by its name, or 0
int typedIdNamed(Machine& m, int type, const char* name) {
    const int first = (int16_t)m.cpu.rd16(kTypeIds + 4 * (uint32_t)type);
    const int last = (int16_t)m.cpu.rd16(kTypeIds + 4 * (uint32_t)type + 2);
    for (int id = first; id <= last && first > 0; ++id)
        if (trimmed(paramName(m, (uint32_t)id)) == name) return id;
    return 0;
}

int timeIdOf(Machine& m, int type) {
    const int rev = typedIdNamed(m, type, "Rev Time");
    return rev ? rev : typedIdNamed(m, type, "Reverb Time");
}

std::string textOf(Machine& m, int id, int value) {
    uint32_t at = 0;
    if (!m.run(kText, {(uint32_t)id, (uint32_t)value}, &at)) return "?";
    std::string s;
    for (char c = (char)m.cpu.rd8(at); c && s.size() < 32; c = (char)m.cpu.rd8(++at)) s += c;
    return trimmed(s);
}

struct Response {
    std::vector<double> l, r;
    int onset = -1;
};

Response responseOf(const XtFxChip& chip, unsigned in) {
    const auto t = tracesOf(chip, in, {kOutL, kOutR}, (int)(kSeconds * kRate), kHeight);
    Response x{lessDc(t[0]), lessDc(t[1])};
    for (size_t k = 0; k < t[0].size() && x.onset < 0; ++k)
        if (std::abs(t[0][k]) > kMoved || std::abs(t[1][k]) > kMoved) x.onset = (int)k;
    return x;
}

double energyOf(const std::vector<double>& v) {
    double e = 0;
    for (const double s : v) e += s * s;
    return e;
}

double dbOf(double energy) {
    return energy > 0 ? 10 * std::log10(energy / ((double)kHeight * kHeight)) : -999;
}

// The two inputs' responses alike: their correlation over both outs, as they are and with the outs swapped
double likeness(const Response& a, const Response& b, bool mirrored) {
    const std::vector<double>& bl = mirrored ? b.r : b.l;
    const std::vector<double>& br = mirrored ? b.l : b.r;
    double ab = 0;
    for (size_t t = 0; t < a.l.size(); ++t) ab += a.l[t] * bl[t] + a.r[t] * br[t];
    const double na = energyOf(a.l) + energyOf(a.r), nb = energyOf(b.l) + energyOf(b.r);
    return na > 0 && nb > 0 ? ab / std::sqrt(na * nb) : 0;
}

// The reverb set by `s` over the defaults, settled; null on a fault
std::unique_ptr<XtFxChip> reverbAt(Machine& m, FxSettings& answers, const XtFxChip& chip, const FxSettings& s) {
    answers = s;
    if (!reloadFx(m)) return nullptr;
    return settled(chip);
}

// A response's decay, both outs, its floor against the impulse
Decay responseDecay(const Response& x) {
    return decayOf(x.l, x.r, kRate, (double)kHeight * kHeight);
}

void sayType(Machine& m, int type, const XtFxChip& chip) {
    const Response a = responseOf(chip, kInL), b = responseOf(chip, kInR);
    const Decay d = responseDecay(a);
    std::printf("  %2d %-20s T%d %5.2f s%s (%.0f dB over the floor, %+.0f dB); onset %d;", type,
                reverbTypeName(m, type).c_str(), d.span, d.seconds, d.settled ? "" : " (still falling at the end)",
                d.rangeDb, d.floorDb, a.onset);
    std::printf(" left in: outs %+.1f, %+.1f dB; right in: %+.1f, %+.1f dB; left against right %.2f, mirrored %.2f\n",
                dbOf(energyOf(a.l)), dbOf(energyOf(a.r)), dbOf(energyOf(b.l)), dbOf(energyOf(b.r)),
                likeness(a, b, false), likeness(a, b, true));
}

// The type's time parameter over its range: the text, the driver's table where its range is one, the T30
bool sweepType(Machine& m, int type, FxSettings& answers, const XtFxChip& chip, const FxSettings& base) {
    const int id = timeIdOf(m, type);
    if (!id) return true;
    const TypedDescriptor t = typedDescriptorOf(m, (uint32_t)id);
    std::set<int> values = {t.def};
    for (int k = 0; k <= kSweepSteps; ++k) values.insert(t.min + (t.max - t.min) * k / kSweepSteps);
    std::printf("  %2d %-20s %03x %s %d…%d:", type, reverbTypeName(m, type).c_str(), id,
                trimmed(paramName(m, (uint32_t)id)).c_str(), t.min, t.max);
    const bool table = t.max == kTable64Max || t.max == kTable80Max;
    for (const int v : values) {
        FxSettings s = base;
        s[id] = v;
        const auto c = reverbAt(m, answers, chip, s);
        uint32_t samples = 0;
        if (!c || (table && !m.run(t.max == kTable64Max ? kTable64 : kTable80, {(uint32_t)v}, &samples))) return false;
        std::printf(" %d \"%s\"", v, textOf(m, id, v).c_str());
        if (table) std::printf(" table %.2f", samples / kRate);
        const Decay d = responseDecay(responseOf(*c, kInL));
        std::printf(" T%d %.2f%s;", d.span, d.seconds, d.settled ? "" : " (still falling at the end)");
    }
    std::printf("\n");
    return true;
}

// The left out every quarter second after an impulse into the left input, and with no impulse (the baseline): each
// window's RMS in dB against the impulse, its mean in LSB and its RMS less the mean in dB
void sayEnvelope(const XtFxChip& chip) {
    constexpr size_t kWindow = 11025;
    const int frames = (int)(kSeconds * kRate);
    const auto a = tracesOf(chip, kInL, {kOutL}, frames, kHeight)[0], b = tracesOf(chip, kInL, {kOutL}, frames, 0)[0];
    const auto db = [](double e) { return e > 0 ? 10 * std::log10(e / ((double)kHeight * kHeight)) : -999; };
    std::printf("  window: RMS, mean (LSB), RMS less the mean; the baseline's RMS\n");
    for (size_t w = 0; w + kWindow <= a.size(); w += kWindow) {
        double ea = 0, eb = 0, mean = 0;
        for (size_t t = w; t < w + kWindow; ++t) {
            ea += (double)a[t] * a[t];
            eb += (double)b[t] * b[t];
            mean += a[t];
        }
        mean /= kWindow;
        std::printf("  %5.2f s: %+7.1f dB, %+8.1f, %+7.1f dB; %+7.1f dB\n", (double)w / kRate, db(ea / kWindow), mean,
                    db(ea / kWindow - mean * mean), db(eb / kWindow));
    }
}

// damping: each type's Hf Damp and Lf Damp, the panel's text at each end of their ranges and at their defaults
int sayDamping(Machine& m) {
    const Descriptor d = descriptorOf(m, kReverbType);
    for (int type = 0; type <= d.max - d.centre; ++type) {
        for (const char* name : {"Hf Damp", "Lf Damp"}) {
            const int id = typedIdNamed(m, type, name);
            if (!id) continue;
            const TypedDescriptor t = typedDescriptorOf(m, (uint32_t)id);
            std::printf("  %2d %-20s %03x %s: %d \"%s\", default %d \"%s\", %d \"%s\"\n", type,
                        reverbTypeName(m, type).c_str(), id, name, t.min, textOf(m, id, t.min).c_str(), t.def,
                        textOf(m, id, t.def).c_str(), t.max, textOf(m, id, t.max).c_str());
        }
    }
    return 0;
}

// envelope <type> [time]: the type, its Pre Delay 0 and its time parameter at `time`, as sayEnvelope says it
int envelopeRun(Machine& m, FxSettings& answers, const XtFxChip& chip, int argc, char** argv) {
    const std::optional<int> type = intArg(argv[2]), time = argc > 3 ? intArg(argv[3]) : std::optional<int>(-1);
    if (!type || !time) {
        std::fprintf(stderr, "fxreverb envelope <type> [time]: numbers\n");
        return 2;
    }
    FxSettings s = {{kReverbSwitch, 1}, {kReverbType, *type}};
    if (const int pre = typedIdNamed(m, *type, "Pre Delay")) s[pre] = 0;
    if (*time >= 0) s[timeIdOf(m, *type)] = *time;
    const auto c = reverbAt(m, answers, chip, s);
    if (c) sayEnvelope(*c);
    return c ? 0 : 1;
}

} // namespace

int fxreverb(Machine& m, int argc, char** argv) {
    FxSettings answers;
    std::set<int> heard;
    auto chip = std::make_unique<XtFxChip>();
    hookFxDriver(m, *chip, answers, heard);
    bool ok = startFxDriver(m);
    if (ok && argc > 2 && std::string_view(argv[1]) == "envelope") return envelopeRun(m, answers, *chip, argc, argv);
    if (ok && argc > 1 && std::string_view(argv[1]) == "damping") return sayDamping(m);
    const Descriptor d = descriptorOf(m, kReverbType);
    const int types = d.max - d.centre + 1;
    std::printf("each Reverb Type at its defaults, Pre Delay 0, %d s from an impulse into each input\n", kSeconds);
    for (int type = 0; type < types && ok; ++type) {
        FxSettings s = {{kReverbSwitch, 1}, {kReverbType, type}};
        if (const int pre = typedIdNamed(m, type, "Pre Delay")) s[pre] = 0;
        const auto c = reverbAt(m, answers, *chip, s);
        ok = c != nullptr;
        if (ok) sayType(m, type, *c);
    }
    std::printf("each type's time over its range (its value \"the panel's text\", the driver's table, decay s)\n");
    for (int type = 0; type < types && ok; ++type) {
        FxSettings s = {{kReverbSwitch, 1}, {kReverbType, type}};
        if (const int pre = typedIdNamed(m, type, "Pre Delay")) s[pre] = 0;
        ok = sweepType(m, type, answers, *chip, s);
    }
    std::printf("the same with Hf Damp and Lf Damp at their top, \"0[dB]\" on every type that has them\n");
    for (int type = 0; type < types && ok; ++type) {
        FxSettings s = {{kReverbSwitch, 1}, {kReverbType, type}};
        if (const int pre = typedIdNamed(m, type, "Pre Delay")) s[pre] = 0;
        const int hf = typedIdNamed(m, type, "Hf Damp"), lf = typedIdNamed(m, type, "Lf Damp");
        if (!hf || !lf) continue;
        s[hf] = typedDescriptorOf(m, (uint32_t)hf).max;
        s[lf] = typedDescriptorOf(m, (uint32_t)lf).max;
        ok = sweepType(m, type, answers, *chip, s);
    }
    if (ok) return 0;
    std::printf("fxreverb: a fault: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
