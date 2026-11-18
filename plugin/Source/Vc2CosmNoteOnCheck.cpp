// xt_vc2cosm's sixth part (v250): rom.4's COSM note-on frame against Vc2CosmNoteOn and Vc2CosmPools: the pools at their
// own address (0x89999290), set up by rom.4's static initializer (0x893ab550); an object, its block and the four bound
// envelopes and LFOs in the interpreter's RAM, which the port's Vc2CosmBound reads too; only operator delete hooked.
// Events: a note-on (0x893aa0e0) on the types ported (Through, the Speaker Simulator, the Growl; from v252 the
// Resonator and the TVF, adapted over XtCosm, counted apart) and codes the
// switches pass, the slot it takes holding noise first on both sides; a new block alone (0x893aa5d0); the destructor
// (0x893aa060); the pools set up again; the object, its block or the bound words drawn again.  After each: the
// object, the pools, the slot's four buffers when one was taken, and the deletes, word for word.  Its draws in
// Vc2CosmNoteOnDraws.cpp.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmAdapters.h"
#include "Vc2CosmCheck.h"
#include "Vc2CosmNoteOnDraws.h"
#include <algorithm>

namespace {
constexpr uint32_t kNoteOn = 0x893aa0e0, kNewBlock = 0x893aa5d0, kDestroy = 0x893aa060;   // [FW]
constexpr uint32_t kPoolsInit = 0x893ab550, kPoolsAt = 0x89999290, kDelete = 0x89506438;   // [FW]
constexpr int kEvents = 60;

struct NoteOnCounts {
    long events = 0, noteOns = 0, slots = 0, newBlocks = 0, destroys = 0, inits = 0, draws = 0;
    long adaptedNoteOns = 0, adaptedBlocks = 0;   // on a type adapted over XtCosm (v252)
};

// Whether a code's type is one of the adapters' (Vc2CosmAdapters)
bool adapted(int32_t code) {
    return std::ranges::any_of(vc2CosmAdapterRows(), [code](const Vc2CosmTypeRow& r) { return r.code == code; });
}

// The bound words as the port reads them: the interpreter's, where the check drew them
struct RamBound final : Vc2CosmBound {
    explicit RamBound(Sh4& c) : cpu(c) {}
    Sh4& cpu;
    float word(uint32_t at) override { return std::bit_cast<float>(cpu.rd32(at)); }
};

struct NoteOnRig {
    explicit NoteOnRig(Vc2Fw& f) : fw(f) {}
    Vc2Fw& fw;
    Vc2CosmObjectWords o {};
    Vc2CosmBlockWords block {};
    Vc2CosmPools pools;
    RamBound bound { fw.cpu };
    Vc2CosmNoteOn noteOn { pools, bound };
    std::vector<uint32_t> romDeletes, portDeletes;
    int32_t filled = -1;   // the slot the last note-on's noise went into, or −1
    uint32_t filledPool = 0;
    NoteOnCounts n;
    void hook();
    void start(std::mt19937& rng);
    std::string event(std::mt19937& rng);
    void say(const Vc2Run& run) const;
};

std::string noteOnDifference(NoteOnRig& r) {
    if (r.romDeletes != r.portDeletes) return "the deletes";
    std::string d = vc2RegionsDifference(r.fw, { { kNoteOnObjectAt, &r.o, sizeof r.o, "the COSM object" },
                                                 { kPoolsAt, &r.pools.w, sizeof r.pools.w, "the pools" } });
    if (!d.empty() || r.filled < 0) return d;
    const Vc2CosmSlot& s = r.pools.w.pools[r.filledPool][(size_t)r.filled];
    for (int k = 0; k < 4 && d.empty(); ++k) {
        const std::vector<float>& mine = r.pools.buffer(r.filledPool, r.filled, k);
        d = vc2RegionsDifference(r.fw, { { s.buffers[(size_t)k], mine.data(), mine.size() * 4, "a slot's buffer" } });
    }
    return d;
}

// rom.4's routine on the object and `args`, then the port's: the object, the pools, the buffers, the deletes
template <class Mine>
std::string noteOnBoth(NoteOnRig& r, uint32_t routine, std::initializer_list<uint32_t> args, const char* what,
                       Mine mine) {
    vc2WriteNoteOnState(r.fw.cpu, r.o, r.block);
    r.romDeletes.clear();
    r.portDeletes.clear();
    return vc2CosmBoth(r.fw, routine, args, { what, false }, [&] {
        mine();
        return 0u;
    }, [&r] { return noteOnDifference(r); });
}

using NoteOnEvent = std::string (*)(NoteOnRig&, std::mt19937&);

std::string noteOnEvent(NoteOnRig& r, std::mt19937& rng) {
    ++r.n.noteOns;
    vc2DrawNoteOnBlock(r.block, rng);
    const uint32_t pool = vc2DrawNoteOnPool(rng);
    r.filled = vc2FillFreeSlot(r.fw.cpu, r.pools, pool, r.block.code, rng);
    r.filledPool = pool;
    r.n.slots += r.filled >= 0;
    r.n.adaptedNoteOns += adapted(r.block.code);
    return noteOnBoth(r, kNoteOn, { kNoteOnObjectAt, kNoteOnBlockAt, pool }, "the note-on",
                      [&] { r.noteOn.noteOn(r.o, r.block, pool); });
}

std::string newBlockEvent(NoteOnRig& r, std::mt19937& rng) {
    ++r.n.newBlocks;
    vc2DrawNoteOnBlock(r.block, rng);
    if (rng() % 4 != 0) r.block.code = r.o.code;
    r.n.adaptedBlocks += r.block.code == r.o.code && adapted(r.block.code);
    r.filled = -1;
    return noteOnBoth(r, kNewBlock, { kNoteOnObjectAt, kNoteOnBlockAt }, "the new block",
                      [&] { r.noteOn.newBlock(r.o, r.block); });
}

std::string destroyEvent(NoteOnRig& r, std::mt19937& rng) {
    ++r.n.destroys;
    const uint32_t flags = rng() % 2;
    r.filled = -1;
    return noteOnBoth(r, kDestroy, { kNoteOnObjectAt, flags }, "the destructor", [&] {
        if (vc2CosmDestroy(r.o, r.pools, flags)) r.portDeletes.push_back(kNoteOnObjectAt);
    });
}

std::string poolsInitEvent(NoteOnRig& r, std::mt19937&) {
    ++r.n.inits;
    r.filled = -1;
    return noteOnBoth(r, kPoolsInit, {}, "the pools' set-up", [&] { r.pools.init(); });
}

std::string redrawEvent(NoteOnRig& r, std::mt19937& rng) {
    ++r.n.draws;
    vc2DrawNoteOnObject(r.o, rng);
    vc2DrawBound(r.fw.cpu, rng);
    return "";
}

// Each event's share of eight
constexpr std::array<NoteOnEvent, 8> kNoteOnEvents { noteOnEvent, noteOnEvent, noteOnEvent, newBlockEvent,
                                                     newBlockEvent, destroyEvent, poolsInitEvent, redrawEvent };

void NoteOnRig::hook() {
    fw.cpu.hook(kDelete, [this](Sh4& c) {
        romDeletes.push_back(c.r[4]);
        c.ret(0);
    });
}

void NoteOnRig::start(std::mt19937& rng) {
    vc2DrawNoteOnObject(o, rng);
    vc2DrawBound(fw.cpu, rng);
    fw.call(kPoolsInit, {});
    pools.init();
    filled = -1;
}

// one-off: each part's own event table, through vc2CosmEvent
std::string NoteOnRig::event(std::mt19937& rng) { return vc2CosmEvent(*this, kNoteOnEvents, rng); }

// one-off: each part's own report line, in fcheck's baseline in its own words; one printer would rewrite the earlier
// parts' lines
void NoteOnRig::say(const Vc2Run& run) const {
    std::printf("note-on: %d cases, %ld events: %ld note-ons (%ld taking a slot, %ld adapted), %ld new blocks (%ld "
                "adapted), %ld destroys, %ld pool set-ups, %ld draws\n", run.cases, n.events, n.noteOns, n.slots,
                n.adaptedNoteOns, n.newBlocks, n.adaptedBlocks, n.destroys, n.inits, n.draws);
}
} // namespace

long vc2CosmNoteOnCases(Vc2Fw& fw, const Vc2Run& run) { return vc2CosmPart<NoteOnRig>(fw, run, 5, kEvents); }
#endif
