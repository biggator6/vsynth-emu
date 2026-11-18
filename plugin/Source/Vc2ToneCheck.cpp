// xt_vc2tone <rom.4> [cases] [seed] (v228, v229): the VC-2's tone, Vc2Tone and Vc2TonePool, against rom.4's own routines
// on its pool (0x8999f524), every part routine hooked to log and answer as drawn (Vc2ToneFakes.cpp), the port's parts
// the same, one a tone (Vc2TonePortParts.cpp).  Each case starts both pools fresh, then 400 events on up to 8 tones at
// once (Vc2ToneEvents.cpp): a tone made (0x893b2db0(0, id): the first take links the pool, a full pool gives none) or
// built where it stands; its note-on (0x893b3390, v229) on an input drawn in the interpreter's RAM, the legato's tone and
// the lists of tones of the same id drawn with it; unmade (0x893b2fc0, back to the pool or not); updated (0x893b4940);
// rendered whole (0x893b4b10) or not (0x893b4bb0); clocked (0x893b4c20); counted (0x893b4c70) or by a mask
// (0x893b4ca0); asked (0x893b4c00, 0x893b4040); started (0x893b4050); installed (0x893b4060); or its head and tail
// scribbled.  Before each, the hooked routines' answers and the tone's part words drawn and written on both sides
// (Vc2ToneSides.cpp).  After each, the pool's words, the tones' own and part words, the input, the free objects, the
// calls and the answers.  XT_VC2TONE_FIRST=1 says where the first case to differ does, XT_VC2TONE_STOP=1 stops there.
// XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2ToneMixTables.h"
#include "XtEnvelope.h"
#include "XtLfo.h"
#include <algorithm>

namespace {
constexpr uint32_t kHeadAt = 0x8993fb48;   // [FW] the pool's head and serial, cleared at a case's start
// [FW] rom.4's static initializers the real parts read: the envelope's start coefficient, the LFOs' generator, the mix's
// member pointers
constexpr uint32_t kStaticInits[] = { 0x893ac4a0, 0x893ad8d0, 0x893e4960 };
constexpr int kSteps = 400, kMaxLive = 8;

// Both pools fresh: rom.4's tones' memory, head and serial cleared, the port's pool new and its parts' words with it
void toneFreshPools(ToneSides& s) {
    const std::vector<uint8_t> zeros(Vc2TonePool::kTones * Vc2Tone::kSize);
    s.fw.cpu.write(Vc2TonePool::kBase, zeros.data(), zeros.size());
    s.fw.cpu.write(kHeadAt, zeros.data(), 2 * sizeof(uint32_t));
    s.pool = Vc2TonePool {};
    for (TonePortParts& p : s.parts) {
        p.words = {};
        p.mods = {};
        p.mix = {};
    }
    s.live.clear();
}

// A case: both pools fresh, then its events; one case in four lets more tones live than the pool holds
std::string vc2ToneCase(ToneSides& s, std::mt19937& rng) {
    toneFreshPools(s);
    const bool fills = rng() % 4 == 0;
    const int most = fills ? Vc2TonePool::kTones + 2 : kMaxLive;
    const std::span<const ToneEvent> events = toneEvents();
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k) {
        const bool make = s.live.empty() || ((int)s.live.size() < most && rng() % (fills ? 2 : 6) == 0);
        what = make ? toneMakeOn(s, rng) : events[rng() % events.size()](s, rng);
    }
    return what.empty() ? toneMixRingsDifference(s) : what;
}

// The mix's constants from rom.4; XtEnvelope's and XtLfo's tables from the rom.2 beside it: rom.4's are the same words as
// far as they're read
bool toneLoadTables(const char* path) {
    std::string rom2 = path;
    const size_t at = rom2.rfind("rom.4");
    if (at != std::string::npos) rom2.replace(at, 5, "rom.2");
    return vc2LoadToneMix(path) && xtEnvLoadTables(rom2.c_str()) && xtLfoLoadTables(rom2.c_str());
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2tone", toneLoadTables);
    if (!run) return 1;
    for (uint32_t init : kStaticInits) {
        if (!fw.call(init, {})) return 1;
    }
    Vc2TonePool pool;
    ToneShared shared;
    shared.pool = &pool;
    std::vector<TonePortParts> parts;
    parts.reserve(Vc2TonePool::kTones);
    for (int tone = 0; tone < Vc2TonePool::kTones; ++tone) parts.emplace_back(tone, shared);
    TonePortOwner owner(shared, parts);
    std::vector<ToneCall> rom;
    ToneSides s { fw, pool, shared, parts, owner, rom };
    toneInstallRom(fw, shared.draws, rom);
    Vc2Tally tally { "XT_VC2TONE_FIRST", "XT_VC2TONE_STOP" };
    vc2RunCases(*run, tally, toneDifference(s, -1), s.n.events,
                [&](std::mt19937& rng, bool) { return vc2ToneCase(s, rng); });
    const ToneCounts& n = s.n;
    std::printf("tone: %d cases, %ld events, %ld made, %ld none, %ld built in place, %ld unmade (%ld back), "
                "%ld updates, %ld ended, %ld renders, %ld different\n", run->cases, n.events, n.makes, n.full,
                n.inPlace, n.unmakes, n.back, n.updates, n.ends, n.renders, tally.different);
    std::printf("note-on: %ld note-ons, %ld players taken, %ld LFOs following, %ld free LFOs, %ld inputs fixed\n",
                n.noteOns, n.takes, n.follows, n.frees, n.fixes);
    std::printf("modulators: %ld reads of their bound words\n", n.bounds);
    std::printf("mix: %ld plays, blocks by routine:", n.mixPlays);
    for (long blocks : n.mixBlocks) std::printf(" %ld", blocks);
    std::printf("\n");
    std::printf("xt_vc2tone: %ld different\n", tally.different);
    return 0;
}
#endif
