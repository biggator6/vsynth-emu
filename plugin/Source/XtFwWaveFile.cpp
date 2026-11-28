// A wave saved by rom.2's own writer: XtFwWaveFile.h (v482).
#include "XtFwWaveFile.h"
#include "XtFirmware.h"
#include "XtFwFileOut.h"
#include "XtFwMachine.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>

using xtfile::fail;

namespace {
// [FW] the writer, (path, the wave's key), and the store's table of 128 lists it finds the wave through (0x8949feb0:
// the list by the key's low 7 bits, a node's next at +4 and key at +8, its entry 0x18 before it).  The store keys wave
// n as n + 3000 (0x894a9d00, 0x8949f380): the harness's one wave is wave 0, key 3000 (v487; key 0 before, which the
// writer, setter and rebuild, given the key, took as well)
constexpr uint32_t kWriter = 0x89496590, kWaves = 0x89ce87fc;
constexpr uint32_t kNode = 0x18, kNumberAt = 8, kWave = 0, kNumber = kWave + 3000;
// [FW] each block in the writer's order: where the entry points at it (vpfl at none: the reader's open builds it) and
// the vtable rom.2's block maker gives it (0x894a0c50: each kind's destructor at its +0x14, which a Truncate's drop of
// the encode calls, v487)
struct BlockAt { uint32_t pointer, vtable; };
constexpr std::array<BlockAt, XtWaveBlocks::kCount> kBlockAt { {
    { 0, 0 }, { 0x24, 0x89831330 }, { 0x28, 0x89831354 }, { 0x2c, 0x89831378 }, { 0x30, 0x8983139c },
    { 0x34, 0x898313c0 }, { 0x40, 0x8983142c }, { 0x3c, 0x89831408 }, { 0x38, 0x898313e4 },
} };
// [FW] a block's buffer: its size at +0x10, its vtable at +0x14, its entry at +0x18 (0x894a0c50) and its bytes from
// +0x1c (0x8949ea30, 0x8949e850)
constexpr uint32_t kSizeAt = 0x10, kVtableAt = 0x14, kEntryAt = 0x18, kBytesAt = 0x1c;
// [FW] the store's own heap's resize and free (0x8949e540(block, header, size); 0x8949e460(block)), which Truncate's
// resize of the data block and drop of the encode, and the band rebuild's grow, call.  A resize is in place, as the
// store's heap grows a chunk into the space after it (0x8949dc10), its callers keeping the block they had (0x894a1ce0);
// the harness puts the data block last, so it has the free memory after it.  v487
struct StoreHeap { uint32_t resize, free; };
constexpr StoreHeap kStoreHeap { 0x8949e540, 0x8949e460 };
// [FW] the data's: its bytes from +0x40, its size 0x20 more than they are, and at +0x1c their address, as its
// constructor sets it (0x894a0bb4), which the bands' rebuild reads them through and puts the seven bands' pointers over
constexpr uint32_t kDataAt = 0x40, kDataSizeMore = 0x20, kSamplesAt = 0x1c;
// [FW] the bands' rebuild, (the store's override word, the wave's number), which the store's load calls on a band
// wave (0x894a3630, 0x8949f380), its load device making the bands from the samples, 1 when it has, the data block grown
// first to the room the bands want (0x894a1ce0: 0x89527980(frames, channels), through the store's heap).  The override
// word, which every lookup of the store takes over the number it is given when it is 0 or more, is put at -1, so the
// number finds the wave (the machine's RAM starts at 0, which would find wave 0 whatever number were given)
struct Rebuild { uint32_t at, overrideWord, none, done; };
constexpr Rebuild kRebuild { 0x894a1ef0, 0x89ce87f8, 0xffffffff, 1 };
// [FW] the loop setter, (&number, frame, which end, the backward loop's), as the Loop FWD and BWD screens' sets call it
// (0x894a48e0 … 0x894a4940): the vplp's word (+4 and +8 the forward loop's start and end, +0xc and +0x10 the backward's
// end and start) written, the record marked changed (+0x50), the cached loop events built again (0x894a1960).  It
// reads the number through a word of the harness's
struct LoopSet { uint32_t at, numberWord; };
constexpr LoopSet kLoopSet { 0x894a4e30, xtfw::kSamp + 0x80 };
// [FW] the edit engine and its Truncate, as the edit screen runs them (0x89380e00, function 1): the function's object
// made for a wave (0x894a8990(engine, wave, function): the one at 0x89cecf10, built by 0x894a9d00 from the store's
// entry for key wave + 3000), given the range (0x894a8b80(engine, Edit Start, Edit End, Current): 1, or -4 where the
// screen asks CANCEL or EXECUTE, else refused), run (0x894a8c00) and finished (0x894a8c70).  The engine, the screen's
// global's (*0x899687c4), is a few words of the harness's
struct EditEngine { uint32_t make, range, run, finish, engine, truncate; };
constexpr EditEngine kEdit { 0x894a8990, 0x894a8b80, 0x894a8c00, 0x894a8c70, xtfw::kSamp + 0xc0, 1 };
constexpr int32_t kEditGiven = 1;   // -4, where the screen would ask, none of the waves here answers: refused, so loudly
// [HW] the XT's word at address 0x10, in its boot flash: the vpfl's builder (0x894a06d0) reads each block's size
// through the entry's pointer unchecked, so for a block the wave has none of it reads this, 9 in every vpfl the XT
// wrote for a SOLO wave with no vmdl (the user's project, analysis/projects/Emu_test.prj)
constexpr uint32_t kLowPage = 0, kLowPageBytes = 0x1000, kLowWord = 0x10, kLowWordIs = 9;
// Where the harness puts them: the path, the entry, then each block's buffer, below the machine's heap
constexpr uint32_t kPath = xtfw::kSamp, kEntry = xtfw::kSamp + 0x100, kBuffers = xtfw::kSamp + 0x200;
constexpr char kPathText[] = "A:\\WAVE.WAV";

void answerStoreHeap(Sh4& c) {
    c.hook(kStoreHeap.resize, [](Sh4& s) { s.ret(s.r[4]); });   // (block, header, size): the block, in place
    c.hook(kStoreHeap.free, [](Sh4& s) { s.ret(0); });
}

// Each block the wave has in its own buffer from kBuffers, the entry pointing at it and it at the entry, with its
// kind's vtable, the data's last; false if they don't fit
bool putBlocks(Sh4& c, const XtWaveBlocks& w) {
    uint32_t at = kBuffers;
    for (size_t k = XtWaveBlocks::kVmdl; k < XtWaveBlocks::kCount; ++k) {
        if (!w.bytes[k]) continue;
        const auto n = static_cast<uint32_t>(w.bytes[k]->size());
        const bool data = k == XtWaveBlocks::kData;
        const uint32_t from = data ? kDataAt : kBytesAt;
        const uint32_t size = data ? n + kDataSizeMore : n;
        if (size > xtfw::kHeap - at - from) return false;
        c.wr32(at + kSizeAt, size);
        c.wr32(at + kVtableAt, kBlockAt[k].vtable);
        c.wr32(at + kEntryAt, kEntry);
        c.write(at + from, w.bytes[k]->data(), n);
        if (data) c.wr32(at + kSamplesAt, at + kDataAt);
        c.wr32(kEntry + kBlockAt[k].pointer, at);
        at = (at + from + size + 15) & ~15u;
    }
    return true;
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

// A failed call's error and where it was called from (its PR)
std::string failedAt(const Sh4& c) {
    char from[24];
    std::snprintf(from, sizeof from, " (pr %08x)", c.pr);
    return c.error() + from;
}

// The wave truncated by rom.2's edit engine, as the screen's EXECUTE runs it; false, and why (the XT's answer), if it
// refuses or a call fails
bool truncate(Sh4& c, const XtWaveCut& cut, std::string* why) {
    uint32_t given = 0;
    const auto from = (uint32_t)cut.from, to = (uint32_t)cut.to;
    bool ran = c.call(kEdit.make, { kEdit.engine, kWave, kEdit.truncate }, xtfw::kStack);
    ran = ran && c.call(kEdit.range, { kEdit.engine, from, to, from }, xtfw::kStack, &given);
    const bool takes = (int32_t)given == kEditGiven;
    ran = ran && (!takes || (c.call(kEdit.run, { kEdit.engine }, xtfw::kStack) &&
                             c.call(kEdit.finish, { kEdit.engine }, xtfw::kStack)));
    if (ran && takes) return true;
    if (why) *why = ran ? "the XT refuses the Truncate: " + std::to_string((int32_t)given)
                        : "the firmware's Truncate failed: " + failedAt(c);
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
    answerStoreHeap(c);
    if (!putBlocks(c, w)) return fail(why, "the wave doesn't fit the machine");
    c.wr32(kEntry + kNode + kNumberAt, kNumber);   // its node alone in its list (no next)
    c.wr32(kWaves + 4 * (kNumber & 0x7f), kEntry + kNode);
    c.wr32(kRebuild.overrideWord, kRebuild.none);
    if (xtWaveHasBands((*vpsc)[1]) && !rebuildBands(c, why)) return false;
    // [HW] open: rom.2's Truncate takes a wave's audio through the data block's +0x1c, which on a band wave its load
    // has put the bands over (band 0 kept as samples, in the harness): refused, until a capture says what the XT does
    if (edits.cut && xtWaveHasBands((*vpsc)[1])) return fail(why, "a band wave's Truncate is open");
    if (edits.cut && !truncate(c, *edits.cut, why)) return false;       // v487: on the wave as loaded, as on the XT
    if (edits.loops && !setLoops(c, *edits.loops, why)) return false;   // v486: after it, as its screen sets them
    return xtfw::runWriter(c, { kWriter, kPath, kPathText, kNumber }, file, why);
}
