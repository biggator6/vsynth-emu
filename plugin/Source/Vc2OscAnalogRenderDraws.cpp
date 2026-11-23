// The analog's renders' draws for xt_vc2osc and xt_vc2tone (v337): an analog voice's render word one of the 19 the
// installs write, and the words they read as the XT has them where a drawn word is one it never has.  From the
// analog's own generator, so the other draws stay as they were.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2OscAnalog.h"
#include "Vc2OscDraws.h"
#include "Vc2OscPlayers.h"
#include "XtAnalogWords.h"
#include <algorithm>
#include <cmath>

namespace {
// [FW] the renders' integer words among XtAnalogWords' (rom.2's object): the flags, HQ's direction, NOISE's second
// state and its hold
constexpr std::array<uint32_t, 4> kIntegers { 0x108, 0x114, 0x170, 0x174 };

// The render's float words finite where drawn as a NaN or an infinity, 0…1, where the XT's mostly lie: it holds none
// of those, a NaN's sign through xtf's arithmetic is not always the interpreter's (its FADD's and FSUB's negated), and
// some reach past the render (the ratio is COSM 1's pitch: a moderate draw's −1000 led rom.4's tone render to a null
// routine under COSM 1's Lo-Fi, code 19)
long finiteWords(Vc2OscVoice& o, std::mt19937& rng) {
    long drawn = 0;
    for (const XtWord& w : XtAnalogWords::words()) {
        const uint32_t at = vc2OscInRom4(w.at);
        if (std::ranges::find(kIntegers, w.at) != kIntegers.end() || std::isfinite(o.f32(at))) continue;
        o.setF32(at, vc2Unit(rng));
        ++drawn;
    }
    return drawn;
}

// The block's output by the port on a copy of the voice: finite and within 16 × full scale, as the XT's renders give it
// (Impact's 4 over a wave of ±1 at a gain about 1: five times)
bool outputInRange(const Vc2OscVoice& o) {
    constexpr float kMost = 16.0f * 2147483648.0f;
    Vc2OscVoice trial = o;
    std::array<float, Vc2OscVoice::kOut> out {};
    trial.renderAnalog(trial.tail.renderAnalog, out.data());
    return std::ranges::all_of(out, [](float x) { return std::fabs(x) <= kMost; });   // a NaN's false
}

bool checkRender(uint32_t word) {
    for (int k = 0; k < kOscRenders; ++k) {
        if (oscRenderAt(k) == word) return true;
    }
    return false;
}

// The render word one of the check's own renders, where the words would give what no XT's analog voice does
void toCheckRender(Vc2OscVoice& o, OscShared& shared) {
    o.tail.renderAnalog = oscRenderAt((int)(shared.analogRng() % kOscRenders));
    ++shared.analogOutside;
}
} // namespace

// One time in two the render word the install's (0x893c2ad0, the voice's own; its +0xa40 and +0xa88 as they were), the
// one for the code a note-on left and the sub oscillator's switch, as on the XT, where a render's words are its own
// note-on's and update's (the check's draws name its own renders, a note-on of another code leaves another's: LA's and
// the sub oscillator's gain targets ×2^26 through a plain code's render go out of every range).  A word naming none of
// the 19 nor the check's (an analog voice no analog note-on reached: its code copy another kind's word, so no render,
// which rom.4's frame calls at 0x893aea84 all the same) one of the check's.  Its float words finite; NOISE's hold 1…31,
// as its note-on gives it from a finite Pulse Width, where a drawn byte (or a NaN Pulse Width's) is 0 or below, with
// which rom.4's NOISE never ends its block (0x893c22b8: the count goes down only while the hold is above 0).  Then,
// where what the words still give is no XT's (a phase far past 1, a Fat of 10^28: a NaN or no bound, which xtf's
// arithmetic and the interpreter's carry with other signs into the mix and the COSMs), one of the check's own renders
void oscSteerAnalogRender(Vc2OscVoice& o, OscShared& shared) {
    constexpr uint32_t kAnalog = 3, kNoise = 8, kHolds = 31;   // [FW] the analog's kind; NOISE's code; its holds
    if (o.kind() != kAnalog) return;
    std::mt19937& rng = shared.analogRng;
    if (rng() % 2) {
        const auto kept = std::pair { o.tail.flag, o.tail.installed };
        o.installAnalog();
        std::tie(o.tail.flag, o.tail.installed) = kept;
    }
    const std::optional<Vc2OscAnalog::Vc2AnalogRoutine> r = Vc2OscAnalog::routineAt(o.tail.renderAnalog);
    if (!r && !checkRender(o.tail.renderAnalog)) toCheckRender(o, shared);
    if (!r) return;
    shared.analogFinite += finiteWords(o, rng);
    if (r->code == kNoise && (int8_t)o.u8(Vc2OscAnalog::kNoiseHold) <= 0) {
        o.setU8(Vc2OscAnalog::kNoiseHold, (uint8_t)(1 + rng() % kHolds));
        ++shared.analogHolds;
    }
    if (outputInRange(o)) {
        ++shared.analogBlocks;
        return;
    }
    toCheckRender(o, shared);
}
#endif
