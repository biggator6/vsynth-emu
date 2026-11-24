// The part's tones: see Vc2SynthTone.h (v340).  Every address is rom.4's.
#include "Vc2SynthTone.h"
#include "Vc2Unvoiced.h"
#include "Vc2WaveStore.h"
#include "XtWord.h"
#include <algorithm>
#include <array>
#include <cstddef>

namespace {
// [FW] a wave header's fields the voice reads and a loop record's (0x1c bytes), by their places in the wave memory
constexpr std::array<XtWord, 6> kHeaderFields { {
    { 0x01, offsetof(Vc2OscWaveHeader, format), 1 },
    { 0x02, offsetof(Vc2OscWaveHeader, flag), 1 },
    { 0x08, offsetof(Vc2OscWaveHeader, rate), 2 },
    { 0x0a, offsetof(Vc2OscWaveHeader, rootKey), 1 },
    { 0x18, offsetof(Vc2OscWaveHeader, cents), 4 },
    { 0x1c, offsetof(Vc2OscWaveHeader, word1c), 4 },
} };
constexpr std::array<XtWord, 10> kLoopFields { {
    { 0x00, offsetof(Vc2OscLoopRecord, tune), 4 },
    { 0x04, offsetof(Vc2OscLoopRecord, loopStart), 4 },
    { 0x08, offsetof(Vc2OscLoopRecord, loopEnd), 4 },
    { 0x0c, offsetof(Vc2OscLoopRecord, bwdEnd), 4 },
    { 0x10, offsetof(Vc2OscLoopRecord, bwdStart), 4 },
    { 0x14, offsetof(Vc2OscLoopRecord, rateFwd), 2 },
    { 0x16, offsetof(Vc2OscLoopRecord, rateBwd), 2 },
    { 0x18, offsetof(Vc2OscLoopRecord, fwdMode), 1 },
    { 0x19, offsetof(Vc2OscLoopRecord, bwdMode), 1 },
    { 0x1b, offsetof(Vc2OscLoopRecord, fine), 1 },
} };

// A record of the wave memory at `at` by its fields; a default one with no waves given
template <class Record, size_t N>
Record fromWaves(const Vc2WaveMemory* waves, uint32_t at, const std::array<XtWord, N>& fields) {
    Record record;
    if (waves == nullptr) return record;
    auto* to = reinterpret_cast<uint8_t*>(&record);
    for (const auto& [place, member, bytes] : fields) {
        for (size_t k = 0; k < bytes; ++k) to[member + k] = waves->byte(at + place + (uint32_t)k);
    }
    return record;
}

// A block of the chip's frames when the part is given none: 32 frames of 8 words, silence
constexpr std::array<int32_t, 32 * 8> kNoFrames {};
} // namespace

// No routine an install writes is left by address; a word naming another (a type or code with no player) renders
// nothing, where rom.4 would run what the word names
uint32_t Vc2SynthOscOutside::renderBy(uint32_t, float* out, uint32_t) {
    std::fill(out, out + Vc2OscVoice::kOut, 0.0f);
    return 0;
}

const float* Vc2SynthOscOutside::unvoiced() { return w_.inputStage().smoothed(); }   // 0x899d99dc
float Vc2SynthOscOutside::inputPeak() { return w_.inputStage().peak(); }             // 0x899d9a5c

const int32_t* Vc2SynthOscOutside::inputFrames() {
    const int32_t* frames = w_.inputFrames();
    return frames != nullptr ? frames : kNoFrames.data();
}

// Another tone's envelope's or LFO's word, its host's; 0 for an address in no tone
float Vc2SynthOscOutside::boundFloat(uint32_t part, uint32_t offset) {
    const uint32_t at = part + offset;
    if (!pool_.holds(at)) return 0.0f;
    const int tone = pool_.indexOf(at);
    return w_.host(tone).mods.boundFloat(pool_.addressOf(tone), part, offset);
}

Vc2OscWaveHeader Vc2SynthOscOutside::waveHeader(uint32_t at) {
    return fromWaves<Vc2OscWaveHeader>(w_.waveMemory(), at, kHeaderFields);
}

Vc2OscLoopRecord Vc2SynthOscOutside::loopRecord(uint32_t at) {
    return fromWaves<Vc2OscLoopRecord>(w_.waveMemory(), at, kLoopFields);
}

Vc2SynthTone::Vc2SynthTone(int tone, const Vc2ToneCommon& common, Vc2SynthToneWorld& world)
    : Vc2ToneHost(tone, common), w_(world), outside_(world, common.pool) {}
