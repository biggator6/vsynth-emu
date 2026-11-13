// vc2run <rom.4> vocosc: the VOCODER OSC's IDs (0x354…0x383) followed from the patch store into the voice by rom.4's own
// routines (research/VC2_VOCAL_DESIGNER.md, "The VOCODER OSC's IDs, run").  rom.4's default patch made a store
// (vc2patches' loadStore), part 0's entry in the part table (0x8950d8e0: +0 1 when the part has a store, +8 the store)
// pointed at it, both OSCs switched on and set to a Type.  Then one ID at a time, over its range: the value written into
// zone 0's tone block by its descriptor and read back by rom.4's own reader (0x8932b050), which must give it; the OSC input
// builder (0x893a04e0(out, part, zone, osc), a 128-byte record per OSC at out + 0x20 + zone·0x100 + osc·0x80); under
// VOCODER the dispatch (0x8939fb40) and the converter (0x8939fbd0) on the input as 0x8939fe08 makes it (an 8-byte note
// header, +2 the OSC, +3 key, +4 velocity, the record at +8, +0x100 pointing at a zeroed object) and a block taken as the
// voice + 0x54; and the carrier's init and update (0x893ebed0, 0x893ec4d0, after the rate setter 0x893ec6a0, the clock
// fixed as the growl run fixes it) on P = the block + 0xb0, with P+0x20 and P+0x2c as the kind 5 routine (0x893afff0)
// leaves them with no modulation (P+0x30, P+0x44) and gain and pitch 1.  Says each record byte and block and carrier word
// that moves from the defaults' pass, with its value at each step: under VOCODER at key 60, the Keyfollows at key 84,
// then the record alone under ANALOG, PCM and EXTIN.  Fails on a fault or a read-back that disagrees.
#include "vc2patches.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kBuilder = 0x893a04e0;     // [FW] (out, part, zone, osc) → nonzero when it built the record
// [FW] (input, block), as 0x8939fe08 calls them: the dispatch by Type (the block's kind; VOCODER 0x8939ced0's kind 3
// made 5), then the converter (the vibrato's fields, then a converter by kind: VOCODER's 0x8939db00)
constexpr uint32_t kDispatch = 0x8939fb40, kConverter = 0x8939fbd0;
constexpr uint32_t kSetRate = 0x893ec6a0, kCarrierInit = 0x893ebed0, kCarrierUpdate = 0x893ec4d0;   // [FW] (carrier, P)
constexpr uint32_t kOsClock = 0x893018d0, kFixedMs = 12345;   // [FW] as vc2growl.cpp answers it
constexpr uint32_t kPartTable = 0x8950d8e0;   // [FW] a pointer per part to its entry
constexpr uint32_t kSwitchId = 0x2e4, kTypeId = 0x2e5, kOscIdStride = 52, kVocoderType = 3;   // [FW] OSC 1's; OSC 2's on
constexpr uint32_t kFirstId = 0x354, kLastId = 0x383;
constexpr uint32_t kKeyfollowIds[] = {0x35c, 0x36a};   // [FW] Growl Keyfollow, OSC 1's and 2's
constexpr uint32_t kGrowlLevelIds[] = {0x35b, 0x369};  // [FW] Growl Level, OSC 1's and 2's
constexpr int kGrowlMid = 64, kGrowlOff = 0;   // so a Keyfollow's term isn't clamped away; the default
constexpr const char* kTypeNames[] = {"ANALOG", "PCM", "EXTIN"};
constexpr uint32_t kOut = kScratch + 0x10000, kInput = kScratch + 0x12000, kBlock = kScratch + 0x13000;
constexpr uint32_t kNote = kScratch + 0x14000, kCarrier = kScratch + 0x15000;
constexpr uint32_t kP = kBlock + 0xb0;        // [FW] the voice's +0x104, the block being the voice's +0x54
constexpr uint32_t kRecordBytes = 0x80, kOutHeader = 0x20, kOscStride = 0x80, kNoteHeader = 8, kNotePointer = 0x100;
constexpr uint32_t kInputBytes = 0x200, kBlockBytes = 0x1000, kNoteBytes = 0x400, kCarrierBytes = 0x80;
constexpr uint32_t kOscs = 2;
constexpr uint8_t kKey = 60, kHighKey = 72, kVelocity = 100;
constexpr int kSteps = 4;   // an ID's range in this many steps

using Bytes = std::vector<uint8_t>;
struct Pass {
    std::array<Bytes, kOscs> record, block, carrier;
};
enum class Part { record, block, carrier };
constexpr const char* kPartNames[] = {"record ", "block  ", "carrier"};

uint32_t toneBlock() {
    return kPatchStore + kToneBase;   // zone 0's
}

// The value into the store by its descriptor, and read back by rom.4's reader; false when the reader disagrees
bool setValue(Machine& m, uint32_t id, int value) {
    const Descriptor d = descriptorOf(m, id);
    const int width = d.width < 0 ? -d.width : d.width, total = width + d.bit, bytes = (total + 7) / 8;
    const uint32_t at = toneBlock() + (uint32_t)d.offset, shift = (uint32_t)(bytes * 8 - total);
    uint32_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | m.cpu.rd8(at + (uint32_t)k);
    const uint32_t mask = ((1u << width) - 1) << shift;
    v = (v & ~mask) | (((uint32_t)(value + d.centre) << shift) & mask);
    for (int k = bytes - 1; k >= 0; --k, v >>= 8) m.cpu.wr8(at + (uint32_t)k, (uint8_t)v);
    uint32_t back = 0;
    if (!m.run(kReadValue, {toneBlock(), id}, &back)) return false;
    if ((int32_t)back == value) return true;
    std::printf("vocosc: %03x set to %d reads %d\n", id, value, (int32_t)back);
    return false;
}

Bytes bytesAt(Machine& m, uint32_t at, uint32_t n) {
    Bytes b(n);
    m.cpu.read(at, b.data(), n);
    return b;
}

void zero(Machine& m, uint32_t at, uint32_t n) {
    const Bytes z(n, 0);
    m.cpu.write(at, z.data(), n);
}

// The converter and the carrier on one OSC's record; false on a fault
bool convert(Machine& m, uint32_t osc, uint8_t key, Pass& p) {
    zero(m, kInput, kInputBytes);
    zero(m, kBlock, kBlockBytes);
    zero(m, kNote, kNoteBytes);
    zero(m, kCarrier, kCarrierBytes);
    m.cpu.wr8(kInput + 2, (uint8_t)osc);
    m.cpu.wr8(kInput + 3, key);
    m.cpu.wr8(kInput + 4, kVelocity);
    m.cpu.write(kInput + kNoteHeader, p.record[osc].data(), kRecordBytes);
    m.cpu.wr32(kInput + kNotePointer, kNote);
    if (!m.run(kDispatch, {kInput, kBlock}) || !m.run(kConverter, {kInput, kBlock})) return false;
    p.block[osc] = bytesAt(m, kBlock, kBlockBytes);
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
        uint32_t built = 0;
        if (!m.run(kBuilder, {kOut, 0, 0, osc}, &built) || !built) return false;
        p.record[osc] = bytesAt(m, kOut + kOutHeader + kOscStride * osc, kRecordBytes);
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
        if (!setValue(m, id, value) || !runPass(m, passes.back(), key, vocoder)) return false;
    }
    std::printf("\n");
    for (uint32_t osc = 0; osc < kOscs; ++osc)
        for (const Part part : {Part::record, Part::block, Part::carrier}) sayMoved(passes, base, part, osc);
    return setValue(m, id, d.def - d.centre);
}

// Both OSCs on and of one Type; false on a fault
bool setType(Machine& m, uint32_t type) {
    for (uint32_t osc = 0; osc < kOscs; ++osc)
        if (!setValue(m, kSwitchId + kOscIdStride * osc, 1) || !setValue(m, kTypeId + kOscIdStride * osc, (int)type))
            return false;
    return true;
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
    const uint32_t entry = m.cpu.rd32(kPartTable);
    bool ok = m.run(kSetRate, {}) && loadStore(m, {});
    m.cpu.wr8(entry, 1);
    m.cpu.wr32(entry + 8, kPatchStore);
    std::printf("VOCODER, key %u\n", kKey);
    ok = ok && setType(m, kVocoderType) && everyId(m, kKey, true);
    std::printf("\nVOCODER, key %u, Growl Level %d, the Keyfollows\n", kHighKey, kGrowlMid);
    Pass high;
    for (const uint32_t id : kGrowlLevelIds) ok = ok && setValue(m, id, kGrowlMid);
    ok = ok && basePass(m, high, kHighKey, true);
    for (const uint32_t id : kKeyfollowIds) ok = ok && followId(m, id, high, kHighKey, true, {-5, -2, -1, 1, 2, 5});
    for (const uint32_t id : kGrowlLevelIds) ok = ok && setValue(m, id, kGrowlOff);
    for (uint32_t type = 0; type < kVocoderType && ok; ++type) {
        std::printf("\n%s, the record alone\n", kTypeNames[type]);
        ok = setType(m, type) && everyId(m, kKey, false);
    }
    if (ok) return 0;
    std::printf("vocosc: faulted: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
