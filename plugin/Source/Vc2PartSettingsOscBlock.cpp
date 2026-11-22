// The VC-2 part's settings by ID, the OSC block's reader 0x893a04e0 (v289): see Vc2PartSettings.h.  Its reads from
// the kind's first block and stores into the OSC table's block, a row each in rom.4's order.
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsWords.h"
#include <cstring>

using namespace Vc2PartWords;

namespace {
using Block = Vc2ParamBlock;
using OscStore = void (*)(uint8_t* at, int32_t v);

void tens(uint8_t* at, int32_t v) { put16(at, times(v, 10)); }
void hundreds(uint8_t* at, int32_t v) { put16(at, times(v, 100)); }

// [FW] Random Pitch Depth's cents × 10 by its value, rom.4's switch of 30: each decade of values by its step from the
// value it counts from (0…9 by tens, 10…19 by hundreds from 100, 20…29 by thousands from 2000), past 29 (as
// unsigned) 12000
struct PitchDecade {
    uint32_t below, step, from;
};
constexpr PitchDecade kDecades[] = { { 10, 10, 0 }, { 20, 100, 9 }, { 30, 1000, 18 } };
constexpr int32_t kRandomPast = 12000;

void randomPitch(uint8_t* at, int32_t v) {
    const auto u = static_cast<uint32_t>(v);
    for (const PitchDecade& d : kDecades)
        if (u < d.below) return put16(at, static_cast<int32_t>(d.step * (u - d.from)));
    put16(at, kRandomPast);
}

// [FW] a read: OSC 1's ID, the step to OSC 2's (by the OSC, or once for any OSC past 1), its place, its store
struct OscRead {
    int16_t id;
    uint8_t step;
    bool once;
    uint8_t at;
    OscStore store;
};

// [FW] up to the Wave Number: Switch, Type, Wave Gain, Pitch, Pitch Keyfollow, its LFO and Envelope Depths, Coarse and
// Fine Tune, Random Pitch Depth, Level, its Keyfollow and LFO Depth; Vibrato Type, Rate and Depth; the analog's
// Waveform, Impact, Pulse Width and its three, Fat and its three; Detune and the Sub Oscillator's Octave and Level;
// the carrier's fourteen (Saw Wave LPF cutoff … Mix Balance Envelope Depth)
constexpr OscRead kFirst[] = {
    { 0x2e4, 52, false, 0x00, put8 }, { 0x2e5, 52, false, 0x01, put8 }, { 0x2e6, 52, false, 0x02, put8 },
    { 0x2e7, 52, false, 0x04, put8 }, { 0x2e8, 52, false, 0x06, hundreds }, { 0x2e9, 52, false, 0x08, put8 },
    { 0x2ea, 52, false, 0x09, put8 }, { 0x2eb, 52, false, 0x0e, put8 }, { 0x2ec, 52, false, 0x10, tens },
    { 0x2ed, 52, false, 0x12, randomPitch }, { 0x2ee, 52, false, 0x14, put8 }, { 0x2ef, 52, false, 0x16, tens },
    { 0x2f0, 52, false, 0x18, put8 },
    { 0x37c, 4, false, 0x1c, put8 }, { 0x37d, 4, false, 0x1d, put8 }, { 0x37e, 4, false, 0x1e, put8 },
    { 0x2f1, 52, false, 0x20, put8 }, { 0x2f2, 52, false, 0x21, put8 }, { 0x2f3, 52, false, 0x22, put8 },
    { 0x2f4, 52, false, 0x24, tens }, { 0x2f5, 52, false, 0x26, put8 }, { 0x2f6, 52, false, 0x27, put8 },
    { 0x2f7, 52, false, 0x2c, put8 }, { 0x2f8, 52, false, 0x2e, tens }, { 0x2f9, 52, false, 0x30, put8 },
    { 0x2fa, 52, false, 0x31, put8 },
    { 0x34d, 3, true, 0x36, put8 }, { 0x34e, 3, true, 0x37, put8 }, { 0x34f, 3, true, 0x38, put8 },
    { 0x354, 14, false, 0x64, put8 }, { 0x355, 14, false, 0x65, put8 }, { 0x356, 14, false, 0x66, put8 },
    { 0x357, 14, false, 0x67, put8 }, { 0x358, 14, false, 0x68, put8 }, { 0x359, 14, false, 0x74, put8 },
    { 0x35a, 14, false, 0x75, put8 }, { 0x35b, 14, false, 0x6a, put8 }, { 0x35c, 14, false, 0x6c, tens },
    { 0x35d, 14, false, 0x6e, put8 }, { 0x35e, 14, false, 0x6f, put8 }, { 0x35f, 14, false, 0x76, put8 },
    { 0x360, 14, false, 0x7a, put8 }, { 0x361, 14, false, 0x7b, put8 },
};

// [FW] the Wave Number, a halfword, and the step counts it clears: part 0's, by kind and OSC
constexpr OscRead kWave = { 0x2fe, 52, false, 0x3c, put16 };
constexpr size_t kStepPart = 64, kStepKind = 4, kStepOsc = 2;

// [FW] after it: Start Offset (a word); Time and its three; Formant and its three; Tempo Sync, Robot Voice, Playback
// Mode, Loop, Vari, Energy, Time Offset (read late), Time Trip and its Beat Keep
constexpr OscRead kAfter[] = {
    { 0x306, 52, false, 0x40, put32 }, { 0x307, 52, false, 0x44, put8 }, { 0x308, 52, false, 0x46, tens },
    { 0x309, 52, false, 0x48, put8 }, { 0x30a, 52, false, 0x49, put8 }, { 0x30c, 52, false, 0x4e, put8 },
    { 0x30d, 52, false, 0x50, tens }, { 0x30e, 52, false, 0x52, put8 }, { 0x30f, 52, false, 0x53, put8 },
    { 0x310, 52, false, 0x58, put8 }, { 0x311, 52, false, 0x59, put8 }, { 0x312, 52, false, 0x5a, put8 },
    { 0x313, 52, false, 0x5b, put8 }, { 0x314, 52, false, 0x5c, put8 }, { 0x315, 52, false, 0x5e, put8 },
    { 0x30b, 52, false, 0x5d, put8 }, { 0x316, 52, false, 0x5f, put8 }, { 0x317, 52, false, 0x60, put8 },
};

// [FW] kind 0's OSC of Type 2 takes its Harm Formant, Keyfollow and Style in the Formant's and Energy's places
constexpr OscRead kHarm[] = {
    { 0x371, 5, true, 0x4e, put8 }, { 0x372, 5, true, 0x50, tens }, { 0x375, 5, true, 0x5e, put8 },
};
constexpr int kHarmKind = 0, kHarmType = 2, kTypeAt = 1;

// A read's ID for the OSC
int16_t oscReadId(const OscRead& r, int osc) {
    const int n = r.once ? (osc != 0 ? 1 : 0) : osc;
    return static_cast<int16_t>(r.id + r.step * n);
}
} // namespace

// The Wave Number's halfword; when it changes, the OSC's step count goes to 0.  Past part 0 rom.4 writes over what
// follows its table (the update queue at 0x89989cc4), which its handler 0x89392830 never asks: the port writes nothing
void Vc2PartSettings::waveNumber(Vc2PartRows& t, uint8_t* at, int32_t v, int part, int kind, int osc) {
    uint16_t was = 0;
    std::memcpy(&was, at, sizeof was);
    put16(at, v);
    if (static_cast<uint16_t>(v) == was) return;
    const size_t step = kStepPart * static_cast<size_t>(part) + kStepKind * kind + kStepOsc * osc;
    if (step < t.steps.size()) put16(t.steps.data() + step, 0);
}

bool Vc2PartSettings::oscBlock(Vc2PartRows& t, int part, int kind, int osc) {
    if (p_.held(kHeldKind)) return false;
    uint8_t* block = oscBlockAt(t, part, kind, osc);
    const auto read = [&](const OscRead& r) { return p_.value(Block::kKindA, part, kind, oscReadId(r, osc)); };
    for (const OscRead& r : kFirst) r.store(block + r.at, read(r));
    waveNumber(t, block + kWave.at, read(kWave), part, kind, osc);
    for (const OscRead& r : kAfter) r.store(block + r.at, read(r));
    if (kind == kHarmKind && block[kTypeAt] == kHarmType)
        for (const OscRead& r : kHarm) r.store(block + r.at, read(r));
    return !p_.held(kHeldKind);
}
