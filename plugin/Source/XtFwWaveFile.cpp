// A wave saved by rom.2's own writer: XtFwWaveFile.h (v482).
#include "XtFwWaveFile.h"
#include "XtFirmware.h"
#include "XtFwFileOut.h"
#include "XtFwMachine.h"
#include <algorithm>
#include <array>
#include <memory>

using xtfile::fail;

namespace {
// [FW] the writer, (path, the wave's number), and the store's table of 128 lists it finds the wave through
// (0x8949feb0: the list by the number's low 7 bits, a node's next at +4 and number at +8, its entry 0x18 before it)
constexpr uint32_t kWriter = 0x89496590, kWaves = 0x89ce87fc;
constexpr uint32_t kNode = 0x18, kNumberAt = 8, kNumber = 0;
// [FW] where the entry points at each block, in the writer's order (vpfl at none: the reader's open builds it)
constexpr std::array<uint32_t, XtWaveBlocks::kCount> kPointerAt { 0, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x40, 0x3c, 0x38 };
// [FW] a block's buffer: its size at +0x10 and its bytes from +0x1c (0x8949ea30, 0x8949e850)
constexpr uint32_t kSizeAt = 0x10, kBytesAt = 0x1c;
// [FW] the data's: its bytes from +0x40, its size 0x20 more than they are, and at +0x1c their address, as its
// constructor sets it (0x894a0bb4), which the bands' rebuild reads them through and puts the seven bands' pointers over
constexpr uint32_t kDataAt = 0x40, kDataSizeMore = 0x20, kSamplesAt = 0x1c;
// [FW] the bands' rebuild, (the store's override word, the wave's number), which the store's load calls on a band
// wave (0x894a3630, 0x8949f380), its load device making the bands from the samples, 1 when it has; and the room it
// wants the data block to have first, (frames, channels), growing it to that (0x894a1ce0).  The override word, which
// every lookup of the store takes over the number it is given when it is 0 or more, is put at -1, so the number finds
// the wave (the machine's RAM starts at 0, which would find wave 0 whatever number were given)
struct Rebuild { uint32_t at, overrideWord, none, room, done; };
constexpr Rebuild kRebuild { 0x894a1ef0, 0x89ce87f8, 0xffffffff, 0x89527980, 1 };
// [FW] the vpsc's stereo flag and frames, where the rebuild reads them (its +0x1e, +0x20)
constexpr size_t kStereoAt = 2, kFramesAt = 4;
// [FW] the loop setter, (&number, frame, which end, the backward loop's), as the Loop FWD and BWD screens' sets call it
// (0x894a48e0 … 0x894a4940): the vplp's word (+4 and +8 the forward loop's start and end, +0xc and +0x10 the backward's
// end and start) written, the record marked changed (+0x50), the cached loop events built again (0x894a1960).  It
// reads the number through a word of the harness's
struct LoopSet { uint32_t at, numberWord; };
constexpr LoopSet kLoopSet { 0x894a4e30, xtfw::kSamp + 0x80 };
// [HW] the XT's word at address 0x10, in its boot flash: the vpfl's builder (0x894a06d0) reads each block's size
// through the entry's pointer unchecked, so for a block the wave has none of it reads this, 9 in every vpfl the XT
// wrote for a SOLO wave with no vmdl (the user's project, analysis/projects/Emu_test.prj)
constexpr uint32_t kLowPage = 0, kLowPageBytes = 0x1000, kLowWord = 0x10, kLowWordIs = 9;
// Where the harness puts them: the path, the entry, then each block's buffer, below the machine's heap
constexpr uint32_t kPath = xtfw::kSamp, kEntry = xtfw::kSamp + 0x100, kBuffers = xtfw::kSamp + 0x200;
constexpr char kPathText[] = "A:\\WAVE.WAV";

// Each block the wave has in its own buffer from kBuffers, the entry pointing at it, the data's at least `room` in
// size (a band wave's, as the rebuild grows it); false if they don't fit
bool putBlocks(Sh4& c, const XtWaveBlocks& w, uint32_t room) {
    uint32_t at = kBuffers;
    for (size_t k = XtWaveBlocks::kVmdl; k < XtWaveBlocks::kCount; ++k) {
        if (!w.bytes[k]) continue;
        const auto n = static_cast<uint32_t>(w.bytes[k]->size());
        const bool data = k == XtWaveBlocks::kData;
        const uint32_t from = data ? kDataAt : kBytesAt;
        const uint32_t size = data ? std::max(n + kDataSizeMore, room) : n;
        if (size > xtfw::kHeap - at - from) return false;
        c.wr32(at + kSizeAt, size);
        c.write(at + from, w.bytes[k]->data(), n);
        if (data) c.wr32(at + kSamplesAt, at + kDataAt);
        c.wr32(kEntry + kPointerAt[k], at);
        at = (at + from + size + 15) & ~15u;
    }
    return true;
}

// The room a band wave's bands want in its data block, by rom.2 (0x89527980); 0 for any other wave
uint32_t bandRoom(Sh4& c, const std::vector<uint8_t>& vpsc) {
    if (!xtWaveHasBands(vpsc[1])) return 0;
    uint32_t frames = 0;
    for (size_t i = 0; i < 4; ++i) frames |= (uint32_t)vpsc[kFramesAt + i] << (8 * i);
    uint32_t room = 0;
    return c.call(kRebuild.room, { frames, vpsc[kStereoAt] != 0 ? 2u : 1u }, xtfw::kStack, &room) ? room : 0;
}

// A band wave's bands made by rom.2's rebuild on its entry, as the XT's load makes them; false, and why, if it fails
bool rebuildBands(Sh4& c, std::string* why) {
    uint32_t rc = 0;
    if (!c.call(kRebuild.at, { kRebuild.overrideWord, kNumber }, xtfw::kStack, &rc)) {
        if (why) *why = "the firmware's band rebuild failed: " + c.error();
        return false;
    }
    if (rc == kRebuild.done) return true;
    if (why) *why = "the firmware's band rebuild refused the wave: " + std::to_string((int32_t)rc);
    return false;
}

// The wave's four loop points set by rom.2's setter, each as its screen sets it; false, and why, if a call fails
bool setLoops(Sh4& c, const XtWaveLoops& l, std::string* why) {
    struct Set { long frame; uint32_t end, backward; };
    const std::array<Set, 4> sets { { { l.loopStart, 0, 0 }, { l.loopEnd, 1, 0 }, { l.bwdEnd, 0, 1 },
                                      { l.bwdStart, 1, 1 } } };
    c.wr32(kLoopSet.numberWord, kNumber);
    for (const Set& s : sets) {
        if (c.call(kLoopSet.at, { kLoopSet.numberWord, (uint32_t)s.frame, s.end, s.backward }, xtfw::kStack)) continue;
        if (why) *why = "the firmware's loop setter failed: " + c.error();
        return false;
    }
    return true;
}
} // namespace

bool xtFwWaveFile(const XtWaveBlocks& w, std::vector<uint8_t>& file, std::string* why, const XtWaveEdits& edits) {
    file.clear();
    const auto& vpsc = w.bytes[XtWaveBlocks::kVpsc];
    if (!vpsc || vpsc->size() < 8) return fail(why, "no vpsc");
    const size_t samples = w.bytes[XtWaveBlocks::kData] ? w.bytes[XtWaveBlocks::kData]->size() / 2 : 0;
    auto m = std::make_unique<xtfw::Machine>(xtFwRomPath(), samples);
    if (!m->ok) return fail(why, "no firmware image");
    Sh4& c = m->cpu;
    c.mapRam(kLowPage, kLowPageBytes);
    c.wr32(kLowWord, kLowWordIs);
    if (!putBlocks(c, w, bandRoom(c, *vpsc))) return fail(why, "the wave doesn't fit the machine");
    c.wr32(kEntry + kNode + kNumberAt, kNumber);   // its node alone in its list (no next)
    c.wr32(kWaves + 4 * (kNumber & 0x7f), kEntry + kNode);
    c.wr32(kRebuild.overrideWord, kRebuild.none);
    if (xtWaveHasBands((*vpsc)[1]) && !rebuildBands(c, why)) return false;
    if (edits.loops && !setLoops(c, *edits.loops, why)) return false;   // v486: on the wave as loaded, as on the XT
    return xtfw::runWriter(c, { kWriter, kPath, kPathText, kNumber }, file, why);
}
