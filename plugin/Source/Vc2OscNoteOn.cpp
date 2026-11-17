// The OSC voice's note-on by its kind (v232): rom.4's 0x893af0b0, which the note-on (0x893ae370) ends in.  Each kind's
// way is its row's (Vc2OscVoiceKinds.cpp): VariPhrase's, LITE's and the analog's own note-ons (0x893af420, v238;
// 0x893c7c30, v240; 0x893c22e0, v242) are ported in their own files; kinds 4, 5, 6 and 7 are inline here.
// Every address is rom.4's.  HARDWARE_FIRMWARE.md, "The VC-2's OSC voice's note-on and targets".
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "Vc2OscVoiceKinds.h"

namespace {
// [FW] the object's words kinds 0, 4 and 7 set: its pitch (+0x70), its gain (+0x80) and its level (+0x84)
constexpr uint32_t kObjectPitch = 0x1cc, kObjectGain = 0x1dc, kObjectLevel = 0x1e0;
// [FW] P at +0x104: the carrier's gain and pitch (Vc2CarrierParams); kind 6's ratio, rate, gain, level
// (Vc2Kind6Params), its level the block's +0x18
constexpr uint32_t kP = 0x104, kP4 = 0x108, kP8 = 0x10c, kPc = 0x110, kKind6Level = 0x6c;
constexpr uint32_t kVibratoBlock = 0xd4;   // [FW] the block's +0x80: what the vibrato's note-on is given
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
            parts.initCarrier(at(kP));
            break;
        case Vc2OscNoteOn::kind6:
            setF32(kObjectPitch, targets.ratioOn(t.noteOn, { t.noteOn.ratio.most, t.kind6Least }));
            setF32(kP8, targets.gain());
            setF32(kP, f32(kObjectPitch));
            setF32(kP4, targets.formant());
            setF32(kPc, f32(kKind6Level));
            parts.startKind6(at(kP));
            break;
    }
    parts.noteOnVibrato(at(kVibratoBlock));
}
