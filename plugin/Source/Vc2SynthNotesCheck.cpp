// xt_vc2synth's notes part (v346): the part's MIDI note path against rom.4's own, after the load part and before the
// reads part, on the seed three after the run's.  [cases] cases (16 at the least), each a patch loaded on both sides
// (a store of noise or none, the part's read, the system's dispatchers and part 0's message, its node the held class;
// v349: a sounding tone's VOCODER OSC keeping its Type, vocoderTypesKept; robot human depth 0 one time in 4),
// then 40 MIDI messages, each as rom.4 takes it: the OS MIDI's handler 0x89314e90 then the notes' task's dispatcher
// 0x8938fed0, each given (0, the message, its word), on rom.4's side; `Vc2Synth::midiMessage` on the part's.  A
// message: a note-on (two in five: a key of a few near the middle, a velocity 1…127, now and then 0), a note-off (a key
// one of the same few), a controller (one the part has a class for, or any number below 128; a value below 128), a
// bend, either pressure or a program change; on source 1 mostly (2, 3 now and then), the channel part 0 receives on
// mostly (any now and then); or one in six a parameters' message (0x40, 0x41: its word a kind 0…13, 3 with Out DC cut
// one time in four, and a second byte) or the reset (0x70), the OS's routines these run on rom.4's side
// (Vc2OsMidiParts::kOsRoutines) hooked as the part leaves them; v349: one event in 13 an update pass instead
// (updatePassEvent).  After each every region of the part, the vocoder, the flags posted the notes' task and each
// tone's words.  v360: two patch steers on a generator of their own (NotesSteers), and after a case the receive
// switch's case (vc2SynthReceiveOff, Vc2SynthTasksCheck.cpp) on another.  v363: run a second time on the factory
// patches, each case's store the next of them, a case for each at the least, on seeds of their own (the run's 17, 25
// and 27 after).  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2Params.h"
#include "Vc2PatchStore.h"
#include "Vc2SynthCheck.h"
#include "Vc2SynthMidi.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {
constexpr uint32_t kHandler = 0x89314e90, kDispatcher = 0x8938fed0;   // [FW]
constexpr uint32_t kMessageAt = 0x8C170000, kWordAt = 0x8C170010;      // in Vc2Fw's RAM: a message, its word
constexpr int kEvents = 40, kKeys = 6, kWordKinds = 14;
// The controllers drawn mostly: those the part has a class for (Vc2PartNumbers' rows) and the modulation wheel
constexpr uint8_t kControllers[] = { 1, 5, 6, 7, 10, 11, 38, 64, 65, 66, 68, 71, 72, 73, 74, 84, 91, 93,
                                     120, 121, 123, 124, 125, 126, 127 };
// A message's kind by a draw of 24: the note-on, note-off, controller, bend, poly pressure, pressure, program change,
// the parameters' message, the part's, the reset
constexpr std::array<uint8_t, 24> kKindByDraw { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 4, 5, 6, 7, 7, 8,
                                                9 };
constexpr uint8_t kClasses[] = { 0x90, 0x80, 0xb0, 0xe0, 0xa0, 0xd0, 0xc0, 0x40, 0x41, 0x70 };   // [FW] by kind
constexpr size_t kWordFrom = 7, kResetKind = 9;   // the kinds given a word, the reset's

constexpr uint32_t kFactorySeeds = 14;   // v363: the factory's cases' seeds, the notes' 14 after theirs
constexpr uint32_t kPassOneIn = 13;   // an event each task's step one time in 13 (v349…v351, Vc2SynthTasksCheck.cpp)

struct NotesCounts {
    long events = 0, voiced = 0;   // voiced: the messages after which a note's age is new (a voice started)
    // v349…v351: the update passes, the notes' task's passes and the blocks among the events, by their ways' order;
    // v354: the clock events, and the schedule's firings in them
    std::array<long, 4> passes {};
    long fired = 0;
    Vc2SynthInput input {};   // v351: a block's input frames, the part given them for it
    long sounding = 0, unfinite = 0;   // v351: the blocks whose frames weren't all 0, held a word not finite
    long under = 0, kept = 0;   // v349: the patches drawn under a VOCODER's tone, the Types kept (vocoderTypesKept)
    std::array<long, std::size(kClasses)> byKind {};
    long fixups = 0, cosmsKept = 0, offCases = 0;   // v360: the steers' patches, the receive switch's cases
};

// A message drawn, and its word: its status's class by kind, its data, its source; the parameters' messages a word of
// a kind 0…13 (3, the system's, with Out DC cut, one time in four) and its second byte (0 half the time)
using Drawn = Vc2SynthDrawn;   // v355: shared with the whole part
Drawn messageDrawn(std::mt19937& rng, uint8_t receiving) {
    const size_t kind = kKindByDraw[rng() % kKindByDraw.size()];
    const uint8_t channel = rng() % 8 == 0 || kind >= kWordFrom ? (uint8_t)(rng() % 16) : receiving;
    const uint8_t key = (uint8_t)(57 + 3 * (rng() % kKeys));
    const uint8_t number = rng() % 4 ? kControllers[rng() % std::size(kControllers)] : (uint8_t)(rng() % 128);
    Drawn d;
    d.kind = kind;
    d.m.status = (uint8_t)(kClasses[kind] | (kind < kWordFrom ? channel : 0));
    d.m.data1 = kind <= 1 ? key : kind == 2 ? number : (uint8_t)(rng() % 128);
    d.m.data2 = (uint8_t)(rng() % 128);
    if (kind == 0 && d.m.data2 == 0 && rng() % 4) d.m.data2 = (uint8_t)(1 + rng() % 127);
    d.m.source = rng() % 8 == 0 ? (uint8_t)(2 + rng() % 2) : 1;
    const uint32_t wordKind = rng() % 4 == 0 ? 3 : rng() % kWordKinds, second = rng() % 2 ? 0 : rng() % 256;
    if (kind >= kWordFrom && kind != kResetKind) d.word = wordKind | second << 8;
    return d;
}

// [FW] v349: OSC 1's Type (OSC 2's 52 IDs on), its VOCODER, and the kind an OSC voice of VOCODER holds; robot human
// depth (the part's block)
constexpr int kOscTypeId = 0x2e5, kOscTypeStep = 52, kOscs = 2, kRobotId = 0x1a3;
constexpr uint32_t kVocoderType = 3, kVocoderKind = 5, kRobotZeroOneIn = 4;

// An ID's bits in the store at a zone (v360: and a COSM's line) set to `raw`, where xtStoreRaw reads them (its value
// raw less the centre)
void rawPut(std::vector<uint8_t>& store, int id, int zone, uint32_t raw, int cosm = 0) {
    const XtParamDesc* d = vc2ParamDesc(id);
    const int section = xtStoreSection(vc2Store(), id, zone, cosm);
    if (d == nullptr || section < 0) return;
    const int width = std::abs(d->width), total = width + d->bit, bytes = (total + 7) / 8, at = section + d->offset;
    if (at < 0 || at + bytes > (int)store.size() || width >= 32) return;
    const int shift = bytes * 8 - total;
    const uint32_t mask = ((1u << width) - 1) << shift;
    uint32_t v = 0;
    for (int k = 0; k < bytes; ++k) v = v << 8 | store[(size_t)(at + k)];
    v = (v & ~mask) | (raw << shift & mask);
    for (int k = bytes - 1; k >= 0; --k, v >>= 8) store[(size_t)(at + k)] = (uint8_t)v;
}

// v349's steer, the user's call (2026-11-24, "Defer it"; VC2_MODE_PLAN.md's [deferred] item on the VOCODER's NaN
// sign): a new patch keeps an OSC's Type at VOCODER in the kind of a tone on the sounding list (its id's high half)
// whose OSC voice there is VOCODER's (kind 5), and is a store then.  Changed under such a tone (to PCM), the update
// pass converts its note's block by the new type, PCM's converter writing −1 at P+0x48 (the block's +0xf8), which kind
// 5's update 0x893afff0 reads as its balance envelope depth: a NaN whose sign the interpreter's FSUB keeps and
// xtf::sub flips.  Every other type change, and every other note, is drawn as before.  Whether such a tone sounds;
// `changed` counts the Types it set back to VOCODER
bool vocoderTypesKept(Vc2Synth& s, std::vector<uint8_t>& store, long& changed) {
    bool under = false;
    int n = 0;
    for (uint32_t at = s.lists.sounding; at != 0 && n < Vc2TonePool::kTones; at = s.tones.at(at).head.next, ++n) {
        const int kind = (int)(s.tones.at(at).head.id >> 16);
        for (int osc = 0; osc < kOscs; ++osc) {
            if (s.hostOf(at).oscs[(size_t)osc].kind() != kVocoderKind) continue;
            const int id = kOscTypeId + kOscTypeStep * osc;
            under = true;
            if (xtStoreValue(vc2Store(), store, id, kind) == (int)kVocoderType) continue;
            const XtParamDesc* d = vc2ParamDesc(id);
            rawPut(store, id, kind, kVocoderType + (uint32_t)(d ? d->centre : 0));
            ++changed;
        }
    }
    return under;
}

// v349: robot human depth 0 one time in 4 (v350; 8 until then), so that the pitch source's post (0x89394a60) is seen on
// Auto Note's On Switch alone, its depth 0 (the depth is read with an EXTIN OSC, Auto Note's switch too; noise gives 0
// one time in 128)
void robotDepthDrawn(std::vector<uint8_t>& store, std::mt19937& rng) {
    if (rng() % kRobotZeroOneIn != 0) return;
    const XtParamDesc* d = vc2ParamDesc(kRobotId);
    rawPut(store, kRobotId, 0, (uint32_t)(d ? d->centre : 0));
}

// The channel part 0's node of source 1 receives on (0 when none, or every one): where the notes go mostly
uint8_t receivingOf(const Vc2Synth& s) {
    const int8_t c = s.nodes.of(1, 0).channel;
    return c < 0 ? 0 : (uint8_t)c;
}

// One message on both sides, then what differs
std::string partMidiEvent(Vc2Fw& fw, Vc2Synth& s, const Drawn& m, const Vc2SynthPosts& posts) {
    fw.cpu.write(kMessageAt, &m.m, sizeof m.m);
    fw.cpu.wr32(kWordAt, m.word);
    if (!fw.call(kHandler, { 0, kMessageAt, kWordAt })) return "the OS MIDI's handler faults: " + fw.cpu.error();
    if (!fw.call(kDispatcher, { 0, kMessageAt, kWordAt })) return "the dispatcher faults: " + fw.cpu.error();
    s.midiMessage(m.m, m.word);
    return vc2SynthEventDifference(fw, s, posts);
}

// An event of the notes part: an update pass, a notes' task's pass or a block one time in 13 each, else a message
struct NotesStep {
    Vc2Fw& fw;
    Vc2Synth& s;
    std::mt19937& rng;
    Vc2SynthPosts& posts;
    NotesCounts& n;
};
using PassWay = std::string (*)(NotesStep&);
std::string updateWay(NotesStep& t) { return vc2SynthUpdatePass(t.fw, t.s, t.rng, t.posts); }
std::string notesWay(NotesStep& t) { return vc2SynthNotesTaskPass(t.fw, t.s, t.rng, t.posts); }
std::string blockWay(NotesStep& t) {
    std::string d = vc2SynthMixerBlock(t.fw, t.s, t.rng, t.n.input, t.posts);
    const auto heard = [](float x) { return x != 0.0f; };
    const auto unfinite = [](float x) { return !std::isfinite(x); };
    t.n.sounding += std::ranges::any_of(t.s.frames, heard) ? 1 : 0;
    t.n.unfinite += std::ranges::any_of(t.s.frames, unfinite) ? 1 : 0;
    return d;
}
std::string clockWay(NotesStep& t) {   // v354
    const uint32_t before = t.s.schedule.count;
    std::string d = vc2SynthClockEvent(t.fw, t.s, t.rng, t.posts);
    t.n.fired += t.s.schedule.count - before;
    return d;
}
constexpr PassWay kPassWays[] = { updateWay, notesWay, blockWay, clockWay };   // NotesCounts::passes' order

std::string notesStepEvent(NotesStep& t) {
    const uint32_t pick = t.rng() % kPassOneIn;
    if (pick >= std::size(kPassWays)) {
        const Drawn m = messageDrawn(t.rng, receivingOf(t.s));
        ++t.n.byKind[m.kind];
        return partMidiEvent(t.fw, t.s, m, t.posts);
    }
    ++t.n.passes[pick];
    return kPassWays[pick](t);
}

// v360: the notes part's steers, on a generator of their own (the run's seed + 13), so no other draw moves. One patch
// in 4 the note-on's fix-up's case (0x893b3870: the MOD Type OSC SYNC, OSC 2 ANALOG on LA-SAW, the analog's code 10),
// in both kinds, which never came of noise (svo-noteon-back's reach). One in 2 both COSMs of each kind on one Switch and
// Type, a sounding kind's as the last patch had its COSM 1, so an update's new block keeps its object's code and
// COSM 2's block can land on COSM 1 (svo-block-cosm's)
constexpr int kModTypeId = 0x2d5, kOsc2TypeId = 0x319, kOsc2WaveId = 0x325, kCosmSwitchId = 0x462;   // [FW]
constexpr int kCosmTypeId = 0x463, kSteeredKinds = 2, kCosms = 2;                                     // [FW]
constexpr uint32_t kOscSync = 4, kAnalog = 0, kLaSaw = 9, kFixupOneIn = 4, kCosmsOneIn = 2;          // [FW] values
struct NotesSteers {
    std::mt19937 own;
    std::vector<uint8_t> last;   // the last patch's store
    NotesCounts& n;
    void fixup(std::vector<uint8_t>& bytes) {
        ++n.fixups;
        for (int kind = 0; kind < kSteeredKinds; ++kind) {
            rawPut(bytes, kModTypeId, kind, kOscSync);
            rawPut(bytes, kOsc2TypeId, kind, kAnalog);
            rawPut(bytes, kOsc2WaveId, kind, kLaSaw);
        }
    }
    void cosmsKept(Vc2Synth& s, std::vector<uint8_t>& bytes) {
        ++n.cosmsKept;
        std::array<bool, kSteeredKinds> sounding {};
        int t = 0;
        for (uint32_t at = s.lists.sounding; at != 0 && t < Vc2TonePool::kTones; at = s.tones.at(at).head.next, ++t) {
            const uint32_t kind = s.tones.at(at).head.id >> 16;
            if (kind < kSteeredKinds) sounding[kind] = true;
        }
        for (int kind = 0; kind < kSteeredKinds; ++kind) {
            const std::vector<uint8_t>& from = sounding[(size_t)kind] && !last.empty() ? last : bytes;
            for (const int id : { kCosmSwitchId, kCosmTypeId }) {
                const auto raw = (uint32_t)xtStoreRaw(vc2Store(), from, id, kind, 0);
                for (int cosm = 0; cosm < kCosms; ++cosm) rawPut(bytes, id, kind, raw, cosm);
            }
        }
    }
    void steer(Vc2Synth& s, std::vector<uint8_t>& bytes) {
        if (own() % kFixupOneIn == 0) fixup(bytes);
        if (own() % kCosmsOneIn == 0) cosmsKept(s, bytes);
        last = bytes;
    }
};

// v363: a factory patch for a case's store, as it is but for a sounding VOCODER OSC's Type kept (v349's deferral)
bool factoryGiven(Vc2Synth& s, std::vector<uint8_t>& bytes, const std::vector<uint8_t>& patch, NotesCounts& n) {
    bytes = patch;
    n.under += vocoderTypesKept(s, bytes, n.kept) ? 1 : 0;
    return true;
}
} // namespace

Vc2SynthDrawn vc2SynthMessageDrawn(std::mt19937& rng, const Vc2Synth& s) { return messageDrawn(rng, receivingOf(s)); }

void vc2SynthRawPut(std::vector<uint8_t>& store, int id, int zone, uint32_t raw) { rawPut(store, id, zone, raw); }

bool vc2SynthSteered(Vc2Synth& s, std::vector<uint8_t>& bytes, std::mt19937& rng, long& under, long& kept) {
    robotDepthDrawn(bytes, rng);
    const bool k = vocoderTypesKept(s, bytes, kept);
    under += k ? 1 : 0;
    return k;
}

long vc2SynthNotesPart(Vc2Fw& fw, Vc2Synth& s, const Vc2Run& run, Vc2SynthPosts& posts,
                       const Vc2SynthPatches* factory) {
    const uint32_t own = factory != nullptr ? kFactorySeeds : 0;   // v363: the factory's cases on seeds of their own
    const Vc2Run cases { factory != nullptr ? std::max(run.cases, (int)factory->size()) : run.cases, run.seed };
    NotesCounts n;
    NotesSteers steers { std::mt19937((uint32_t)run.seed + 13 + own), {}, n };   // v360
    std::mt19937 receiveOwn((uint32_t)run.seed + 11 + own);                       // v360: the receive switch's case's
    size_t next = 0;
    const auto oneCase = [&](std::mt19937& rng, std::vector<uint8_t>& store) {
        const auto kept = [&s, &n, &rng, &steers, factory, &next](std::vector<uint8_t>& bytes) {
            if (factory != nullptr) return factoryGiven(s, bytes, (*factory)[next++ % factory->size()], n);
            const bool k = vc2SynthSteered(s, bytes, rng, n.under, n.kept);
            steers.steer(s, bytes);
            return k;
        };
        std::string d = vc2SynthPatchLoaded(fw, s, store, rng, posts, kept);
        NotesStep step { fw, s, rng, posts, n };
        for (int e = 0; e < kEvents && d.empty(); ++e) {
            ++n.events;
            const uint32_t age = s.voices.ages;
            d = notesStepEvent(step);
            n.voiced += s.voices.ages != age && s.voices.ages != 0 ? 1 : 0;
        }
        if (d.empty()) d = vc2SynthReceiveOff(fw, s, posts, receiveOwn, n.offCases);   // v360
        return d;
    };
    const char* what = factory != nullptr ? "factory's" : "notes";
    const long different = vc2SynthCases(fw, s, cases, 3 + own, factory != nullptr ? "the factory's" : "the notes",
                                         oneCase);
    std::printf("synth %s: %d cases, %ld events: %ld note-ons, %ld note-offs, %ld controllers, %ld bends, %ld poly "
                "pressures, %ld pressures, %ld programs, %ld parameters', %ld the part's, %ld resets, %ld update "
                "passes, %ld notes' task's passes, %ld blocks (%ld sounding, %ld not finite), %ld clock events (%ld "
                "schedules); %ld voiced; %ld patches under a VOCODER's tone, %ld Types kept; v360: %ld patches on the "
                "fix-up's case, %ld with their COSMs' types kept, %ld cases with the receive switch off\n",
                what, vc2SynthCaseCount(cases), n.events, n.byKind[0], n.byKind[1], n.byKind[2], n.byKind[3],
                n.byKind[4], n.byKind[5], n.byKind[6], n.byKind[7], n.byKind[8], n.byKind[9], n.passes[0], n.passes[1],
                n.passes[2], n.sounding, n.unfinite, n.passes[3], n.fired, n.voiced, n.under, n.kept, n.fixups,
                n.cosmsKept, n.offCases);
    return different;
}
#endif
