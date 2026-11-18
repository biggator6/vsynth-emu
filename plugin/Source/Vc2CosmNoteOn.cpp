// rom.4's COSM note-on frame: see Vc2CosmNoteOn.h (v250).  Every address is rom.4's, every offset the object's.
#include "Vc2CosmNoteOn.h"
#include "Vc2CosmAdapters.h"
#include "Vc2CosmBufferedAdapters.h"
#include "XtFloat.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr int32_t kPosition = 3266;        // [FW] the lines' place from the note-on (13 × 256 − 62)
constexpr uint32_t kLastCode = 20;         // [FW] the new block's and the note-on's switches
constexpr uint32_t kLastStart = 19;        // [FW] the start values' switch, on the code's byte − 1
// [FW] the allocator's switch on the code's byte − 7: 0…3 (codes 7…10) and 8…11 (15…18) take a slot, 4…7 none
constexpr int32_t kFirstBuffered = 7;
constexpr uint32_t kBufferedLast = 11, kUnbufferedFrom = 4, kUnbufferedTo = 7;
constexpr uint32_t kLevel = 0x4c, kRate = 0x54, kLfoLevel = 0x38;   // [FW] the bound objects' words
constexpr float kRateScale = 4.0f;         // [FW] an envelope's rate held × 4 (0x893aa284)

// The Growl's parts on the object's words: the object into its view, the part, the view back
template <class Part>
void onGrowl(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, Part part) {
    std::memcpy(&n.growl.w, &o, sizeof o);
    part(n.growl);
    std::memcpy(&o, &n.growl.w, sizeof o);
}

// [FW] the Speaker Simulator's new block, inline: the block's byte +4 into the object's +0x14
void speakerNewBlock(Vc2CosmNoteOn&, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    o.variant = static_cast<uint8_t>(b.words[0]);
}

void growlNewBlock(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    Vc2CosmGrowlBlock gb {};
    std::memcpy(&gb, &b, sizeof gb);
    onGrowl(n, o, [&gb](Vc2CosmGrowl& g) { g.newBlock(gb); });
}

void growlNoteOn(Vc2CosmNoteOn& n, Vc2CosmObjectWords& o, const Vc2CosmBlockWords&) {
    onGrowl(n, o, [](Vc2CosmGrowl& g) { g.noteOn(); });
}

// The Growl's start values, 0x893b9d20: an rts
void growlStartValues(Vc2CosmNoteOn&, Vc2CosmObjectWords&, const Vc2CosmBlockWords&) {}

// Each ported type's parts: Through none; the Speaker Simulator its new block; the Growl its start values (an rts),
// new block (0x893b9dc0) and case (0x893b9d30)
constexpr Vc2CosmTypeRow kRows[] { { 0, nullptr, nullptr, nullptr }, { 5, nullptr, speakerNewBlock, nullptr },
                                   { Vc2CosmGrowl::kCode, growlStartValues, growlNewBlock, growlNoteOn } };
} // namespace

Vc2CosmNoteOn::Vc2CosmNoteOn(Vc2CosmPools& pools, Vc2CosmBound& bound)
    : pools_(pools), bound_(bound), scratch_(std::make_unique<Vc2CosmBufferedScratch>()) {}
Vc2CosmNoteOn::~Vc2CosmNoteOn() = default;

const Vc2CosmTypeRow* Vc2CosmNoteOn::rowOf(int32_t code) {
    for (std::span<const Vc2CosmTypeRow> rows : { std::span<const Vc2CosmTypeRow>(kRows), vc2CosmAdapterRows() })
        for (const Vc2CosmTypeRow& r : rows)
            if (r.code == code) return &r;
    return nullptr;
}

void Vc2CosmNoteOn::allocate(Vc2CosmObjectWords& o) {
    o.slot = -1;
    if (o.pool >= Vc2CosmPoolWords::kPools) return;
    const auto k = static_cast<uint32_t>(static_cast<int8_t>(o.dspCode) - kFirstBuffered);
    if (k > kBufferedLast || (k >= kUnbufferedFrom && k <= kUnbufferedTo)) {
        o.buffers[0] = 0;
        return;
    }
    std::array<uint32_t, 4> buffers = o.buffers;   // with no slot free rom.4 keeps its stack's words; never reached
    o.slot = pools_.take(o.pool, buffers);
    o.buffers = buffers;
    if (o.slot < 0) return;
    for (int b = 0; b < static_cast<int>(buffers.size()); ++b)
        std::ranges::fill(pools_.buffer(o.pool, o.slot, b), 0.0f);
}

void Vc2CosmNoteOn::hold(Vc2CosmObjectWords& o) {
    o.rates[0] = xtf::mul(bound_.word(o.bindings[0] + kRate), kRateScale);
    o.rates[1] = xtf::mul(bound_.word(o.bindings[1] + kRate), kRateScale);
    o.held[0] = bound_.word(o.bindings[0] + kLevel);
    o.held[1] = bound_.word(o.bindings[1] + kLevel);
    o.held[2] = bound_.word(o.bindings[2] + kLfoLevel);
    o.held[3] = bound_.word(o.bindings[3] + kLfoLevel);
}

void Vc2CosmNoteOn::noteOn(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b, uint32_t pool) {
    o.pool = pool;
    o.code = b.code;
    std::ranges::fill(o.cleared, 0.0f);
    std::ranges::fill(o.cleared0f4, 0.0f);
    o.position = kPosition;
    o.dspCode = static_cast<uint8_t>(b.code);
    allocate(o);
    const auto start = static_cast<int8_t>(o.dspCode);
    const Vc2CosmTypeRow* starts = static_cast<uint32_t>(start - 1) <= kLastStart ? rowOf(start) : nullptr;
    if (starts != nullptr && starts->startValues != nullptr) starts->startValues(*this, o, b);
    hold(o);
    newBlock(o, b);
    if (static_cast<uint32_t>(o.code) > kLastCode) return;
    const Vc2CosmTypeRow* row = rowOf(o.code);
    if (row != nullptr && row->noteOnCase != nullptr) row->noteOnCase(*this, o, b);
}

void Vc2CosmNoteOn::newBlock(Vc2CosmObjectWords& o, const Vc2CosmBlockWords& b) {
    if (o.code != b.code || static_cast<uint32_t>(o.code) > kLastCode) return;
    const Vc2CosmTypeRow* row = rowOf(o.code);
    if (row != nullptr && row->newBlock != nullptr) row->newBlock(*this, o, b);
}
