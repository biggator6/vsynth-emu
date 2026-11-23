// xt_vc2osc's port side: the voice's fake parts (Vc2OscCheck.h, v230, v232), logging each call as rom.4's hooks do and
// answering from the same draws and records.  From v319 xt_vc2tone's too: what its tones' real voices call beyond the
// tone.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscCheck.h"
#include <bit>

namespace {
// The record of `records` at `at`, by `where(k)`; an empty one when none is there (rom.4 would read what memory holds)
template <class Record, class Where>
Record recordAt(const std::array<Record, kOscRecords>& records, uint32_t at, Where where) {
    for (int k = 0; k < kOscRecords; ++k) {
        if (where(k) == at) return records[(size_t)k];
    }
    return Record {};
}
} // namespace

uint32_t OscPortParts::renderBy(uint32_t routine, float* out, uint32_t third) {
    log(kRenderBy, routine, outOf(out), third);
    return s_.draws.rendered;
}

float OscPortParts::boundFloat(uint32_t part, uint32_t offset) {
    const uint32_t in = part + offset - address();   // v328: the voice's own words, which its head names
    if (in - Vc2OscVoice::kOwn <= Vc2OscVoice::kOwnEnd - Vc2OscVoice::kOwn - sizeof(float)) return self_.f32(in);
    const OscEventTable& events = s_.records.events;   // v327: mode 1's events' table, as rom.4's memory holds it
    if (events.holds(part + offset)) return std::bit_cast<float>(events.wordAt(part + offset));
    const uint32_t inWave = part + offset - kOscWaveAt;   // v332: the wave memory, the band players' records in it
    if (inWave <= s_.wave.size() * sizeof(int16_t) - sizeof(float)) {
        float word = 0.0f;
        std::memcpy(&word, reinterpret_cast<const uint8_t*>(s_.wave.data()) + inWave, sizeof word);
        return word;
    }
    const auto word = s_.records.bound.find(part + offset);
    return word == s_.records.bound.end() ? 0.0f : word->second;
}

Vc2OscWaveHeader OscPortParts::waveHeader(uint32_t at) { return recordAt(s_.records.headers, at, oscHeaderAt); }

// v333: one in the wave memory as it holds it (a band wave's own, which the band players' set-ups read in xt_vc2osc's
// band part)
Vc2OscLoopRecord OscPortParts::loopRecord(uint32_t at) {
    constexpr uint32_t kLoopBytes = 0x1c;   // [FW] its fields' span
    const uint32_t inWave = at - kOscWaveAt;
    if (inWave <= s_.wave.size() * sizeof(int16_t) - kLoopBytes) {
        return oscLoopFrom(reinterpret_cast<const uint8_t*>(s_.wave.data()) + inWave);
    }
    return recordAt(s_.records.loops, at, oscLoopAt);
}

#endif
