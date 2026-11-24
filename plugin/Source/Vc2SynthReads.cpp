// C's reads of the part and its children's routines: see Vc2SynthReads.h (v343).
#include "Vc2SynthReads.h"
#include <algorithm>
#include <cstring>
#include <type_traits>

namespace {
// The kinds of C's children by their rows' places in rom.4's table (Vc2Sets::children(), kChildren's order)
enum ChildKind : std::ptrdiff_t { kCommon, kOscBlock, kMix, kEnvelope, kLfo, kCosmInput };
constexpr uint32_t kLinksAt = 0x168, kPartByteAt = 0x24;   // [FW] M's channel and part, a trip record's byte

template <class Pool>
constexpr bool isCommons = std::is_same_v<std::remove_cvref_t<Pool>, Vc2SetCommons>;

// Whether `object` is one of the pool's: a child of none (0), which C passes on for every kind but the common, is
// never one after a make (every placement gives an object), and reaches nothing here
template <class Pool>
bool holds(const Pool&, uint32_t object) {
    return object - Pool::kAt < (uint32_t)Pool::kCount * Pool::kBytes;
}
} // namespace

template <class F>
void Vc2SynthReads::onPool(const Vc2SetChild& c, F f) {
    switch (&c - Vc2Sets::children().data()) {
    case kCommon: f(s_.commons); break;
    case kOscBlock: f(s_.oscBlocks); break;
    case kMix: f(s_.mixes); break;
    case kEnvelope: f(s_.envelopes); break;
    case kLfo: f(s_.lfos); break;
    case kCosmInput: f(s_.cosmInputs); break;
    default: f(s_.tvas); break;
    }
}

uint32_t* Vc2SynthReads::block(uint32_t noteAt) { return s_.notes.wordsAt(noteAt); }

uint32_t Vc2SynthReads::allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) {
    uint32_t object = 0;
    onPool(c, [&](auto& p) {
        if constexpr (isCommons<decltype(p)>) {
            object = p.addressOf(number);
        } else {
            object = p.placeOf(number, index);
        }
    });
    return object;
}

void Vc2SynthReads::construct(const Vc2SetChild& c, const Vc2SetChildCall& k) {
    uint32_t* b = block(k.noteAt);
    onPool(c, [&](auto& p) {
        if (!holds(p, k.object)) return;
        if constexpr (isCommons<decltype(p)>) {
            p.construct(k.object, k.call, b, *this);
        } else {
            p.construct(k, b, *this);
        }
    });
}

void Vc2SynthReads::convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) {
    uint32_t* b = block(noteAt);
    onPool(c, [&](auto& p) {
        if (holds(p, object)) p.convert(object, call, b, *this);
    });
}

void Vc2SynthReads::update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) {
    onPool(c, [&](auto& p) {
        if (holds(p, object)) p.update(object, call, *this);
    });
}

void Vc2SynthReads::copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) {
    uint32_t* b = block(noteAt);
    onPool(c, [&](auto& p) {
        if (!holds(p, object)) return;
        if constexpr (isCommons<decltype(p)>) {
            p.copy(object, b);
        } else {
            p.copy(object, b, *this);
        }
    });
}

// The envelopes' alone (C's release passes only them)
void Vc2SynthReads::release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) {
    uint32_t* b = block(noteAt);
    onPool(c, [&](auto& p) {
        if constexpr (requires { p.release(object, velocity, b, *this); }) {
            if (holds(p, object)) p.release(object, velocity, b, *this);
        }
    });
}

// The OSC blocks' alone (C's refresh passes only them)
uint32_t Vc2SynthReads::refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) {
    uint32_t word = 0;
    onPool(c, [&](auto& p) {
        if constexpr (requires { p.refresh(object, call, *this); }) {
            if (holds(p, object)) word = p.refresh(object, call, *this);
        }
    });
    return word;
}

int Vc2SynthReads::velocityCurve(int velocity, int sens, int curve) { return s_.math.velocity(velocity, sens, curve); }

const Vc2ToneShared& Vc2SynthReads::shared() { return s_.math; }

// M by rom.4's address: a note's in the pool, or the part's own (the static M)
Vc2MatrixWords* Vc2SynthReads::matrixAt(uint32_t matrix) {
    using Ms = Vc2NoteMatrices;
    const uint32_t from = matrix - Ms::kAt;
    if (from < (uint32_t)Ms::kCount * Ms::kBytes && from % Ms::kBytes == 0) {
        return &s_.sets.matrices.items[from / Ms::kBytes].m;
    }
    return matrix == Vc2StaticMatrix::kAt ? &s_.staticM.m : nullptr;
}

// M's sources' state as its links stand: the channel state M's +0x168 names, G's assignments and the part's byte (the
// part table's record by M's +0x16c, its +0x24)
Vc2MatrixSourceState Vc2SynthReads::stateOf(uint32_t matrix, Vc2MatrixWords* m) {
    const auto links = value<Vc2MatrixLinks>(matrix + kLinksAt);
    read(links.channel, &channel_, sizeof channel_);
    Vc2MatrixSourceState state { m, &channel_, {}, 0 };
    std::memcpy(state.assigns.data(), s_.g.assigns, sizeof s_.g.assigns);
    const uint32_t record = rd32(kVc2OscPartTableAt + 4u * (uint32_t)(int32_t)links.part);
    state.partByte = rd8(record + kPartByteAt);
    return state;
}

// The reader's group on M, its sources on M's state
const int16_t* Vc2SynthReads::matrixGroup(uint32_t reader, uint32_t matrix) {
    const auto found = std::ranges::find(kVc2MatrixReaders, reader);
    Vc2MatrixWords* m = matrixAt(matrix);
    if (found == kVc2MatrixReaders.end() || m == nullptr) return none_.data();
    Vc2MatrixSourceState state = stateOf(matrix, m);
    Vc2MatrixSourceRoutines sources(state);
    Vc2MatrixCore core(sources);
    return vc2MatrixRead(*m, vc2MatrixGroups()[(size_t)(found - kVc2MatrixReaders.begin())], core);
}

// v349: the Matrix core's refresh on M at its address, its state read as its links stand then
void Vc2SynthReads::refreshMatrix(uint32_t matrix) {
    Vc2MatrixWords* m = matrixAt(matrix);
    if (m == nullptr) return;
    Vc2MatrixSourceState state = stateOf(matrix, m);
    Vc2MatrixSourceRoutines sources(state);
    Vc2MatrixCore core(sources);
    core.refresh(*m);
}

// The sum `offset` bytes into the group the reader returns
int16_t Vc2SynthReads::matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) {
    const int16_t* sums = matrixGroup(reader, matrix);
    if (sums == none_.data()) return 0;
    int16_t sum = 0;
    std::memcpy(&sum, reinterpret_cast<const uint8_t*>(sums) + offset, sizeof sum);
    return sum;
}

Vc2MatrixGroups Vc2SynthReads::oscGroups(uint32_t matrix, uint32_t osc) {
    const size_t second = osc == 0 ? 0 : 1;
    Vc2MatrixGroups g;
    std::memcpy(g.osc, matrixGroup(kVc2OscGetters[second], matrix), sizeof g.osc);
    std::memcpy(g.lfo, matrixGroup(kVc2LfoGetters[second], matrix), sizeof g.lfo);
    return g;
}

uint16_t Vc2SynthReads::nextOscStep(uint32_t part, uint32_t kind, uint32_t osc) {
    const uint32_t at = kVc2OscStepsAt + kVc2OscStepsPart * part + 4 * kind + 2 * osc;
    const uint16_t step = rd16(at);
    const auto next = static_cast<uint16_t>(step + 1);
    memory_.write(at, &next, sizeof next);
    return step;
}

float Vc2SynthReads::pitchSource() { return s_.autoNote.ratioFor(s_.tracker, s_.pitch); }

const Vc2WaveStore& Vc2SynthReads::waveStore() { return *s_.store; }

const Vc2WaveMemory& Vc2SynthReads::waveMemory() {
    if (s_.waves == nullptr) return noWaves_;
    return *s_.waves;
}

Vc2WaveSets& Vc2SynthReads::waveSets() { return s_.waveSets; }
