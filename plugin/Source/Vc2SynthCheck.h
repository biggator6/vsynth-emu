#pragma once
// xt_vc2synth's shared parts (v343): a region of the part against rom.4's memory byte for byte, every region in turn,
// and the further parts: C's reads of the part (Vc2SynthReadsCheck.cpp).  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2CheckRig.h"
#include "Vc2SynthMemory.h"
#include <functional>
#include <random>
#include <span>
#include <string>
#include <vector>

// v355: rom.4's start as main runs it, on a fresh image: its initializers (how many in `rows`; the OS's rows main's, or
// `osRows` in the table's order), then the system's start; what faulted, or none
std::string vc2SynthRomStart(Vc2Fw& fw, int& rows, std::span<const uint32_t> osRows = {});
// v362: the disk image's waves on both sides (Vc2SynthWavesCheck.cpp), and the reads past them each side counted: a
// player's sample reads, LITE's loop records
struct Vc2LoadedWaves;
struct Vc2SynthPast {
    long romFar = 0, portFar = 0, romRecords = 0, portRecords = 0;
    long romTables = 0;   // v363: the renders' loads of their own tables, which aren't samples
};
// The waves loaded by rom.4's own loaders from the disk image beside `rom4`, written into `fw` and given `s`; false,
// said, when they can't be
bool vc2SynthWavesBoth(const char* rom4, Vc2Fw& fw, Vc2Synth& s, Vc2LoadedWaves& w, Vc2SynthPast& past);
// v363: the factory patches (the disk image's Internal.vc2/Patch, by rom.4's .p02 reader) as stores; false, said, when
// they can't be read
bool vc2SynthFactoryPatches(const char* rom4, std::vector<std::vector<uint8_t>>& out);
// The waves' bytes into a machine's memory at their addresses, the zero pages past them, rom.4's reads there counted
void vc2SynthWavesWritten(Sh4& c, const Vc2LoadedWaves& w, Vc2SynthPast& past);
// The waves given the part (giveWaves), its reads past the samples counted
void vc2SynthWavesGiven(Vc2Synth& s, const Vc2LoadedWaves& w, Vc2SynthPast& past);
// The part's records past the waves, added before it goes
void vc2SynthPortRecords(const Vc2Synth& s, Vc2SynthPast& past);
// The counts printed; 1 when a side's differ from the other's
long vc2SynthPastDifference(const Vc2SynthPast& past);
// v355: the whole part (Vc2SynthWholeCheck.cpp): a fresh pair on their OSes, the port's model and rom.1's kernel with
// rom.4 (Vc2MachineRig, rom.1 beside `rom4`); how many differ. v362: each pair given `waves`, its reads past them
// counted in `past`
long vc2SynthWholePart(const char* rom4, const Vc2Run& run, const Vc2LoadedWaves& waves, Vc2SynthPast& past);
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
    uint32_t clock = 0;          // v354: what rom.4 raised on the clock task (0x89301840 on 0x8994fdf8)
    uint32_t clockWaited = 0;    // v354: and what the clock task's wait (on mask 0x47) answers
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
// v355: the patch given both sides as vc2SynthPatchLoaded gives it, without its compare (the whole part's, whose posts
// go to its OSes), a system ID of one bit, `offId`, kept off in the system drawn; what faulted, or none
std::string vc2SynthPatchGiven(Vc2Fw& fw, Vc2Synth& s, std::vector<uint8_t>& store, std::mt19937& rng,
                               const Vc2SynthSteer& steer, int offId = -1);
// v355: the notes part's draws (Vc2SynthNotesCheck.cpp), the whole part's too: a message and its word, its kind by
// the notes part's order (on the channel part 0's node of source 1 receives on, mostly); and a store's steer (robot
// human depth 0 one time in 4, a sounding VOCODER's Type kept), counting the patches under such a tone and the Types kept
struct Vc2SynthDrawn {
    Vc2MidiMessage m {};
    uint32_t word = 0;
    size_t kind = 0;
};
Vc2SynthDrawn vc2SynthMessageDrawn(std::mt19937& rng, const Vc2Synth& s);
bool vc2SynthSteered(Vc2Synth& s, std::vector<uint8_t>& bytes, std::mt19937& rng, long& under, long& kept);
// v355: an ID's raw bits put in a store at a zone, where xtStoreRaw reads them (the notes part's own)
void vc2SynthRawPut(std::vector<uint8_t>& store, int id, int zone, uint32_t raw);
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
// v363: with `factory`, its cases each on the next of the factory patches (at least one each), on seeds of their own
using Vc2SynthPatches = std::vector<std::vector<uint8_t>>;
long vc2SynthNotesPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, Vc2SynthPosts& posts,
                       const Vc2SynthPatches* factory = nullptr);

// The tasks' steps (Vc2SynthTasksCheck.cpp, v351), each on both sides, then what differs: after a step every region,
// the vocoder, the events and each tone's words; an update pass (v349); a notes' task's pass (v350); a block on input
// frames drawn into `input` (v351)
using Vc2SynthInput = std::array<int32_t, 32 * 8>;
std::string vc2SynthEventDifference(Vc2Fw& fw, Vc2Synth& s, const Vc2SynthPosts& posts);
std::string vc2SynthUpdatePass(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts);
// v360: its events drawn of `only` alone
std::string vc2SynthNotesTaskPass(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts,
                                  uint32_t only = 0xffffffffu);
// v360: with the part receiving on channel 0 and a voice sounding there, Part 1 Receive Switch off by the routing's
// reset, two update passes, then the notes' task's own poly pressure (on channel −1, where its channel state is the
// word before the table's), the switch on again after; `own` its draws, `cases` counted (kt-state-before's reach)
std::string vc2SynthReceiveOff(Vc2Fw& fw, Vc2Synth& s, Vc2SynthPosts& posts, std::mt19937& own, long& cases);
// v360: a parameters' message (the part's, or any kind's) on both sides, as the load part gives it
std::string vc2SynthMessage(Vc2Fw& fw, Vc2Synth& s, bool part, uint8_t kind, uint8_t second);
std::string vc2SynthMixerBlock(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthInput& input,
                               const Vc2SynthPosts& posts);
// v354: one to three of the clock's ms (0x89312f50), then the clock task's pass (its loop body from 0x89313c36)
std::string vc2SynthClockEvent(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, Vc2SynthPosts& posts);

// The reads part (v343): the regions apart; every read of C's over its arguments reaching only the part's regions; then
// `run.cases` cases, each the part's memory read by address against rom.4's after noise in drawn regions, the Matrix's
// ten readers, the OSC's getters and a sum on drawn Ms against rom.4's, and the children's placements; how many differ
long vc2SynthReadsPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run);
#endif
