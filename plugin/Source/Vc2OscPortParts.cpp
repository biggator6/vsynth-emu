// xt_vc2osc's port side: the voice's fake parts (Vc2OscCheck.h, v230, v232), logging each call as rom.4's hooks do and
// answering from the same draws and records.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
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

void OscPortParts::seek(uint32_t routine, int32_t at) {
    log(kSeek, routine, (uint32_t)at, self_.u8(Vc2OscVoice::kObject));   // the object's flags as the player finds them
    self_.setU8(Vc2OscVoice::kObject, s_.draws.seekFlags);              // and as it leaves them
}

float OscPortParts::boundFloat(uint32_t part, uint32_t offset) {
    const auto word = s_.records.bound.find(part + offset);
    return word == s_.records.bound.end() ? 0.0f : word->second;
}

Vc2OscWaveHeader OscPortParts::waveHeader(uint32_t at) { return recordAt(s_.records.headers, at, oscHeaderAt); }

Vc2OscLoopRecord OscPortParts::loopRecord(uint32_t at) { return recordAt(s_.records.loops, at, oscLoopAt); }

int32_t OscPortParts::eventPosition(uint32_t event) {   // the next of the drawn positions, as rom.4's hook gives them
    constexpr uint32_t kLookup = 0x893cc0d0;   // [FW] the routine rom.4's hook logs it as
    log(kEventPosition, kLookup, event);
    return s_.draws.positions[s_.positionsTaken++ % s_.draws.positions.size()];
}
#endif
