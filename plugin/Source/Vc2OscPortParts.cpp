// xt_vc2osc's port side: the voice's fake parts (Vc2OscCheck.h, v230), logging each call as rom.4's hooks do and
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

float OscPortParts::rampValue(int ramp) {
    log(kRampValue, (uint32_t)ramp);
    return s_.draws.rampValue;
}

uint32_t OscPortParts::renderBy(uint32_t routine, float* out, uint32_t third) {
    log(kRenderBy, routine, outOf(out), third);
    return s_.draws.rendered;
}

float OscPortParts::kindValue(uint32_t routine) {
    log(kKindValue, routine);
    return s_.draws.kindValue;
}

void OscPortParts::seek(uint32_t routine, int32_t at) {
    log(kSeek, routine, (uint32_t)at, self_.u8(Vc2OscVoice::kObject));   // the object's flags as the player finds them
    self_.setU8(Vc2OscVoice::kObject, s_.draws.seekFlags);              // and as it leaves them
}

float OscPortParts::envelopeOut(uint32_t envelope) { return recordAt(s_.records.envelopes, envelope, oscEnvelopeAt); }

Vc2OscSample OscPortParts::sample(uint32_t at) { return recordAt(s_.records.samples, at, oscSampleAt); }

Vc2OscMap OscPortParts::map(uint32_t at) { return recordAt(s_.records.maps, at, oscMapAt); }

void OscPortParts::logFloats(OscKind kind, int ramp, float a, float b) {
    log(kind, (uint32_t)ramp, std::bit_cast<uint32_t>(a), std::bit_cast<uint32_t>(b));
}
#endif
