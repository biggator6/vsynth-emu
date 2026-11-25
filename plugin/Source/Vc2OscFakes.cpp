// xt_vc2osc's rom.4 side: see Vc2OscCheck.h (v230, v232).  Every routine the OSC voice calls hooked, each row of the
// table with its own reading of the arguments and its answer; the records the voice reads written where its words
// point.  The port's fake parts are Vc2OscPortParts.cpp.  From v319 xt_vc2tone's too, whose tones' voices call the
// same: a call names its voice by a check's own numbering of the voices' addresses.
// XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#if defined(XT_VC2OSC_MAIN) || defined(XT_VC2TONE_MAIN)
#include "Vc2CheckCalls.h"
#include "Vc2OscCheck.h"
#include <algorithm>

namespace {
constexpr uint32_t kOsClock = 0x893018d0;   // [FW] the OS's millisecond word, which a carrier's init reads (v266)

uint32_t outOf(uint32_t at) { return at == kOscOutAt ? 1 : kOscOther; }

// How a hooked routine's arguments read into its call, one shape each; each gives the address of the voice the call is
// on (its object's less the object's offset; v330: a kind's routine on the voice itself went with the last of them)
uint32_t renderBy(Sh4& c, OscCall& t, uint32_t at) {   // (out, the object, third)
    t.a = at;
    t.b = outOf(c.r[4]);
    t.c = c.r[6];
    return c.r[5] - Vc2OscVoice::kObject;
}

// How a hooked routine answers: its r0 (v335: a set-up's none and a seek's flags went with the last of them)
uint32_t rendered(Sh4&, const OscDraws& d) { return d.rendered; }
// A hooked routine: where, what it logs as, how its arguments read, its answer (v328's answering by the caller went
// with v331, the event lookup now the voice's own)
struct OscHook {
    uint32_t at;
    OscKind kind;
    uint32_t (*decode)(Sh4&, OscCall&, uint32_t);
    uint32_t (*answer)(Sh4&, const OscDraws&);
};
// [FW] every routine the voice's frame and the kind's note-on call but the ramps' (v236: rom.4's own run), LITE's
// and VariPhrase's installs and VariPhrase's note-on (v238), LITE's note-on (v240), the analog's (v242) and its sub
// oscillator's (v244) with its shape (v246), the analog's install, update and settings (v248), the carrier's init,
// settings and render and kind 6's start and render (v266), the vibrato's four (v267), kinds 5's and 6's updates
// (v321), the EXT IN player's update and two renders (v322), Vari off's two renders (v323), LITE's update and
// settings (v324), the players' beat start (0x893cbf40) with Vari off's and the heads' seeks (v325), LITE's heads'
// renders (v326) and kind 1's mode 1's (v327) with its set-up (v328) and seek (v329), LITE whole, and VariPhrase's
// update, players' start and settings (v330) and its note-on's event lookup (v331), the band players' renders (v332),
// set-ups and seek (v333), SOLO's render, set-ups and seek (v334) and the period-jump player's (v335), VariPhrase's
// players all, and the analog's 19 renders (v337): rom.4's own run, and the routines they call hooked, each by its
// address; the check's own renders, which a render word drawn names
const OscHook kHooks[] = {
    { kOscRendersAt, kRenderBy, renderBy, rendered }, { kOscRendersAt + 0x10, kRenderBy, renderBy, rendered },
};

} // namespace

uint32_t oscVoiceAt(int voice) { return kOscVoiceAt + kOscVoiceStride * (uint32_t)voice; }
uint32_t oscRenderAt(int k) { return kOscRendersAt + 0x10 * (uint32_t)k; }

int oscVoiceNumber(uint32_t at) {
    const uint32_t from = at - kOscVoiceAt;
    const bool one = from % kOscVoiceStride == 0 && from / kOscVoiceStride < (uint32_t)kOscVoices;
    return one ? (int)(from / kOscVoiceStride) : -1;
}

void oscInstallRom(Vc2Fw& fw, const OscDraws& draws, std::vector<OscCall>& log, int (*voiceOf)(uint32_t)) {
    for (const OscHook& h : kHooks) {
        fw.cpu.hook(h.at, [&draws, &log, h, voiceOf](Sh4& cpu) {
            OscCall call { h.kind, -1, 0, 0, 0 };
            call.voice = voiceOf(h.decode(cpu, call, h.at));
            log.push_back(call);
            cpu.ret(h.answer(cpu, draws));
        });
    }
    fw.cpu.hook(kOsClock, [&draws](Sh4& cpu) { cpu.ret((uint32_t)draws.clock); });
    // v327: a page where mode 1's jump to a loop's cached event (0x893ca1a0) writes the event's number, through the
    // render's mask 0xfffff in r4 (0x0010000b: on the XT a half word to an odd address, an address error), so that the
    // rest of the jump runs; nothing reads it
    constexpr uint32_t kStrayAt = 0x80100000, kStrayBytes = 0x1000;
    if (fw.cpu.ptr(kStrayAt, 1) == nullptr) fw.cpu.mapRam(kStrayAt, kStrayBytes);
}

std::string oscCallsDifference(const std::vector<OscCall>& rom, const std::vector<OscCall>& port) {
    return vc2CallsDifference(rom, port, [](const OscCall& c) {
        char t[112];
        std::snprintf(t, sizeof t, "%d on voice %d (%08x %08x %08x)", c.kind, c.voice, c.a, c.b, c.c);
        return std::string(t);
    });
}
#endif
