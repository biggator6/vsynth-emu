// The OSC voice's note-on by its kind (v232): rom.4's 0x893af0b0, which the note-on (0x893ae370) ends in.  Each kind's
// way is its row's (Vc2OscVoiceKinds.cpp): VariPhrase's, LITE's and the analog's own note-ons (0x893af420, v238;
// 0x893c7c30, v240; 0x893c22e0, v242) are ported in their own files; kinds 4, 5, 6 and 7 are inline here.  From v321
// kinds 5's and 6's updates too, which write the same P.  Every address is rom.4's.  HARDWARE_FIRMWARE.md, "The VC-2's
// OSC voice's note-on and targets", "The VC-2's kinds 5 and 6 updates".
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "Vc2OscVoiceKinds.h"

namespace {
// [FW] the object's words kinds 0, 4 and 7 set: its pitch (+0x70), its gain (+0x80) and its level (+0x84)
constexpr uint32_t kObjectPitch = 0x1cc, kObjectGain = 0x1dc, kObjectLevel = 0x1e0;
// [FW] P at +0x104: the carrier's gain and pitch (Vc2CarrierParams); kind 6's ratio, rate, gain, level
// (Vc2Kind6Params), its level the block's +0x18
constexpr uint32_t kP = 0x104, kP4 = 0x108, kP8 = 0x10c, kPc = 0x110, kKind6Level = 0x6c;
// one-off: rom.4's offsets in its voice (the vibrato's block; from v321 0x893afff0's view), declarations, not a job to
// template
constexpr uint32_t kVibratoBlock = 0xd4;   // [FW] the block's +0x80: what the vibrato's note-on is given
// [FW] 0x893afff0's view of the voice (Vc2Kind5Voice's offsets): the pitch's cents and its envelope's and vibrato's
// depths, the ratio's scale, the gain's vibrato depth and two factors; the bindings it reads (the pitch's, growl's and
// balance's envelopes, the growl's and balance's LFOs) and their outputs
constexpr uint32_t kCents = 0x5c, kPitchDepth = 0x70, kPitchModDepth = 0xfc, kRatioScale = 0x60;
constexpr uint32_t kGainModDepth = 0x100, kGainFirst = 0x88, kGainSecond = 0xb0;
constexpr size_t kPitchEnvelope = 0, kGrowlEnvelope = 1, kBalanceEnvelope = 2, kGrowlLfo = 5, kBalanceLfo = 6;
constexpr uint32_t kEnvelopeOut = 0x4c, kLfoOut = 0x38;

// The voice's words at `offset` as a part reads them: P for the carrier's init and kind 6's start (v266), the block's
// +0x80 for the vibrato's note-on (v267)
template <class Params>
Params paramsOf(const Vc2OscVoice& v, uint32_t offset = kP) {
    Params p;
    std::memcpy(&p, v.at(offset), sizeof p);
    return p;
}
} // namespace

void Vc2OscVoice::kindNoteOn(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscKindRow& row = vc2OscKindRow(kind());
    const Vc2OscTables& t = vc2OscTables();
    const Vc2OscTargets targets(*this, parts, shared);
    switch (row.noteOn) {
        case Vc2OscNoteOn::none: return;
        case Vc2OscNoteOn::one: setF32(kObjectPitch, 1.0f); return;
        case Vc2OscNoteOn::vari: noteOnVari(parts, shared); break;   // 0x893af420 (v238, Vc2OscPlayers.cpp)
        case Vc2OscNoteOn::lite: noteOnLite(parts, shared); break;   // 0x893c7c30 (v240, Vc2OscLite.cpp)
        case Vc2OscNoteOn::analog: noteOnAnalog(parts, shared); break;   // 0x893c22e0 (v242, Vc2OscAnalog.cpp)
        case Vc2OscNoteOn::extIn:
            setF32(kObjectPitch, targets.ratioOn(t.noteOn, t.noteOn.ratio));
            setF32(kObjectGain, targets.gain());
            setF32(kObjectLevel, targets.level());
            break;
        case Vc2OscNoteOn::carrier:
            setF32(kP, targets.gain());
            setF32(kP4, targets.ratioOn(t.noteOn, t.noteOn.ratio));
            carrier.init(paramsOf<Vc2CarrierParams>(*this), parts.osClock());   // 0x893ebed0
            break;
        case Vc2OscNoteOn::kind6:
            setF32(kObjectPitch, targets.ratioOn(t.noteOn, { t.noteOn.ratio.most, t.kind6Least }));
            setF32(kP8, targets.gain());
            setF32(kP, f32(kObjectPitch));
            setF32(kP4, targets.formant());
            setF32(kPc, f32(kKind6Level));
            kind6.setAndStart(parts.micRing(), paramsOf<Vc2Kind6Params>(*this));   // 0x893ec700
            break;
    }
    vibrato.noteOn(paramsOf<Vc2VibratoBlock>(*this, kVibratoBlock));
}

// 0x893afff0: P's growl, balance, gain and pitch by vc2Kind5Update (v222) on the voice's words, its bound modulators'
// outputs, the pitch ramp's value and factor, then the carrier's update with the input stage's peak
void Vc2OscVoice::updateCarrier(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const auto bound = [&](size_t which, uint32_t word) { return parts.boundFloat(tail.binds[which], word); };
    const Vc2PitchRamp& pitch = ramps[kPitchRamp];
    const Vc2Kind5Voice v { f32(kCents), bound(kPitchEnvelope, kEnvelopeOut), f32(kPitchDepth), vibrato.w.pitch,
                            f32(kPitchModDepth), pitch.value(), tail.offset, f32(kRatioScale), tail.rateRatio,
                            vibrato.w.level, f32(kGainModDepth), { f32(kGainFirst), f32(kGainSecond), pitch.factor() },
                            bound(kGrowlEnvelope, kEnvelopeOut), bound(kGrowlLfo, kLfoOut),
                            bound(kBalanceEnvelope, kEnvelopeOut), bound(kBalanceLfo, kLfoOut) };
    Vc2VocoderOscP p = paramsOf<Vc2VocoderOscP>(*this);
    vc2Kind5Update(v, shared, p, carrier, parts.inputPeak());
    std::memcpy(own.data() + (kP - kOwn), &p, sizeof p);
}

// 0x893b0150: P's ratio by the pitch's sum within its own bounds, the formant (0x893afeb0), the gain and kind 6's
// level, then kind 6's set (0x893ec790) on P; the object's pitch (+0x1cc), the note-on's, stays
void Vc2OscVoice::updateKind6(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscTargetPool& pool = vc2OscTables().kind6Update;
    const Vc2OscTargets targets(*this, parts, shared);
    setF32(kP, targets.ratioOn(pool, pool.ratio));
    setF32(kP4, targets.formant());
    setF32(kP8, targets.gain());
    setF32(kPc, f32(kKind6Level));
    const Vc2Kind6Params p = paramsOf<Vc2Kind6Params>(*this);
    kind6.set(&p);
}
