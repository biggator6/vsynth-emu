// A wave saved by rom.2's own writer: XtFwWaveFile.h (v482; loaded first by the XT's own loader, v488).
#include "XtFwWaveFile.h"
#include "XtFirmware.h"
#include "XtFwFileOut.h"
#include "XtFwMachine.h"
#include "XtFwWaveLoad.h"
#include <array>
#include <cstdio>
#include <memory>

using xtfile::fail;

namespace {
// [FW] the writer, (path, the wave's key).  The store keys wave n as n + 3000 (0x894a9d00, 0x8949f380): the harness's
// one wave is wave 0, key 3000
constexpr uint32_t kWriter = 0x89496590, kWave = 0, kNumber = kWave + 3000;
// [FW] a wave's type, (key): its vpsc's byte 1 (0x8949e800, which Truncate's object reads)
constexpr uint32_t kTypeOf = 0x8949e800;
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
// The paths the harness gives: the one written, the one loaded (its name the wave's)
constexpr uint32_t kPath = xtfw::kSamp;
constexpr char kPathText[] = "A:\\WAVE.WAV", kLoadPath[] = "A:\\001 WAVE.WAV";

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

bool xtFwWaveFile(const std::vector<uint8_t>& wave, std::vector<uint8_t>& file, std::string* why,
                  const XtWaveEdits& edits) {
    file.clear();
    auto m = std::make_unique<xtfw::Machine>(xtFwRomPath(), wave.size() / 2);
    if (!m->ok) return fail(why, "no firmware image");
    Sh4& c = m->cpu;
    c.mapRam(kLowPage, kLowPageBytes);
    c.wr32(kLowWord, kLowWordIs);
    xtfw::WaveIn in { wave, kLoadPath };
    if (!xtfw::loadIntoStore(c, in, kNumber, why)) return false;
    uint32_t type = 0;
    if (!c.call(kTypeOf, { kNumber }, xtfw::kStack, &type)) return fail(why, "the wave's type unread");
    // [HW] open: rom.2's Truncate takes a wave's audio through the data block's +0x1c, which on a band wave its load
    // has put the bands over (band 0 kept as samples): refused, until a capture says what the XT does
    if (edits.cut && xtWaveHasBands((int)type)) return fail(why, "a band wave's Truncate is open");
    if (edits.cut && !truncate(c, *edits.cut, why)) return false;       // v487: on the wave as loaded, as on the XT
    if (edits.loops && !setLoops(c, *edits.loops, why)) return false;   // v486: after it, as its screen sets them
    return xtfw::runWriter(c, { kWriter, kPath, kPathText, kNumber }, file, why);
}
