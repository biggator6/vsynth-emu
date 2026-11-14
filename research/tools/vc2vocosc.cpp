// vc2run <rom.4> vocosc: the VOCODER OSC's IDs (0x354…0x383) followed from the patch store into the voice by rom.4's own
// routines (research/VC2_VOCAL_DESIGNER.md, "The VOCODER OSC's IDs, run").  rom.4's default patch made part 0's store,
// both OSCs switched on and set to a Type (vc2oscinput.h).  Then one ID at a time, over its range: the value written into
// zone 0's tone block by its descriptor and read back by rom.4's own reader (0x8932b050), which must give it; the OSC input
// builder's record; under VOCODER the dispatch and the converter on the input as 0x8939fe08 makes it (vc2oscinput.h) and
// a block taken as the voice + 0x54; and the carrier's init and update (0x893ebed0, 0x893ec4d0, after the rate setter 0x893ec6a0, the clock
// fixed as the growl run fixes it) on P = the block + 0xb0, with P+0x20 and P+0x2c as the kind 5 routine (0x893afff0)
// leaves them with no modulation (P+0x30, P+0x44) and gain and pitch 1.  Says each record byte and block and carrier word
// that moves from the defaults' pass, with its value at each step: under VOCODER at key 60, the Keyfollows at key 84,
// then the record alone under ANALOG, PCM and EXTIN.  Fails on a fault or a read-back that disagrees.
#include "vc2oscinput.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kSetRate = 0x893ec6a0, kCarrierInit = 0x893ebed0, kCarrierUpdate = 0x893ec4d0;   // [FW] (carrier, P)
constexpr uint32_t kOsClock = 0x893018d0, kFixedMs = 12345;   // [FW] as vc2growl.cpp answers it
constexpr uint32_t kVocoderType = 3;                           // [FW] OSC Type's VOCODER
constexpr uint32_t kFirstId = 0x354, kLastId = 0x383;
constexpr uint32_t kKeyfollowIds[] = {0x35c, 0x36a};   // [FW] Growl Keyfollow, OSC 1's and 2's
constexpr uint32_t kGrowlLevelIds[] = {0x35b, 0x369};  // [FW] Growl Level, OSC 1's and 2's
constexpr int kGrowlMid = 64, kGrowlOff = 0;   // so a Keyfollow's term isn't clamped away; the default
constexpr const char* kTypeNames[] = {"ANALOG", "PCM", "EXTIN"};
constexpr uint32_t kOut = kScratch + 0x10000, kCarrier = kScratch + 0x15000, kCarrierBytes = 0x80;
constexpr OscNote kAt{kScratch + 0x12000, kScratch + 0x13000, kScratch + 0x14000};   // the input, block and note
constexpr uint32_t kP = kAt.block + 0xb0;     // [FW] the voice's +0x104, the block being the voice's +0x54
constexpr uint8_t kKey = 60, kHighKey = 72, kVelocity = 100;
constexpr int kSteps = 4;   // an ID's range in this many steps

struct Pass {
    std::array<Bytes, kOscs> record, block, carrier;
};
enum class Part { record, block, carrier };
constexpr const char* kPartNames[] = {"record ", "block  ", "carrier"};

// The converter and the carrier on one OSC's record; false on a fault
bool convert(Machine& m, uint32_t osc, uint8_t key, Pass& p) {
    zeroBytes(m, kCarrier, kCarrierBytes);
    writeOscNote(m, kAt, osc, key, kVelocity, p.record[osc]);
    if (!m.run(kDispatch, {kAt.input, kAt.block}) || !m.run(kConverter, {kAt.input, kAt.block})) return false;
    p.block[osc] = bytesAt(m, kAt.block, kBlockBytes);
    m.setF32(kP + 0x00, 1.0f);
    m.setF32(kP + 0x04, 1.0f);
    m.setF32(kP + 0x20, m.f32(kP + 0x30));
    m.setF32(kP + 0x2c, m.f32(kP + 0x44));
    if (!m.run(kCarrierInit, {kCarrier, kP}) || !m.run(kCarrierUpdate, {kCarrier, kP})) return false;
    p.carrier[osc] = bytesAt(m, kCarrier, kCarrierBytes);
    return true;
}

// The builder for each OSC, and under VOCODER the converter and carrier; false on a fault or a builder that built nothing
bool runPass(Machine& m, Pass& p, uint8_t key, bool vocoder) {
    for (uint32_t osc = 0; osc < kOscs; ++osc) {
        if (!buildRecord(m, kOut, osc, p.record[osc])) return false;
        if (vocoder && !convert(m, osc, key, p)) return false;
    }
    return true;
}

const Bytes& partOf(const Pass& p, Part part, uint32_t osc) {
    return part == Part::record ? p.record[osc] : part == Part::block ? p.block[osc] : p.carrier[osc];
}

uint32_t word(const Bytes& b, uint32_t at) {
    uint32_t w = 0;
    std::memcpy(&w, &b[at], 4);
    return w;
}

// Each record byte, or block or carrier word, of one OSC where a pass differs from `base`, with its values
void sayMoved(const std::vector<Pass>& passes, const Pass& base, Part part, uint32_t osc) {
    const Bytes& b = partOf(base, part, osc);
    const uint32_t stride = part == Part::record ? 1 : 4;
    for (uint32_t at = 0; at < b.size(); at += stride) {
        const auto same = [&](const Pass& p) {
            const Bytes& o = partOf(p, part, osc);
            return stride == 1 ? o[at] == b[at] : word(o, at) == word(b, at);
        };
        if (std::all_of(passes.begin(), passes.end(), same)) continue;
        std::printf("    OSC %u %s +0x%03x:", osc + 1, kPartNames[(int)part], at);
        for (const Pass& p : passes) {
            const Bytes& o = partOf(p, part, osc);
            float f = 0;
            const uint32_t w = stride == 1 ? 0 : word(o, at);
            std::memcpy(&f, &w, 4);
            if (stride == 1)
                std::printf(" %d", (int8_t)o[at]);
            else
                std::printf(" %.6g(%08x)", f, w);
        }
        std::printf("\n");
    }
}

// One ID at the values given, or over its range in kSteps steps when none are: the values tried, and what each moved;
// false on a fault
bool followId(Machine& m, uint32_t id, const Pass& base, uint8_t key, bool vocoder, std::vector<int> values = {}) {
    const Descriptor d = descriptorOf(m, id);
    if (d.width == 0 || d.min == d.max) return true;
    std::printf("%03x %s (%d…%d, default %d), values", id, paramName(m, id).c_str(), d.min - d.centre,
                d.max - d.centre, d.def - d.centre);
    for (int k = 0; values.empty() && k <= kSteps; ++k)
        values.push_back(d.min + (d.max - d.min) * k / kSteps - d.centre);
    std::vector<Pass> passes;
    for (const int value : values) {
        std::printf(" %d", value);
        passes.emplace_back();
        if (!setToneValue(m, id, value) || !runPass(m, passes.back(), key, vocoder)) return false;
    }
    std::printf("\n");
    for (uint32_t osc = 0; osc < kOscs; ++osc)
        for (const Part part : {Part::record, Part::block, Part::carrier}) sayMoved(passes, base, part, osc);
    return setToneValue(m, id, d.def - d.centre);
}

// The defaults' pass at a key and Type, checked against a second one; false on a fault or a pass that moves
bool basePass(Machine& m, Pass& base, uint8_t key, bool vocoder) {
    Pass again;
    if (!runPass(m, base, key, vocoder) || !runPass(m, again, key, vocoder)) return false;
    for (uint32_t osc = 0; osc < kOscs; ++osc)
        for (const Part part : {Part::record, Part::block, Part::carrier})
            if (partOf(base, part, osc) != partOf(again, part, osc)) {
                std::printf("vocosc: two passes of the defaults differ (OSC %u %s)\n", osc + 1, kPartNames[(int)part]);
                return false;
            }
    return true;
}

bool everyId(Machine& m, uint8_t key, bool vocoder) {
    Pass base;
    if (!basePass(m, base, key, vocoder)) return false;
    for (uint32_t id = kFirstId; id <= kLastId; ++id)
        if (!followId(m, id, base, key, vocoder)) return false;
    return true;
}

} // namespace

int vocosc(Machine& m, int, char**) {
    m.cpu.hook(kOsClock, [](Sh4& c) { c.ret(kFixedMs); });
    bool ok = m.run(kSetRate, {}) && useDefaultPatch(m);
    std::printf("VOCODER, key %u\n", kKey);
    ok = ok && setOscType(m, kVocoderType) && everyId(m, kKey, true);
    std::printf("\nVOCODER, key %u, Growl Level %d, the Keyfollows\n", kHighKey, kGrowlMid);
    Pass high;
    for (const uint32_t id : kGrowlLevelIds) ok = ok && setToneValue(m, id, kGrowlMid);
    ok = ok && basePass(m, high, kHighKey, true);
    for (const uint32_t id : kKeyfollowIds) ok = ok && followId(m, id, high, kHighKey, true, {-5, -2, -1, 1, 2, 5});
    for (const uint32_t id : kGrowlLevelIds) ok = ok && setToneValue(m, id, kGrowlOff);
    for (uint32_t type = 0; type < kVocoderType && ok; ++type) {
        std::printf("\n%s, the record alone\n", kTypeNames[type]);
        ok = setOscType(m, type) && everyId(m, kKey, false);
    }
    if (ok) return 0;
    std::printf("vocosc: faulted: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
