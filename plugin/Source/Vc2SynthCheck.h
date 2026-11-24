#pragma once
// xt_vc2synth's shared parts (v343): a region of the part against rom.4's memory byte for byte, every region in turn,
// and the further parts: C's reads of the part (Vc2SynthReadsCheck.cpp).  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2CheckRig.h"
#include "Vc2SynthMemory.h"
#include <functional>
#include <random>
#include <string>
#include <vector>

// The first byte of the region that differs from rom.4's at its address, said; none
std::string vc2SynthBytesDifference(Vc2Fw& fw, const Vc2SynthRegion& r);
// The first region of the part that differs, said; none
std::string vc2SynthRegionsDifference(Vc2Fw& fw, Vc2Synth& s);
// v345: the regions, then the vocoder's engines, stages, engine word and hook word (the start's compare)
std::string vc2SynthDifference(Vc2Fw& fw, Vc2Synth& s);

// v346: a made tone's words against rom.4's (its head, tail, link, OSC voices, TVA, mix, envelopes and LFOs)
std::string vc2SynthToneDifference(Vc2Fw& fw, Vc2Synth& s, int tone);

// v346: what rom.4 posted through the OS's event post, by object (the notes' task's flags; any other), and the OS's
// hooks the parts share: the post kept, its timers, locks, semaphores and clock answering 0
struct Vc2SynthPosts {
    uint32_t notes = 0;
    bool others = false;
    uint32_t updateEvents = 0;   // v349: what the update task's wait (0x893017f0 on mask 3) answers
    uint32_t notesWaited = 0;    // v350: and the notes' task's (on mask 0x3fe)
};
void vc2SynthInstallOs(Vc2Fw& fw, Vc2SynthPosts& posts);
// After a step: every region, the vocoder, and the flags each side posted the notes' task
std::string vc2SynthStepDifference(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthPosts& posts);
// v349: what steers a store's draw: given its bytes before either side has them, whether it must be a store (not none)
using Vc2SynthSteer = std::function<bool(std::vector<uint8_t>&)>;
// A patch loaded on both sides: a store of noise (none one time in four) given each, then the part's read, the
// system's, assignments' and calibration's dispatchers and part 0's message (kind 0: its node the held class); what
// differs after
std::string vc2SynthPatchLoaded(Vc2Fw& fw, Vc2Synth& s, std::vector<uint8_t>& store, std::mt19937& rng,
                                const Vc2SynthPosts& posts, const Vc2SynthSteer& steer);
// The store taken away on both sides (rom.4's part 0 entry none), before the store's bytes go
void vc2SynthPatchDropped(Vc2Fw& fw, Vc2Synth& s);
// A part's cases (v346): as many as the run's, 16 at the least, on the seed `offset` after the run's, each `one` given
// the generator and a store's bytes and saying what differs; the first difference ends them (the two sides have
// parted), said under `what` with XT_VC2SYNTH_FIRST; the store taken away after; how many differ
inline int vc2SynthCaseCount(const Vc2Run& run) { return run.cases < 16 ? 16 : run.cases; }
long vc2SynthCases(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, uint32_t offset, const char* what,
                   const std::function<std::string(std::mt19937&, std::vector<uint8_t>&)>& one);

// The load part (v345, Vc2SynthLoadCheck.cpp): the notes' task's start, then `run.cases` cases of a drawn store (or
// none) and 20 events of the part's readers and messages against rom.4's; how many differ
long vc2SynthLoadPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, const Vc2SynthPosts& posts);
// The notes part (v346, Vc2SynthNotesCheck.cpp): cases of a patch loaded, then MIDI messages through the OS MIDI's
// handler and the dispatcher on both sides; how many differ
long vc2SynthNotesPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, Vc2SynthPosts& posts);

// The tasks' steps (Vc2SynthTasksCheck.cpp, v351), each on both sides, then what differs: after a step every region,
// the vocoder, the events and each tone's words; an update pass (v349); a notes' task's pass (v350); a block on input
// frames drawn into `input` (v351)
using Vc2SynthInput = std::array<int32_t, 32 * 8>;
std::string vc2SynthEventDifference(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthPosts& posts);
std::string vc2SynthUpdatePass(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts);
std::string vc2SynthNotesTaskPass(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts);
std::string vc2SynthMixerBlock(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthInput& input,
                               const Vc2SynthPosts& posts);

// The reads part (v343): the regions apart; every read of C's over its arguments reaching only the part's regions; then
// `run.cases` cases, each the part's memory read by address against rom.4's after noise in drawn regions, the Matrix's
// ten readers, the OSC's getters and a sum on drawn Ms against rom.4's, and the children's placements; how many differ
long vc2SynthReadsPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run);
#endif
