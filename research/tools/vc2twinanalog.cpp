// vc2run analog <rom.2>: the analog OSC's waveform routines on rom.2's and rom.4's, without the port (xt_analog holds the
// port to rom.2, so rom.2 equal to rom.4 is the port equal to rom.4).  A voice is made in rom.2 alone, as xt_analog makes
// it (plugin/Source/XtAnalog.cpp): the calibration (0x894e5f80) from the descriptors' defaults, the converter (0x894e1920)
// on a random input of the waveform's, the note-on (0x89504af0) and the install (0x89505170), which leaves the waveform's
// routine at +0x968.  rom.4's note-on, install and update are compiled anew on a voice laid out otherwise
// (research/VC2_VOCAL_DESIGNER.md, "The analog OSC in rom.4"), so its analog sub-object (voice + 0xd4, what a waveform
// routine is given) is copied across with the 4-byte field rom.4 inserts above +0x84, and each image's routine (the pair
// by the installs' literals) renders block by block: the 64 samples, the returned mask and the sub-object compared.  Every
// fifth block rom.2's update (0x89505400) moves the targets and the sub-object is copied again.  Fails on any difference.
#include "vc2runs.h"
#include "vc2twin.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

// [FW] the waveform routines, rom.2's and rom.4's, by the installs' literals in order (0x89505170, 0x893c2ad0) and the
// sub-oscillator's by shape at the block's distance (0x1435a0)
constexpr Twin::Pair kWaveforms[] = {
    {0x895063f0, 0x893c3890}, {0x895060e0, 0x893c3580}, {0x89508c20, 0x893c5680}, {0x89508aa0, 0x893c5500},
    {0x895094c0, 0x893c5f20}, {0x89508800, 0x893c5260}, {0x89509030, 0x893c5a90}, {0x89508db0, 0x893c5810},
    {0x89504a40, 0x893c2230}, {0x89508540, 0x893c4fa0}, {0x895081f0, 0x893c4c50}, {0x89508f30, 0x893c5990},
    {0x89509360, 0x893c5dc0}, {0x89509680, 0x893c60e0}, {0x89508960, 0x893c53c0}, {0x89509230, 0x893c5c90},
    {0x89508060, 0x893c4ac0}, {0x89508670, 0x893c50d0}, {0x89508350, 0x893c4db0},
};
constexpr uint32_t kConverter = 0x894e1920, kNoteOn = 0x89504af0, kInstall = 0x89505170, kUpdate = 0x89505400;  // [FW]
constexpr uint32_t kSystem = 0x894e5f80, kSystemObject = 0x89d1b070;                                         // [FW]
constexpr uint32_t kVelCurveOf = 0x894ebed0, kVelSensOf = 0x894ebe20;                                        // [FW]
constexpr uint32_t kDescriptors = 0x897a167c, kSys0 = 0x528, kSysLast = 0x55b;   // [FW] the hidden calibration IDs
constexpr uint32_t kSkip = 0x894796d0, kGetter = 0x8947d310, kMatrix = 0x894d96d0;                           // [FW]
constexpr uint32_t kControllerGetters[] = {0x894e7010, 0x894e70b0, 0x894e7320, 0x894e7520};                 // [FW]
constexpr int kWaveCodes = 11;   // [FW] rom.4's OSC Analog Waveform, 0…10 (SAW…LA-SQUARE)

constexpr uint32_t kIn = kScratch, kPart = kScratch + 0x200, kZero = kScratch + 0x400, kVoice = kScratch + 0x1000;
constexpr uint32_t kEnvs = kScratch + 0x3000, kLfo = kScratch + 0x3400, kBuf = kScratch + 0x4000, kBufBytes = 64 * 4;
// [FW] the OSC object, laid out in rom.4 with vc2twin.h's kOscInsert: the waveform routines read +0x80 and +0x84 where
// rom.2's do and +0xec on 4 bytes further (0x89509360 against 0x893c5dc0); the band run places the insert at +0x94
constexpr uint32_t kSub = kVoice + 0xd4, kSubSize = 0x200;
constexpr int kNotes = 100, kBlocks = 40;

void hookRom2(Twin& t) {
    Sh4& c = t.vsynth;
    c.hook(kSkip, [](Sh4& m) { m.ret(0); });
    c.hook(kGetter, [](Sh4& m) {   // the calibration: default − centre from the descriptors, as xt_analog's T.sys
        const uint32_t id = (uint32_t)(int16_t)m.r[5];
        const uint32_t d = kDescriptors + 12 * id;
        m.ret(id >= kSys0 && id <= kSysLast ? (uint32_t)((int16_t)m.rd16(d + 6) - (int16_t)m.rd16(d + 4)) : 0u);
    });
    c.hook(kMatrix, [](Sh4& m) { m.ret(0); });
    for (const uint32_t a : kControllerGetters) c.hook(a, [](Sh4& m) { m.ret(kZero); });
}

uint32_t pick(Rng& r, int lo, int hi) {
    return (uint32_t)(lo + (int)(r.next() % (uint32_t)(hi - lo + 1)));
}

uint32_t rom2Return(Twin& t, uint32_t routine, uint32_t arg) {
    return t.callVSynth(routine, {arg}) ? t.vsynth.r[0] : 0;
}

struct Field {
    uint32_t offset, bytes, value;
};

// A modulated knob's group in the input (Pulse Width's, Fat's): its value, key follow ×10, LFO and envelope depths,
// velocity curve and sensitivity, at their offsets
struct Group {
    uint32_t value, kf, lfo, env, curve, sens;
    int lowest;   // the value's range is lowest…lowest + 126 or 127
};
constexpr Group kPulseWidth{0x26, 0x28, 0x2a, 0x2b, 0x2c, 0x2e, -63}, kFat{0x30, 0x32, 0x34, 0x35, 0x36, 0x38, 0};

void groupFields(Twin& t, Rng& r, const Group& g, bool kfZero, std::vector<Field>& out) {
    out.push_back({g.value, 1, pick(r, g.lowest, g.lowest == 0 ? 127 : 63)});
    out.push_back({g.kf, 2, kfZero ? 0 : pick(r, -20, 20) * 10});
    out.push_back({g.lfo, 1, pick(r, -63, 63)});
    out.push_back({g.env, 1, pick(r, -63, 63)});
    out.push_back({g.curve, 1, rom2Return(t, kVelCurveOf, pick(r, 0, 7))});
    out.push_back({g.sens, 2, rom2Return(t, kVelSensOf, pick(r, -63, 63))});
}

// The analog converter's input as xt_analog's newCase(false) and writeInput make it, for one waveform, the sub oscillator
// on or off
void writeAnalogInput(Twin& t, Rng& r, int code, bool sub) {
    std::vector<Field> fields = {
        {0x03, 1, pick(r, 0, 127)},                                // key
        {0x04, 1, pick(r, 1, 127)},                                // velocity
        {0x0a, 1, pick(r, -12, 12)},                               // gain, dB
        {0x0e, 2, r.next() & 1 ? 1000 : pick(r, -2000, 2000)},     // pitch key follow
        {0x68, 2, pick(r, -12, 12)},                               // coarse
        {0x70, 4, pick(r, -500, 500)},                             // fine
        {0x7c, 1, pick(r, 0, 127)},                                // the second key
        {0x1c, 1, pick(r, 0, 127)},                                // level
        {0x1e, 2, pick(r, 0, 40) * 10 - 200},                      // level key follow
        {0x21, 1, rom2Return(t, kVelCurveOf, 1)},
        {0x22, 2, rom2Return(t, kVelSensOf, 0)},
        {0x24, 1, (uint32_t)code},                                 // waveform
        {0x25, 1, pick(r, 0, 40)},                                 // impact
        {0x3a, 1, sub ? pick(r, -63, 63) : 0},                     // detune
        {0x3b, 1, sub ? pick(r, 1, 3) : 0},                        // sub octave
        {0x3c, 1, sub ? pick(r, 0, 127) : 0},                      // sub level
        {0xdc, 4, kPart},
    };
    const bool kfZero = r.next() % 4 == 0;
    groupFields(t, r, kPulseWidth, kfZero, fields);
    groupFields(t, r, kFat, kfZero, fields);
    const std::vector<uint8_t> zero(0x200, 0);
    t.vsynth.write(kIn, zero.data(), 0x100);
    t.vsynth.write(kPart, zero.data(), 0x200);
    for (const Field& f : fields) t.vsynth.write(kIn + f.offset, &f.value, f.bytes);
}

// The envelopes' and the LFO's outputs the analog note-on and update read, rom.2's
void writeEnvelopeOutputs(Twin& t, Rng& r, bool attack) {
    const float out[4] = {r.in(-1, 1), r.in(0, 1), r.in(0, 1), r.in(0, 1)};   // pitch, Fat, PW, amp
    for (uint32_t i = 0; i < 4; ++i) t.vsynth.write(kEnvs + 0x100 * i + 0x4c, &out[i], 4);
    const float lfo = r.in(-1, 1);
    const float atk = attack ? 25.0f : 0.0f;
    t.vsynth.write(kLfo + 0x38, &lfo, 4);
    t.vsynth.write(kEnvs + 0x300 + 0x1c, &atk, 4);
}

// The voice made in rom.2 for one waveform: rom.2's routine for it, 0 on a fault
uint32_t makeVoice(Twin& t, Rng& r, int code, bool sub) {
    writeAnalogInput(t, r, code, sub);
    const std::vector<uint8_t> zero(0x1000, 0);
    t.vsynth.write(kVoice, zero.data(), zero.size());
    for (uint32_t i = 0; i < 4; ++i) t.vsynth.wr32(kVoice + 0x970 + 4 * i, kEnvs + 0x100 * i);
    for (uint32_t o = 0x980; o < 0x990; o += 4) t.vsynth.wr32(kVoice + o, kLfo);
    if (!t.callVSynth(kConverter, {kIn, kVoice})) return 0;
    const float one = 1.0f;
    t.vsynth.write(kVoice + 0x990, &one, 4);
    writeEnvelopeOutputs(t, r, r.next() & 1);
    if (!t.callVSynth(kNoteOn, {kVoice}) || !t.callVSynth(kInstall, {kVoice})) return 0;
    return t.vsynth.rd32(kVoice + 0x968);
}

// Where one block's output or sub-object parts, or nullptr; `at` the byte offset
const char* blockDiffers(Twin& t, long& at) {
    if ((at = t.differs(kBuf, kBufBytes)) >= 0) return "samples";
    at = t.differsInserted(kSub, kSubSize, kOscInsert);
    return at >= 0 ? "sub-object" : nullptr;
}

// One note: false on a fault; `counts` its blocks and those that differ, the first said
bool note(Twin& t, Rng& r, int code, bool sub, Twin::Returns& counts) {
    const uint32_t fn2 = makeVoice(t, r, code, sub);
    const auto* pair = std::ranges::find(kWaveforms, fn2, &Twin::Pair::vsynth);
    if (pair == std::end(kWaveforms)) {
        std::printf("waveform %d%s: rom.2's routine %08x has no pair\n", code, sub ? " with sub" : "", fn2);
        return false;
    }
    t.copyInserted(kSub, kSubSize, kOscInsert);
    const std::vector<uint8_t> zero(kBufBytes, 0);
    for (int b = 0; b < kBlocks; ++b) {
        if (b % 5 == 4) {
            writeEnvelopeOutputs(t, r, false);
            if (!t.callVSynth(kUpdate, {kVoice})) return false;
            t.copyInserted(kSub, kSubSize, kOscInsert);
        }
        const uint32_t sync = r.next() % 8 == 0 ? (r.next() << 16 | r.next()) & (r.next() << 16 | r.next()) : 0u;
        t.write(kBuf, zero.data(), kBufBytes);
        if (!t.call(fn2, pair->vc2, {kBuf, kSub, sync})) return false;
        long at = -1;
        const char* where = blockDiffers(t, at);
        const bool mask = t.vsynth.r[0] != t.vc2.cpu.r[0];
        ++counts.cases;
        if (!where && !mask) continue;
        if (counts.differ++ == 0)
            std::printf("waveform %d%s (%08x / %08x) block %d: %s at +%#lx%s\n", code, sub ? " with sub" : "", fn2,
                        pair->vc2, b, where ? where : "mask", at, mask ? ", the mask differs" : "");
    }
    return true;
}

int analogCases(Twin& t) {
    hookRom2(t);
    if (!t.callVSynth(kSystem, {kSystemObject, 0, 0, 0})) return -1;
    Rng r;
    Twin::Returns all;
    for (int code = 0; code < kWaveCodes; ++code)
        for (const bool sub : {false, true}) {
            Twin::Returns counts;
            for (int n = 0; n < kNotes; ++n)
                if (!note(t, r, code, sub, counts)) return -1;
            char label[32];
            std::snprintf(label, sizeof label, "waveform %2d%s", code, sub ? " with sub" : "         ");
            addCount(label, "blocks", counts, all);
        }
    return sayCount("analog", "blocks", all);
}

} // namespace

int analog(Machine& m, int argc, char** argv) {
    return twinRun(m, argc, argv, noPortTables, analogCases);
}

} // namespace vc2
