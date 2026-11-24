// The VC-2 part's conversion sets' OSC blocks: see Vc2SetOscBlock.h (v391).  The routines' reads and writes in rom.4's
// order; the integer sums as the SH-4's 32-bit registers hold them.
#include "Vc2SetOscBlock.h"
#include "Vc2SetChildHead.h"
#include "Vc2SetOscKinds.h"
#include <cstring>

namespace {
using Header = std::array<uint8_t, 32>;
using Systems = std::array<uint8_t, 60>;
constexpr uint32_t kSeedTimes = 18813;   // [FW] Random Pitch's seed's step: seed × it + 1
constexpr int32_t kKeyTop = 127;         // [FW] the start key's hold
constexpr uint8_t kPedalDown = 64;       // [FW] the Hold pedal's value from which it holds
constexpr uint8_t kLegatoOn = 1;         // [FW] the part's portamento byte that keeps a key not held its own
constexpr uint32_t kOctave = 12;         // [FW] the scale tunes, a halfword a key of the octave
// [FW] the part's header: its key's offset, coarse and fine tunes, mono, portamento and the scale tunes
constexpr uint32_t kKeyShiftAt = 0, kCoarseAt = 1, kFineAt = 2, kMonoAt = 4, kLegatoAt = 5, kScaleAt = 6;
constexpr uint32_t kSystemsFineAt = 0, kSystemsCoarseAt = 2;   // [FW] the systems' master tune and coarse
// [FW] the record's coarse, fine and Random Pitch Depth; the refresh's type, waveform and PCM wave's words
constexpr uint32_t kRecordCoarseAt = 0x0e, kRecordFineAt = 0x10, kRandomAt = 0x12, kTypeAt = 1;
constexpr uint32_t kWaveformAt = 0x20, kWaveNumberAt = 0x3c, kWaveByteAt = 0x5c, kWaveByteTwoAt = 0x59;
constexpr uint8_t kAnalogType = 0, kPcmType = 1;   // [FW] the refresh's switch

int8_t s8(const uint8_t* b, uint32_t at) { return static_cast<int8_t>(b[at]); }

int16_t s16(const uint8_t* b, uint32_t at) {
    int16_t v;
    std::memcpy(&v, b + at, sizeof v);
    return v;
}

// Random Pitch by its depth: none with no depth, the seed left; else the seed steps on, and the offset is the depth
// times the seed as a halfword, doubled, its bits 16…31 as a halfword
int32_t randomOf(int16_t depth, uint16_t& seed) {
    if (depth == 0) return 0;
    const uint32_t next = static_cast<uint32_t>(seed) * kSeedTimes + 1u;
    seed = static_cast<uint16_t>(next);
    const auto product = static_cast<uint32_t>(depth * static_cast<int32_t>(static_cast<int16_t>(next)));
    return static_cast<int16_t>((product << 1) >> 16);
}

// The record from the part's table, by the call's part and the input's kind and OSC
void recordOf(Vc2SetOscInput& w, uint32_t part, Vc2SetReads& reads) { w.record = reads.oscRecord(part, w.kind, w.osc); }

// +0x88, the coarse tune: the systems', the part's and the record's, signed bytes
void coarseOf(Vc2SetOscInput& w, const Header& h, const Systems& sys) {
    w.coarse = static_cast<int16_t>(s8(sys.data(), kSystemsCoarseAt) + s8(h.data(), kCoarseAt) +
                                    s8(w.record.data(), kRecordCoarseAt));
}

// +0x90, the fine tune: the key's scale tune, the systems' and the part's fine, the record's, M's bend and `random`
void fineOf(Vc2SetOscInput& w, const Header& h, const Systems& sys, int32_t random, Vc2SetReads& reads) {
    uint32_t sum = static_cast<uint32_t>(s16(h.data(), kScaleAt + 2 * (w.key % kOctave)));
    sum += static_cast<uint32_t>(s16(sys.data(), kSystemsFineAt));
    sum += static_cast<uint32_t>(s16(h.data(), kFineAt));
    sum += static_cast<uint32_t>(s16(w.record.data(), kRecordFineAt));
    sum += static_cast<uint32_t>(reads.matrixBend(w.matrix));
    sum += static_cast<uint32_t>(random);
    w.fine = static_cast<int32_t>(sum);
}

// The tunes with a new Random Pitch (the constructor's, the conversion's)
void tunesDrawn(Vc2SetOscInput& w, const Header& h, uint16_t& seed, Vc2SetReads& reads) {
    const Systems sys = reads.oscSystems();
    coarseOf(w, h, sys);
    w.random = randomOf(s16(w.record.data(), kRandomAt), seed);
    fineOf(w, h, sys, w.random, reads);
}

// A start key held to 0…127
uint8_t startKeyHeld(int32_t k) { return static_cast<uint8_t>(k < 0 ? 0 : k > kKeyTop ? kKeyTop : k); }

// The constructor's start key: in POLY the glide key's last POLY key, or the key itself with a legato portamento when
// none was held then and the Hold pedal is up; in MONO the same by its MONO key, the pedal unread; held 0…127
uint8_t madeStartKey(const Vc2SetOscInput& w, const Header& h, const Vc2SetVoice& v, Vc2SetReads& reads) {
    const Vc2SetGlide g = reads.glideAt(v.source);
    const bool legato = h[kLegatoAt] == kLegatoOn;
    const int32_t shift = s8(h.data(), kKeyShiftAt);
    if (h[kMonoAt] == 0) {
        const bool own = legato && g.prevPolyHeld == 0 && reads.holdOf(g.table) < kPedalDown;
        return startKeyHeld(own ? w.key : g.prevPoly + shift);
    }
    return startKeyHeld(legato && g.prevMonoHeld == 0 ? w.key : g.prevMono + shift);
}

// The conversion's: the glide key's last MONO key, held 0…127
uint8_t startKey(const Header& h, const Vc2SetVoice& v, Vc2SetReads& reads) {
    return startKeyHeld(reads.glideAt(v.source).prevMono + s8(h.data(), kKeyShiftAt));
}

void voiceBytes(Vc2SetOscInput& w, const Vc2SetVoice& v) {
    w.key = v.key;
    w.velocity = v.byte43;
}
} // namespace

void Vc2SetOscBlocks::construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads) {
    Vc2SetOscInput& w = at(c.object);
    w.matrix = c.matrix;
    w.kind = static_cast<uint8_t>(c.kind);
    w.osc = static_cast<uint8_t>(c.index);
    const Vc2SetVoice v = reads.voiceAt(c.call.channelAt);
    voiceBytes(w, v);
    const std::array<uint8_t, 2> legato = reads.legatoOf(c.call.word);
    w.legatoKey = legato[1];
    w.legato = legato[0];
    const uint32_t part = reads.firstByte(v.part);
    w.step = reads.nextOscStep(part, w.kind, w.osc);
    w.part = static_cast<uint8_t>(part);
    recordOf(w, part, reads);
    const Header h = reads.oscHeader(part);
    tunesDrawn(w, h, seed.value, reads);
    w.startKey = madeStartKey(w, h, v, reads);
    vc2SetOscStart(w, block, reads);
    vc2SetOscConvert(w, block, reads);
}

void Vc2SetOscBlocks::convert(uint32_t object, const Vc2SetCall& call, uint32_t* block, Vc2SetReads& reads) {
    Vc2SetOscInput& w = at(object);
    const Vc2SetVoice v = reads.voiceAt(call.channelAt);
    voiceBytes(w, v);
    const Header h = reads.oscHeader(reads.firstByte(v.part));
    tunesDrawn(w, h, seed.value, reads);
    w.startKey = startKey(h, v, reads);
    vc2SetOscConvert(w, block, reads);
}

void Vc2SetOscBlocks::update(uint32_t object, const Vc2SetCall& call, Vc2SetReads& reads) {
    Vc2SetOscInput& w = at(object);
    const uint32_t part = vc2CallsPart(call, reads);
    recordOf(w, part, reads);
    const Header h = reads.oscHeader(part);
    const Systems sys = reads.oscSystems();
    coarseOf(w, h, sys);
    fineOf(w, h, sys, w.random, reads);
}

void Vc2SetOscBlocks::copy(uint32_t object, uint32_t* block, Vc2SetReads& reads) {
    vc2SetOscConvert(at(object), block, reads);
}

uint32_t Vc2SetOscBlocks::refresh(uint32_t object, const Vc2SetCall& call, Vc2SetReads& reads) {
    const Vc2SetOscInput& w = at(object);
    const std::array<uint8_t, 0x80> now = reads.oscRecord(vc2CallsPart(call, reads), w.kind, w.osc);
    const uint8_t type = w.record[kTypeAt];
    if (type != now[kTypeAt]) return 1;
    if (type == kAnalogType) return w.record[kWaveformAt] != now[kWaveformAt];
    if (type != kPcmType) return 0;
    const bool same = s16(w.record.data(), kWaveNumberAt) == s16(now.data(), kWaveNumberAt) &&
                      w.record[kWaveByteAt] == now[kWaveByteAt] && w.record[kWaveByteTwoAt] == now[kWaveByteTwoAt];
    return same ? 0 : 1;
}
