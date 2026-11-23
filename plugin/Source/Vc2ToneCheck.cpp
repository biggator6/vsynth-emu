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
// calls and the answers.  From v319 every part is real, rom.4's own against the host's (Vc2ToneHost), the OSC voices'
// calls beyond the tone xt_vc2osc's hooks and fake parts.  XT_VC2TONE_FIRST=1 says where the first case to differ does,
// XT_VC2TONE_STOP=1 stops there.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2CarrierTables.h"
#include "Vc2CheckRom2.h"
#include "Vc2Kind6Tables.h"
#include "Vc2OscDraws.h"
#include "Vc2OscTables.h"
#include "Vc2RampTables.h"
#include "Vc2ToneMixTables.h"
#include "Vc2TvaTables.h"
#include "Vc2CosmTables.h"
#include "XtCosm.h"
#include "XtEnvelope.h"
#include "XtLfo.h"
#include "XtMultiBandPlayer.h"
#include <algorithm>

namespace {
constexpr uint32_t kHeadAt = 0x8993fb48;   // [FW] the pool's head and serial, cleared at a case's start
// [FW] rom.4's static initializers the real parts read: the envelope's start coefficient, the LFOs' generator, the mix's
// member pointers, (v318) the TB Filter's floats and (v319) the OSC vibrato's scale and glide
constexpr uint32_t kStaticInits[] = { 0x893ac4a0, 0x893ad8d0, 0x893e4960, 0x893b98e0, 0x893b2d60 };
constexpr int kSteps = 400, kMaxLive = 8;

// Both pools fresh: rom.4's tones' memory, head and serial cleared, the port's pool new and every tone's parts made
// again (v318: a COSM's sound keeps more than its object's words show)
void toneFreshPools(ToneSides& s) {
    const std::vector<uint8_t> zeros(Vc2TonePool::kTones * Vc2Tone::kSize);
    s.fw.cpu.write(Vc2TonePool::kBase, zeros.data(), zeros.size());
    s.fw.cpu.write(kHeadAt, zeros.data(), 2 * sizeof(uint32_t));
    s.fw.cpu.write(kToneBufferAt, zeros.data(), sizeof s.shared.buffers);   // v318: no case's renders in the next's
    s.shared.buffers = {};
    s.pool = Vc2TonePool {};
    s.parts.clear();
    for (int tone = 0; tone < Vc2TonePool::kTones; ++tone) s.parts.emplace_back(tone, s.shared);
    toneEmptyLists(s);
    s.live.clear();
}

// A case: both pools fresh, then its events; one case in four lets more tones live than the pool holds
std::string vc2ToneCase(ToneSides& s, std::mt19937& rng) {
    toneFreshPools(s);
    const bool fills = rng() % 4 == 0;
    const int most = fills ? Vc2TonePool::kTones + 2 : kMaxLive;
    const std::span<const ToneEvent> events = toneEvents();
    std::string what = toneFreeLfosFresh(s);
    if (what.empty()) what = toneFreshCosms(s);
    if (what.empty()) what = toneOscCase(s, rng);   // v319: the mic ring reset and written whole
    for (int k = 0; k < kSteps && what.empty(); ++k) {
        const bool make = s.live.empty() || ((int)s.live.size() < most && rng() % (fills ? 2 : 6) == 0);
        what = make ? toneMakeOn(s, rng) : events[rng() % events.size()](s, rng);
    }
    return what.empty() ? toneMixRingsDifference(s) : what;
}

// The mix's constants from rom.4; XtEnvelope's and XtLfo's tables from the rom.2 beside it: rom.4's are the same words as
// far as they're read
bool toneLoadTables(const char* path) {
    const std::string rom2 = vc2Rom2Of(path);
    return vc2LoadToneMix(path) && vc2LoadTva(path) && vc2LoadCosm(path) && vc2LoadOsc(path) && vc2LoadRamp(path)
           && vc2LoadCarrier(path) && vc2LoadKind6(path) && xtEnvLoadTables(rom2.c_str())
           && xtLfoLoadTables(rom2.c_str()) && xtCosmLoadTables(rom2.c_str())
           && xtMbLoadTables(rom2.c_str());   // v332: the band players'
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
    shared.osc.renders.seed(~(uint32_t)run->seed);   // v322: the renders' generator of its own, from the run's seed
    shared.pool = &pool;
    const ToneMath math(fw);   // v319: the OSC voices' shared routines, rom.4's own
    shared.math = &math;
    std::deque<TonePortParts> parts;   // each case makes them again (toneFreshPools)
    for (int tone = 0; tone < Vc2TonePool::kTones; ++tone) parts.emplace_back(tone, shared);
    shared.parts = &parts;
    TonePortOwner owner(shared, parts);
    std::vector<ToneCall> rom;
    std::vector<OscCall> oscRom;
    ToneSides s { fw, pool, shared, parts, owner, rom, oscRom };
    shared.osc.bandRng.seed((uint32_t)run->seed * 3 + 1);   // v332: the band players' states, before the hooks
    std::string start = oscCaptureBandStates(fw, shared.osc, oscVoiceAt(0));
    toneInstallRom(fw, rom, shared.fade, shared.osc.draws, oscRom);
    if (start.empty()) start = toneOscSetUp(s);   // v319: the records' exp2 octaves, the carriers' rate, kind 6's globals
    if (start.empty()) start = toneDifference(s, -1);
    Vc2Tally tally { "XT_VC2TONE_FIRST", "XT_VC2TONE_STOP" };
    vc2RunCases(*run, tally, start, s.n.events, [&](std::mt19937& rng, bool) { return vc2ToneCase(s, rng); });
    const ToneCounts& n = s.n;
    std::printf("tone: %d cases, %ld events, %ld made, %ld none, %ld built in place, %ld unmade (%ld back), "
                "%ld updates, %ld ended, %ld renders, %ld different\n", run->cases, n.events, n.makes, n.full,
                n.inPlace, n.unmakes, n.back, n.updates, n.ends, n.renders, tally.different);
    std::printf("note-on: %ld note-ons, %ld players taken, %ld LFOs following, %ld free LFOs, %ld inputs fixed\n",
                n.noteOns, n.takes, shared.follows, shared.takes, n.fixes);
    std::printf("free LFOs: %ld ticks, %ld clocks\n", n.freeTicks, n.freeClocks);
    std::printf("modulators: %ld reads of their bound words\n", n.bounds);
    std::printf("mix: %ld plays, blocks by routine:", n.mixPlays);
    for (long blocks : n.mixBlocks) std::printf(" %ld", blocks);
    std::printf("\n");
    std::printf("methods: %ld setters, %ld key-offs, %ld truncations (%ld waits), %ld costs, %ld getters\n", n.setters,
                n.keyOffs, n.truncations, n.waits, n.costs, n.getters);
    std::printf("lists: %ld starts (%ld tones started), %ld moves alone, %ld left before a rebuild\n", n.starts,
                n.started, n.listMoves, n.leaves);
    std::printf("settings and release: %ld envelopes' settings in sustain, %ld LFOs' fades started, %ld released "
                "envelopes ticked\n", n.sustains, n.releaseFades, n.releaseTicks);
    std::printf("tva: %ld renders whose told fade ended\n", n.told);
    std::printf("cosm: %ld slots taken, %ld freed before a note-on, %ld stale named none, %ld modes kept, %ld whole "
                "renders through COSM 1 (%ld a stereo Side Band Filter)\n", n.cosmSlots, n.cosmFreed, n.cosmStale,
                n.cosmModesKept, n.cosmRenders, n.cosmFollowers);
    const auto& k = n.oscKinds;
    const auto& c = n.oscCalls;
    std::printf("osc: note-ons by kind %ld %ld %ld %ld %ld %ld %ld %ld (%ld OSC 1s steady for a COSM 1 of its pitch), "
                "%ld settings, %ld mic blocks; calls beyond the tone: %ld renders, %ld seeks (%ld VariPhrase re-seeks "
                "asked, v329), %ld players' set-ups (v330)\n", k[0], k[1], k[2], k[3], k[4], k[5], k[6], k[7],
                n.oscSteadied, n.oscSettings, n.micBlocks, c[kRenderBy], c[kSeek], n.bandReseeks,
                c[kObjectCall]);   // v331: no event lookups
    std::printf("own renders: %ld renders with an OSC through the EXT IN player's, %ld through Vari off's, %ld through "
                "LITE's heads'; %ld OSCs' renders through the band players'\n", n.extInBlocks, n.variOffBlocks,
                n.headsBlocks, shared.osc.bandSteers);   // v322, v323, v326, v332
    std::printf("band players: %ld OSC blocks given a band wave, %ld re-seeks a set-up state\n", shared.osc.bandSetUps,
                shared.osc.bandSeeks);   // v333
    std::printf("lite: %ld settings on a type with a way, %ld through an earlier tone's head; %ld inputs of two "
                "VariPhrase OSCs; %ld mode 1 set-ups (%ld through an earlier tone's head, %ld earlier voices' heads "
                "stale), %ld OSCs steered for its seek\n", n.liteSettings, n.settingsTaken, n.bothVari,
                n.modeOneSetUps, n.modeOneTaken, n.staleHeads, n.modeOneSeeks);   // v324, v328, v329, v333
    std::printf("vari: %ld words drawn finite before its update or note-on, %ld note-ons by events\n", n.variDrawn,
                n.variByEvents);   // v330, v331
    std::printf("xt_vc2tone: %ld different\n", tally.different);
    return 0;
}
#endif
