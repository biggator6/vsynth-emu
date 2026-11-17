// The analog OSC's install, update and settings (v248): rom.4's 0x893c2ad0, 0x893c2ce0 and 0x893c3460, kind 3's in the
// OSC voice's frame (its row's Vc2OscInstall::analog, Vc2OscUpdate::analog, Vc2OscSettings::analog); see
// Vc2OscAnalog.h.  rom.2's install and update (XtAnalog's update and shape) on rom.4's own words: the update's plain
// codes are the note-on's laws (codeTargets, Vc2OscAnalog.cpp) on its words, LA's its targets (laTargets,
// Vc2OscAnalogLa.cpp) on its words, the sub oscillator's v246's shape.  0x893c30c0, LA's note-on as a function of its
// own, is not here: nothing in rom.4 calls it (the note-on has LA's way inline).  Every address is rom.4's; every
// offset the voice's.  HARDWARE_FIRMWARE.md, "The VC-2's analog install, update and settings".  Audio thread.
#include "Vc2OscAnalog.h"
#include "XtFloat.h"
#include <array>

using xtf::add;
using xtf::mul;
using xtf::sub;
using namespace Vc2OscAnalog;

namespace {
constexpr uint32_t kLaFirst = 9, kLaCodes = 2;   // [FW] 0x893c2d1a: codes 9 and 10 take LA's targets (0x893c31d0)
// [FW] the installs' render routines by code, the sub oscillator off (0x893c2b6c's cases) and on (0x893c3e8c's), by
// rom.4's address; past 10 the first installs none, the second NOISE's (code 8, its float too)
constexpr std::array<uint32_t, kVc2AnalogCodes> kRenders { 0x893c3890, 0x893c3580, 0x893c5680, 0x893c5500, 0x893c5f20,
                                                           0x893c5260, 0x893c5a90, 0x893c5810, 0x893c2230, 0x893c4fa0,
                                                           0x893c4c50 };
constexpr std::array<uint32_t, kVc2AnalogCodes> kSubRenders { 0x893c3890, 0x893c3580, 0x893c5990, 0x893c5dc0,
                                                              0x893c60e0, 0x893c53c0, 0x893c5c90, 0x893c4ac0,
                                                              0x893c2230, 0x893c50d0, 0x893c4db0 };
constexpr uint32_t kSubPast = 8;
// [FW] 0x893c3460: the settings' words it copies onto the block, by their offset past the settings' +0xb0, which is the
// block's code (+0x104): Fat, its envelope's and LFO's depths, the Pulse Width and its two, the Slope, the sub level,
// the Tone and its keyfollow's value
constexpr std::array<uint32_t, 10> kCopied { 0x04, 0x08, 0x10, 0x18, 0x1c, 0x20, 0x30, 0x38, 0x3c, 0x40 };

bool subOn(const Vc2OscVoice& v) { return xtf::ftrc(v.f32(kSubOn)) != 0; }   // [FW] FTRC of the block's +0x138

// [FW] 0x893c2d70…: the gain's target as the targets give it; Fat's target 70% of the way to its half, at least its
// floor; the Pulse Width's target by the code (codeTargets: past 8 none); on the update's own words
void plainUpdate(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s, uint32_t code) {
    const Vc2OscCodeWords& t = vc2OscTables().analog.updateCodes;
    v.setF32(kGainTarget, Vc2OscTargets(v, p, s).gain());
    const float half = sub(t.half, mul(sum(v, p, kFat, kFatMod), t.half));
    v.setF32(kFatTarget, add(mul(sub(half, v.f32(kFatTarget)), t.fatMove), v.f32(kFatTarget)));
    if (!(v.f32(kFatTarget) > t.fatMin)) v.setF32(kFatTarget, t.fatFloor);
    codeTargets(v, add(mul(sum(v, p, kPw, kPwMod), t.pwSpan), t.pwLo), code, t);
}
} // namespace

// [FW] 0x893c2ad0: +0xa40 cleared; the sub oscillator on, its own install (0x893c3e60); else by the code its render
// routine (+0xa48) and float (+0xa88), past 10 none
void Vc2OscVoice::installAnalog() {
    const Vc2OscAnalogPool& a = vc2OscTables().analog;
    tail.flag = 0;
    const uint32_t code = u32(kCodeCopy);
    if (subOn(*this)) {
        const uint32_t at = code < kVc2AnalogCodes ? code : kSubPast;
        tail.renderAnalog = kSubRenders[at];
        tail.installed = a.subInstalled[at];
        return;
    }
    if (code >= kVc2AnalogCodes) return;
    tail.renderAnalog = kRenders[code];
    tail.installed = a.installed[code];
}

// [FW] 0x893c2ce0: the ratio's target into +0x1cc; then the sub oscillator's shape (0x893c4020), LA's targets on the
// update's words (0x893c31d0) or the plain codes'
void Vc2OscVoice::updateAnalog(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    setF32(kRatio, Vc2OscTargets(*this, parts, shared).ratio());
    if (subOn(*this)) {
        subShape(*this, parts, shared);
        return;
    }
    const uint32_t code = u32(kCodeCopy);
    if (code - kLaFirst < kLaCodes) laTargets(*this, parts, shared, code, vc2OscTables().analog.updateLa);
    else plainUpdate(*this, parts, shared, code);
}

// [FW] 0x893c3460: the ten words, as they are
void Vc2OscVoice::settingsAnalog(const uint32_t* from) {
    for (uint32_t at : kCopied) setU32(kCode + at, from[at / 4]);
}
