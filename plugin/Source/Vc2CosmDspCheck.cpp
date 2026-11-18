// xt_vc2cosm's seventh part (v263, v264): rom.4's own COSM DSP routines against Vc2CosmDsp.  An object in the
// interpreter's RAM and the port's take the same note-on (0x893aa0e0), install (0x893aafe0), new blocks (0x893aa5d0) and
// updates (0x893aa7d0); then rom.4's routine (the install's, +0x148) and the port's render the same frames.  After each
// render: the frames, the state through XtCosm's getters against rom.4's D words, and now and then the rings against the
// slot's buffers.  A new note on the same object follows a destructor, so what one type leaves in the words a note-on
// doesn't clear reaches the next.  The slot a note takes is drawn, its neighbours' ends noise or what the last note left
// there: a long note fills its rings to their ends, and the first Side Band Filter after it, in the next slot, reads
// them.  Now and then every slot is taken: rom.4's allocator then clears and plays on four stale words of its stack,
// drawn here as buffers of their own (Vc2CosmDspSlots.cpp).  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmCheck.h"
#include "Vc2CosmDspCheck.h"
#include "Vc2CosmNoteOnDraws.h"
#include <optional>

namespace {
constexpr uint32_t kNoteOn = 0x893aa0e0, kNewBlock = 0x893aa5d0, kUpdate = 0x893aa7d0, kInstall = 0x893aafe0;   // [FW]
constexpr uint32_t kDestroy = 0x893aa060, kPoolsInit = 0x893ab550;   // [FW]
constexpr uint32_t kFramesAt = 0x8C076000, kD = kNoteOnObjectAt + 0x58, kRoutine = kNoteOnObjectAt + 0x148;
constexpr uint32_t kBuffersAt = kNoteOnObjectAt + 0x60;
constexpr uint32_t kEnvelopeRate = 0x54;   // [FW] a bound envelope's rate
constexpr int kEvents = 150, kFrames = 32;
constexpr int kLongBlocks = 104;           // a ring's 3,267 samples, written to its end
constexpr int32_t kFirstBand = 7;          // [FW] the first Side Band Filter's block code
constexpr size_t kFirstBandSub = 15;       // [FW] its Sub Delay Line Detune's word: below 0, line 2 is the longer

struct DspCounts {
    long events = 0, notes = 0, stereo = 0, renders = 0, updates = 0, newBlocks = 0, words = 0, rings = 0;
    long slotZero = 0, staleEnds = 0, exhausted = 0, longNotes = 0;
};

struct DspRig {
    explicit DspRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2CosmObjectWords o {};
    Vc2CosmBlockWords block {};
    Vc2CosmPools pools;
    Vc2CosmRamBound bound { fw.cpu };
    Vc2CosmNoteOn frame { pools, bound };
    std::optional<Vc2CosmDsp> dsp;          // one a case: the words a first note inherits are the object's noise
    const Vc2CosmDspKind* kind = nullptr;   // the note's, none before a case's first
    bool stereo = false;
    bool followEnds = false;                // a long note has just filled its rings: the next note reads their ends
    int32_t lastSlot = -1;
    uint32_t lastPool = 0;
    DspCounts n;
    void hook() {}
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
    std::string note(std::mt19937& rng);
    std::string stateDifference();
    std::string ringDifference();
    int drawTaken(uint32_t& pool, bool following, std::mt19937& rng);
};

std::string DspRig::stateDifference() {
    Vc2CosmDspWords words;
    kind->state(*dsp, o, stereo, words);
    for (const Vc2CosmDspWord& w : words) {
        ++n.words;
        const uint32_t rom = fw.cpu.rd32(kD + w.at), mine = std::bit_cast<uint32_t>(w.value);
        if (rom == mine) continue;
        char said[80];
        std::snprintf(said, sizeof said, "code %d's D+0x%x: %08x, against %08x", kind->code, w.at, rom, mine);
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

// The rings' write index and samples, in the buffers the slot (or the stale words) gave rom.4
std::string DspRig::ringDifference() {
    ++n.rings;
    const Vc2CosmDspRing ring = kind->ring(*dsp, o);
    if ((int32_t)fw.cpu.rd32(kD + 4) != ring.index) return "the ring's write index";
    const size_t bytes = (size_t)ring.length * sizeof(float);
    for (uint32_t k = 0; k < ring.lines.size(); ++k) {
        if (ring.lines[k] == nullptr) continue;
        const uint32_t at = fw.cpu.rd32(kBuffersAt + 4 * k);
        const std::string d = vc2RegionsDifference(fw, { { at, ring.lines[k], bytes, "a ring" } });
        if (!d.empty()) return d;
    }
    return "";
}

// How many of a pool's first slots are in use, so the note takes the one after them: the slot after a long note's,
// whose line 2 reads what that note left; for a type with rings none free, the last slot (in front of slot 0), slot 0,
// or any
int DspRig::drawTaken(uint32_t& pool, bool following, std::mt19937& rng) {
    if (following) {
        pool = lastPool;
        return (lastSlot + 1) % kDspSlots;
    }
    pool = rng() % 2;
    const int any = (int)(rng() % kDspSlots);
    const std::array<int, 8> slots { kDspSlots, kDspSlots - 1, kDspSlots - 1, kDspSlots - 1, 0, any, any, any };
    return kind->ring != nullptr ? slots[rng() % slots.size()] : any;
}

// A note on the object: a block of a type rendered, the slot it takes drawn, the note-on and install on both sides
std::string DspRig::note(std::mt19937& rng) {
    ++n.notes;
    const int32_t before = kind != nullptr ? kind->code : -1;
    const bool following = followEnds;
    followEnds = false;
    kind = following ? &vc2DspKindOf(kFirstBand) : &vc2DspDrawKind(kind, rng);
    stereo = rng() % (following ? 4 : 2) != 0;
    n.stereo += stereo;
    vc2DrawBlockOf(block, kind->code, rng);
    const bool firstBand = kind->code == kFirstBand && (following || rng() % 4 != 0);
    if (firstBand) block.words[kFirstBandSub] = std::bit_cast<uint32_t>(-20.0f - 80.0f * vc2Unit(rng));
    uint32_t pool = 0;
    const int taken = drawTaken(pool, following, rng);
    vc2DspHoldSlots(fw, pools, pool, taken);
    const bool exhausted = kind->ring != nullptr && taken == kDspSlots;
    if (exhausted) {
        ++n.exhausted;
        vc2DspDrawStaleStack(fw);
    } else if (kind->ring != nullptr) {
        n.slotZero += taken == 0;
        vc2FillFreeSlot(fw.cpu, pools, pool, kind->code, rng);
        if (!following && rng() % 2 != 0) {
            ++n.staleEnds;
            vc2DspDrawEnds(fw, pools, pool, taken, rng);
        }
    }
    // A pitch, and for the first Side Band Filter one that puts its delay near the line's end, where line 2 reads in
    // front of its buffer
    o.setUp.pitch = firstBand ? 44100.0f / (o.setUp.other * (3140.0f + 120.0f * vc2Unit(rng)))
                              : 0.25f + 3.75f * vc2Unit(rng);
    fw.cpu.wr32(kNoteOnObjectAt, std::bit_cast<uint32_t>(o.setUp.pitch));
    drawDspLevels(fw, o, rng);
    fw.cpu.write(kNoteOnBlockAt, &block, sizeof block);
    if (!fw.call(kNoteOn, { kNoteOnObjectAt, kNoteOnBlockAt, pool })) return "the note-on faults: " + fw.cpu.error();
    frame.noteOn(o, block, pool);
    if (!fw.call(kInstall, { kNoteOnObjectAt, stereo ? 1u : 0u })) return "the install faults: " + fw.cpu.error();
    vc2CosmInstall(o, stereo ? 1 : 0);
    dsp->noteOn(o, block, bound, stereo);
    lastSlot = o.slot;
    lastPool = pool;
    std::string d = exhausted ? vc2DspStaleDifference(fw, o) : stateDifference();
    if (d.empty() && kind->ring != nullptr) d = ringDifference();   // a note-on leaves its rings clear
    return d.empty() ? d : d + ", a note after code " + std::to_string(before);
}

void DspRig::start(std::mt19937& rng) {
    vc2DrawNoteOnObject(o, rng);
    // The words a first note inherits (D+0x8c, +0x90, +0x98) as a note would leave them: never a NaN, whose sign
    // through a sum is the host's
    for (size_t k : { 0u, 1u, 3u }) o.d0e4[k] = std::bit_cast<uint32_t>(2.0f * vc2Unit(rng) - 1.0f);
    fw.cpu.write(kNoteOnObjectAt, &o, sizeof o);
    fw.call(kPoolsInit, {});
    pools.init();
    dsp.emplace(pools);
    kind = nullptr;
    followEnds = false;
    lastSlot = -1;
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

// A block of frames through rom.4's routine and the port's
std::string renderBlock(DspRig& r, std::mt19937& rng) {
    ++r.n.renders;
    constexpr std::array<float, 6> kPeaks { 30000.0f, 1.0e7f, 1.0e8f, 6.0e8f, 2.1e9f, 1.0f };
    const float left = kPeaks[rng() % kPeaks.size()], right = kPeaks[rng() % kPeaks.size()];
    std::array<float, 2 * kFrames> lr;
    for (int i = 0; i < kFrames; ++i) {
        lr[(size_t)(2 * i)] = left * (2.0f * vc2Unit(rng) - 1.0f);
        lr[(size_t)(2 * i + 1)] = right * (2.0f * vc2Unit(rng) - 1.0f);
    }
    std::array<float, 3> past;   // what lies after the frames, which a stereo Side Band Filter's follower reads
    for (float& p : past) p = left * (2.0f * vc2Unit(rng) - 1.0f);
    r.fw.cpu.write(kFramesAt, lr.data(), sizeof lr);
    r.fw.cpu.write(kFramesAt + sizeof lr, past.data(), sizeof past);
    if (!r.fw.call(r.fw.cpu.rd32(kRoutine), { kD, kNoteOnObjectAt + 0x10, kNoteOnObjectAt, kFramesAt }))
        return "the DSP faults: " + r.fw.cpu.error();
    r.dsp->render(r.o, lr.data(), past.data());
    std::string d = vc2RegionsDifference(r.fw, { { kFramesAt, lr.data(), sizeof lr, "the frames" } });
    if (!d.empty()) {
        const std::string state = r.stateDifference();
        return "code " + std::to_string(r.kind->code) + (r.stereo ? " stereo" : " mono") + " in slot " +
               std::to_string(r.o.slot) + ": " + d + (state.empty() ? "" : "; and " + state);
    }
    d = r.stateDifference();
    return d.empty() && r.kind->ring != nullptr && rng() % 8 == 0 ? r.ringDifference() : d;
}

std::string dspRender(DspRig& r, std::mt19937& rng) {
    return r.kind == nullptr ? dspNewNote(r, rng) : renderBlock(r, rng);
}

// A note held until its rings are written to their ends, then the next note, which reads them from the next slot
std::string dspLongNote(DspRig& r, std::mt19937& rng) {
    if (r.kind == nullptr || r.kind->ring == nullptr || r.o.slot < 0) return dspRender(r, rng);
    ++r.n.longNotes;
    for (int b = 0; b < kLongBlocks; ++b) {
        const std::string d = renderBlock(r, rng);
        if (!d.empty()) return d;
    }
    const std::string d = r.ringDifference();
    if (!d.empty()) return d;
    r.followEnds = true;
    return dspNewNote(r, rng);
}

std::string dspUpdate(DspRig& r, std::mt19937& rng) {
    if (r.kind == nullptr) return dspNewNote(r, rng);
    ++r.n.updates;
    drawDspLevels(r.fw, r.o, rng);
    const float pitch = 0.25f + 3.75f * vc2Unit(rng);
    std::memcpy(&r.fw.cpu.fb[0][4], &pitch, sizeof pitch);
    if (!r.fw.call(kUpdate, { kNoteOnObjectAt })) return "the update faults: " + r.fw.cpu.error();
    r.frame.update(r.o, pitch);
    r.dsp->update(r.o, r.bound);
    return r.stateDifference();
}

std::string dspNewBlock(DspRig& r, std::mt19937& rng) {
    if (r.kind == nullptr) return dspNewNote(r, rng);
    ++r.n.newBlocks;
    // One in four of another type's code, which rom.4's new block leaves alone
    const std::span<const Vc2CosmDspKind> kinds = vc2CosmDspKinds();
    vc2DrawBlockOf(r.block, rng() % 4 != 0 ? r.kind->code : kinds[rng() % kinds.size()].code, rng);
    r.fw.cpu.write(kNoteOnBlockAt, &r.block, sizeof r.block);
    if (!r.fw.call(kNewBlock, { kNoteOnObjectAt, kNoteOnBlockAt })) return "the new block faults: " + r.fw.cpu.error();
    r.frame.newBlock(r.o, r.block);
    r.dsp->newBlock(r.o, r.block, r.bound);
    return r.stateDifference();
}

// Each event's share of twelve
constexpr std::array<DspEvent, 12> kDspEvents { dspRender, dspRender, dspRender, dspRender, dspRender, dspRender,
                                                dspRender, dspLongNote, dspUpdate, dspUpdate, dspNewBlock, dspNewNote };

// one-off: each part's own event table, through vc2CosmEvent
std::string DspRig::event(std::mt19937& rng) { return vc2CosmEvent(*this, kDspEvents, rng); }

// one-off: each part's own report line, in fcheck's baseline in its own words
void DspRig::say(const Vc2Run& run) const {
    std::printf("dsp: %d cases, %ld events: %ld notes (%ld stereo, %ld long, %ld on slot 0, %ld on stale ends, "
                "%ld with no slot), %ld renders, %ld updates, %ld new blocks, %ld state words, %ld rings\n", run.cases,
                n.events, n.notes, n.stereo, n.longNotes, n.slotZero, n.staleEnds, n.exhausted, n.renders, n.updates,
                n.newBlocks, n.words, n.rings);
}
} // namespace

long vc2CosmDspCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<DspRig>(fw, run, 6, kEvents); }
#endif
