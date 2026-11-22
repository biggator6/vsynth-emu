// The VC-2 part's settings by ID, the vocoder reader 0x893a72f0 (v290): see Vc2PartSettings.h.  Its reads, the
// vocoder's settings it fills and what it hands on, in rom.4's order, its floats in XtFloat's arithmetic.
#include "Vc2LibMath.h"
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2PartSettingsWords.h"
#include "XtFloat.h"

using namespace Vc2PartWords;

namespace {
using Block = Vc2ParamBlock;
// [FW] the part's IDs: Formant Shift; Vocoder Sound Level, Vocoder Depth, Unvoice Level, Unvoice Detect, Unvoice
// Spread, Env Attack and Release, Formant Template, Hold Dump Time; Direct Sound Level; Auto Note On Switch, Legato
// Switch, robot human depth; Vocoder Sw and Type; Stereo Process Sw
constexpr int16_t kFormantShift = 0x19f, kSoundLevel = 0x196, kDepth = 0x197, kUnvoiceLevel = 0x199;
constexpr int16_t kDetect = 0x198, kSpread = 0x1a5, kAttack = 0x19a, kRelease = 0x19b, kTemplate = 0x1a0;
constexpr int16_t kDump = 0x19e, kDirect = 0x195, kAutoNote = 0x19c, kLegato = 0x131, kRobot = 0x1a3;
constexpr int16_t kVocoderSw = 0x193, kVocoderType = 0x194, kStereo = 0x1a4;
// [FW] a kind's (its first block): the Reverb and MFX Sends, its first OSC's Type
constexpr int16_t kReverbSend = 0x2e2, kMfxSend = 0x2e0, kOscType = 0x2e5;
constexpr int kFirstKind = 0, kSecondKind = 1;
// [FW] Unvoice Detect's offset, OSC Type EXTIN, Stereo Process Sw's on, the templates (each with a record a side)
constexpr uint32_t kDetectFrom = 24;
constexpr int32_t kExtin = 2, kStereoOn = 1;
constexpr uint32_t kTemplates = 16;
constexpr float kTwo = 2.0f;

// A value on Formant Shift's scale as a power of two's exponent: x × 1/63 − 64/63, rom.4's FMAC
float formantExponent(float x) {
    const Vc2PartSettingsTables& tb = vc2PartSettingsTables();
    return xtf::add(xtf::mul(x, tb.shiftStep), tb.shiftBase);
}

float per127(int32_t v) { return xtf::mul(xtf::flt(v), vc2PartSettingsTables().per127); }
} // namespace

bool Vc2PartSettings::vocoder(Vc2PartRows& t, int part, Vc2SettingsParts& out) {
    if (p_.held(kHeldCommon)) return false;
    const auto get = [&](int16_t id) { return p_.value(Block::kPart, part, 0, id); };
    const auto kindGet = [&](int kind, int16_t id) { return p_.value(Block::kKindA, part, kind, id); };
    Vc2VocoderSettings s {};
    float shift = xtf::flt(get(kFormantShift));
    if (shift == 0.0f) shift = 1.0f;
    s.ratio[0] = vc2LibPow(kTwo, formantExponent(shift));
    s.ratio[1] = s.ratio[0];
    s.soundLevel = per127(get(kSoundLevel));
    s.depth = per127(get(kDepth));
    s.unvoiceLevel = per127(get(kUnvoiceLevel));
    s.detect = static_cast<int32_t>(static_cast<uint32_t>(get(kDetect)) + kDetectFrom);
    s.spread = get(kSpread) != 0 ? 1 : 0;
    s.attack = get(kAttack);
    s.release = get(kRelease);
    s.formantTemplate = get(kTemplate);
    s.dump = per127(get(kDump));
    out.setUnvoiceLevel(s.unvoiceLevel);
    out.setDirect(per127(get(kDirect)));
    int32_t firstType = 0;
    if (!p_.held(kHeldKind)) {
        out.setReverbSend(per127(kindGet(kFirstKind, kReverbSend)));
        out.setMfxSend(per127(kindGet(kFirstKind, kMfxSend)));
        firstType = kindGet(kFirstKind, kOscType);
    }
    out.mixerTargets();
    if (p_.held(kHeldKind)) return false;
    const int32_t secondType = kindGet(kSecondKind, kOscType);
    int32_t on = 0;
    t.robot = 0.0f;
    if (firstType == kExtin || secondType == kExtin) {
        on = get(kAutoNote);
        get(kLegato);   // read, its answer tested and dropped
        t.robot = per127(get(kRobot));
    }
    out.setAutoNote(on);
    out.setAutoNoteLegato();
    const auto record = static_cast<uint32_t>(s.formantTemplate);
    if (on > 0 && record < kTemplates) {
        const auto& shifts = vc2PartSettingsTables().templateShifts;
        s.offset[0] = formantExponent(shifts[record]);
        s.offset[1] = formantExponent(shifts[record + kTemplates]);
    }
    const int32_t vocoderOn = get(kVocoderSw);
    const int32_t type = get(kVocoderType);
    out.setVocoderMode(times(static_cast<int32_t>(static_cast<uint32_t>(type) + 1u), vocoderOn));
    out.vocoderSettings(s, get(kStereo) == kStereoOn ? 1 : 0);
    return !p_.held(kHeldCommon);
}
