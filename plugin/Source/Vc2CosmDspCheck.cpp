// xt_vc2cosm's seventh part (v263): rom.4's own COSM DSP routines against Vc2CosmDsp.  An object in the interpreter's
// RAM and the port's take the same note-on (0x893aa0e0, the slot it takes holding noise first), install (0x893aafe0),
// new blocks (0x893aa5d0) and updates (0x893aa7d0); then rom.4's routine (the install's, +0x148) and the port's render
// the same frames.  After each render: the frames, the state through XtCosm's getters against rom.4's D words, and now
// and then a ring's samples against the slot's buffer.  A new note on the same object follows a destructor, so what
// one type leaves in the words a note-on doesn't clear reaches the next.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2CosmDsp.h"
#include "Vc2CosmNoteOnDraws.h"
#include <vector>

namespace {
constexpr uint32_t kNoteOn = 0x893aa0e0, kNewBlock = 0x893aa5d0, kUpdate = 0x893aa7d0, kInstall = 0x893aafe0;   // [FW]
constexpr uint32_t kDestroy = 0x893aa060, kPoolsInit = 0x893ab550;   // [FW]
constexpr uint32_t kFramesAt = 0x8C076000, kD = kNoteOnObjectAt + 0x58, kRoutine = kNoteOnObjectAt + 0x148;
constexpr uint32_t kEnvelopeRate = 0x54;   // [FW] a bound envelope's rate
constexpr int kEvents = 60, kFrames = 32, kRingLength = 3266;

struct DspCounts {
    long events = 0, notes = 0, stereo = 0, renders = 0, updates = 0, newBlocks = 0, words = 0, rings = 0;
};

// A word of the state: its place in D, and what the port holds there
struct Vc2CosmDspWord {
    uint32_t at;
    float value;
};
using DspWords = std::vector<Vc2CosmDspWord>;

void wordsFrom(DspWords& w, uint32_t at, const float* v, int n) {
    for (int k = 0; k < n; ++k) w.push_back({ at + 4 * (uint32_t)k, v[k] });
}

// A type's two values and their targets (D+0xbc, +0xc0, D+0xa4, +0xa8) by its getters, and `Words` of state from
// D+0x1c, R's from D+0x54
template <auto Member, auto Value0, auto Value1, auto Target0, auto Target1, int Words>
void twoState(const XtCosmSection& s, bool stereo, DspWords& w) {
    const auto& c = s.*Member;
    w.insert(w.end(), { { 0xbc, (c.*Value0)() }, { 0xc0, (c.*Value1)() }, { 0xa4, (c.*Target0)() },
                        { 0xa8, (c.*Target1)() } });
    wordsFrom(w, 0x1c, c.state(), Words);
    if (stereo) wordsFrom(w, 0x54, c.state() + 14, Words);
}

// The same, with the gain at D+0x8c
template <auto Member, auto Value0, auto Value1, auto Target0, auto Target1, int Words>
void gainedState(const XtCosmSection& s, bool stereo, DspWords& w) {
    twoState<Member, Value0, Value1, Target0, Target1, Words>(s, stereo, w);
    w.push_back({ 0x8c, (s.*Member).gain() });
}

template <int Words>
constexpr auto odState = gainedState<&XtCosmSection::od, &XtCosmOd::drive, &XtCosmOd::tone, &XtCosmOd::driveTarget,
                                     &XtCosmOd::toneTarget, Words>;
constexpr auto wsState = gainedState<&XtCosmSection::ws, &XtCosmWs::drive, &XtCosmWs::tone, &XtCosmWs::driveTarget,
                                     &XtCosmWs::toneTarget, 6>;
constexpr auto resState = twoState<&XtCosmSection::res, &XtCosmRes::size, &XtCosmRes::balance, &XtCosmRes::sizeTarget,
                                   &XtCosmRes::balanceTarget, 12>;
constexpr auto dualState = twoState<&XtCosmSection::dual, &XtCosmDual::c1, &XtCosmDual::c2, &XtCosmDual::target1,
                                    &XtCosmDual::target2, 8>;
constexpr auto tvfState = twoState<&XtCosmSection::tvf, &XtCosmTvf::c1, &XtCosmTvf::c2, &XtCosmTvf::target1,
                                   &XtCosmTvf::target2, 4>;

void lofiState(const XtCosmSection& s, bool stereo, DspWords& w) {
    twoState<&XtCosmSection::lofi, &XtCosmLofi::filter, &XtCosmLofi::balance, &XtCosmLofi::filterTarget,
             &XtCosmLofi::balanceTarget, 6>(s, stereo, w);
    w.insert(w.end(), { { 0x94, s.lofi.rateCoef() }, { 0x98, s.lofi.upScale() }, { 0x9c, s.lofi.downScale() } });
}
void ampState(const XtCosmSection& s, bool stereo, DspWords& w) {
    w.insert(w.end(), { { 0xbc, s.amp.gain() }, { 0xa4, s.amp.gainTarget() } });
    wordsFrom(w, 0x8c, s.amp.slews(), 4);
    wordsFrom(w, 0x1c, s.amp.state(), 10);
    if (stereo) wordsFrom(w, 0x54, s.amp.state() + 14, 10);
}
void spkState(const XtCosmSection& s, bool stereo, DspWords& w) {
    wordsFrom(w, 0x1c, s.spk.state(), 14);
    if (stereo) wordsFrom(w, 0x54, s.spk.state() + 14, 14);
}
void compState(const XtCosmSection& s, bool stereo, DspWords& w) {
    const XtCosmComp& c = s.comp;
    wordsFrom(w, 0x8c, c.slews(), 3);
    w.insert(w.end(), { { 0x98, c.gainTarget(0) }, { 0x9c, c.gain(0) }, { 0xa0, c.envelope(0) } });
    if (stereo) w.insert(w.end(), { { 0xcc, c.gainTarget(1) }, { 0xd0, c.gain(1) }, { 0xd4, c.envelope(1) } });
}
void limState(const XtCosmSection& s, bool stereo, DspWords& w) {
    const XtCosmLim& l = s.lim;
    w.insert(w.end(), { { 0x8c, l.slope() }, { 0x90, l.gain(0) }, { 0x94, l.smooth(0) }, { 0xa0, l.slews()[3] } });
    wordsFrom(w, 0xcc, l.slews(), 3);
    if (stereo) w.insert(w.end(), { { 0x98, l.gain(1) }, { 0x9c, l.smooth(1) } });
}
void fsState(const XtCosmSection& s, bool, DspWords& w) {
    const XtCosmFs& f = s.fs;
    w.insert(w.end(), { { 0xbc, f.depth() }, { 0xc0, f.balance() }, { 0xa4, f.depthTarget() },
                        { 0xa8, f.balanceTarget() }, { 0x94, f.phase() } });
}

// A ring a buffered type keeps, with its write index
struct Vc2CosmDspRing {
    int index;
    const float* left;
    const float* right;
};
template <auto Member>
Vc2CosmDspRing ringOf(const XtCosmSection& s) {
    const auto& c = s.*Member;
    return { c.writeIndex(), c.buffer(0), c.buffer(1) };
}

// A block code Vc2CosmDsp renders: what its object holds, by the place rom.4 holds it, and its ring if it has one
struct Vc2CosmDspKind {
    int32_t code;
    void (*state)(const XtCosmSection& s, bool stereo, DspWords& w);
    Vc2CosmDspRing (*ring)(const XtCosmSection& s);
};
// Through holds nothing: the frames alone say it leaves them as they came
void throughState(const XtCosmSection&, bool, DspWords&) {}

constexpr std::array<Vc2CosmDspKind, 13> kKinds { { { 0, throughState, nullptr },
                                                    { 1, odState<9>, nullptr }, { 2, odState<5>, nullptr },
                                                    { 3, wsState, nullptr }, { 4, ampState, nullptr },
                                                    { 5, spkState, nullptr }, { 6, resState, nullptr },
                                                    { 12, dualState, nullptr }, { 13, tvfState, nullptr },
                                                    { 15, compState, ringOf<&XtCosmSection::comp> },
                                                    { 16, limState, ringOf<&XtCosmSection::lim> },
                                                    { 18, fsState, ringOf<&XtCosmSection::fs> },
                                                    { 19, lofiState, nullptr } } };

struct DspRig {
    explicit DspRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2CosmObjectWords o {};
    Vc2CosmBlockWords block {};
    Vc2CosmPools pools;
    Vc2CosmRamBound bound { fw.cpu };
    Vc2CosmNoteOn frame { pools, bound };
    Vc2CosmDsp dsp;
    const Vc2CosmDspKind* kind = nullptr;   // the note's, none before a case's first
    bool stereo = false;
    DspCounts n;
    void hook() {}
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
    std::string note(std::mt19937& rng);
    std::string stateDifference();
    std::string ringDifference();
};

std::string DspRig::stateDifference() {
    DspWords words;
    kind->state(dsp.section(), stereo, words);
    for (const Vc2CosmDspWord& w : words) {
        ++n.words;
        if (fw.cpu.rd32(kD + w.at) == std::bit_cast<uint32_t>(w.value)) continue;
        char said[64];
        std::snprintf(said, sizeof said, "code %d's D+0x%x", kind->code, w.at);
        return said;
    }
    return "";
}

// New bound levels, the two envelopes' rates as an envelope gives them (0.2…1 of 1/32, as rom.2's check draws them: the
// DSP's values step by 4 × the rate, and diverge past it)
void drawDspLevels(Vc2Fw& fw, const Vc2CosmObjectWords& o, std::mt19937& rng) {
    vc2DrawBound(fw.cpu, rng);
    for (uint32_t binding : { o.bindings[0], o.bindings[1] }) {
        const float rate = (0.2f + 0.8f * vc2Unit(rng)) / 32.0f;
        fw.cpu.wr32(binding + kEnvelopeRate, std::bit_cast<uint32_t>(rate));
    }
}

// The ring's write index and samples, in the buffers the slot gave rom.4
std::string DspRig::ringDifference() {
    ++n.rings;
    const Vc2CosmDspRing ring = kind->ring(dsp.section());
    if ((int32_t)fw.cpu.rd32(kD + 4) != ring.index) return "the ring's write index";
    const size_t bytes = kRingLength * sizeof(float);
    std::string d = vc2RegionsDifference(fw, { { fw.cpu.rd32(kD + 8), ring.left, bytes, "the ring" } });
    if (!d.empty() || !stereo) return d;
    return vc2RegionsDifference(fw, { { fw.cpu.rd32(kD + 0x10), ring.right, bytes, "R's ring" } });
}

// A note on the object: a block of a type rendered, noise in the slot it takes, the note-on and install on both sides
std::string DspRig::note(std::mt19937& rng) {
    ++n.notes;
    kind = &kKinds[rng() % kKinds.size()];
    stereo = rng() % 2 != 0;
    n.stereo += stereo;
    vc2DrawBlockOf(block, kind->code, rng);
    const uint32_t pool = rng() % 2;
    vc2FillFreeSlot(fw.cpu, pools, pool, kind->code, rng);
    drawDspLevels(fw, o, rng);
    fw.cpu.write(kNoteOnBlockAt, &block, sizeof block);
    if (!fw.call(kNoteOn, { kNoteOnObjectAt, kNoteOnBlockAt, pool })) return "the note-on faults: " + fw.cpu.error();
    frame.noteOn(o, block, pool);
    if (!fw.call(kInstall, { kNoteOnObjectAt, stereo ? 1u : 0u })) return "the install faults: " + fw.cpu.error();
    vc2CosmInstall(o, stereo ? 1 : 0);
    dsp.noteOn(o, block, bound, stereo);
    return stateDifference();
}

void DspRig::start(std::mt19937& rng) {
    vc2DrawNoteOnObject(o, rng);
    fw.cpu.write(kNoteOnObjectAt, &o, sizeof o);
    fw.call(kPoolsInit, {});
    pools.init();
    kind = nullptr;
}

using DspEvent = std::string (*)(DspRig&, std::mt19937&);

// The note ended and another begun on the same object, or the first of a case
std::string dspNewNote(DspRig& r, std::mt19937& rng) {
    if (r.kind != nullptr) {
        if (!r.fw.call(kDestroy, { kNoteOnObjectAt, 0 })) return "the destructor faults: " + r.fw.cpu.error();
        vc2CosmDestroy(r.o, r.pools, 0);
    }
    return r.note(rng);
}

std::string dspRender(DspRig& r, std::mt19937& rng) {
    if (r.kind == nullptr) return dspNewNote(r, rng);
    ++r.n.renders;
    constexpr std::array<float, 6> kPeaks { 30000.0f, 1.0e7f, 1.0e8f, 6.0e8f, 2.1e9f, 1.0f };
    const float left = kPeaks[rng() % kPeaks.size()], right = kPeaks[rng() % kPeaks.size()];
    std::array<float, 2 * kFrames> lr;
    for (int i = 0; i < kFrames; ++i) {
        lr[(size_t)(2 * i)] = left * (2.0f * vc2Unit(rng) - 1.0f);
        lr[(size_t)(2 * i + 1)] = right * (2.0f * vc2Unit(rng) - 1.0f);
    }
    r.fw.cpu.write(kFramesAt, lr.data(), sizeof lr);
    if (!r.fw.call(r.fw.cpu.rd32(kRoutine), { kD, kNoteOnObjectAt + 0x10, kNoteOnObjectAt, kFramesAt }))
        return "the DSP faults: " + r.fw.cpu.error();
    r.dsp.render(lr.data());
    std::string d = vc2RegionsDifference(r.fw, { { kFramesAt, lr.data(), sizeof lr, "the frames" } });
    if (!d.empty()) return "code " + std::to_string(r.kind->code) + (r.stereo ? " stereo: " : " mono: ") + d;
    d = r.stateDifference();
    return d.empty() && r.kind->ring != nullptr && rng() % 8 == 0 ? r.ringDifference() : d;
}

std::string dspUpdate(DspRig& r, std::mt19937& rng) {
    if (r.kind == nullptr) return dspNewNote(r, rng);
    ++r.n.updates;
    drawDspLevels(r.fw, r.o, rng);
    const float pitch = 0.25f + 3.75f * vc2Unit(rng);
    std::memcpy(&r.fw.cpu.fb[0][4], &pitch, sizeof pitch);
    if (!r.fw.call(kUpdate, { kNoteOnObjectAt })) return "the update faults: " + r.fw.cpu.error();
    r.frame.update(r.o, pitch);
    r.dsp.update(r.o, r.bound);
    return r.stateDifference();
}

std::string dspNewBlock(DspRig& r, std::mt19937& rng) {
    if (r.kind == nullptr) return dspNewNote(r, rng);
    ++r.n.newBlocks;
    // One in four of another type's code, which rom.4's new block leaves alone
    vc2DrawBlockOf(r.block, rng() % 4 != 0 ? r.kind->code : kKinds[rng() % kKinds.size()].code, rng);
    r.fw.cpu.write(kNoteOnBlockAt, &r.block, sizeof r.block);
    if (!r.fw.call(kNewBlock, { kNoteOnObjectAt, kNoteOnBlockAt })) return "the new block faults: " + r.fw.cpu.error();
    r.frame.newBlock(r.o, r.block);
    r.dsp.newBlock(r.o, r.block, r.bound);
    return r.stateDifference();
}

// Each event's share of twelve
constexpr std::array<DspEvent, 12> kDspEvents { dspRender, dspRender, dspRender, dspRender, dspRender, dspRender,
                                                dspRender, dspRender, dspUpdate, dspUpdate, dspNewBlock, dspNewNote };

// one-off: each part's own event table, through vc2CosmEvent
std::string DspRig::event(std::mt19937& rng) { return vc2CosmEvent(*this, kDspEvents, rng); }

// one-off: each part's own report line, in fcheck's baseline in its own words
void DspRig::say(const Vc2Run& run) const {
    std::printf("dsp: %d cases, %ld events: %ld notes (%ld stereo), %ld renders, %ld updates, %ld new blocks, "
                "%ld state words, %ld rings\n", run.cases, n.events, n.notes, n.stereo, n.renders, n.updates,
                n.newBlocks, n.words, n.rings);
}
} // namespace

long vc2CosmDspCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<DspRig>(fw, run, 6, kEvents); }
#endif
