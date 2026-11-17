// xt_vc2osc's rom.4 side: see Vc2OscCheck.h (v230, v232).  Every routine the OSC voice calls hooked, each row of the
// table with its own reading of the arguments and its answer; the records the voice reads written where its words
// point.  The port's fake parts are Vc2OscPortParts.cpp.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2OscCheck.h"
#include <algorithm>

namespace {
// [FW] the voice's offsets a callee is given: the carrier, kind 6's object (the ramps, its own from v236, aren't hooked)
constexpr uint32_t kCarrier = 0xa90, kKind6 = 0xafc;

// The voice owning the part at `offset` a callee is given by `at`; −1 when no voice does
int voiceOwning(uint32_t at, uint32_t offset) {
    const uint32_t from = at - offset - kOscVoiceAt;
    const bool one = from % kOscVoiceStride == 0 && from / kOscVoiceStride < (uint32_t)kOscVoices;
    return one ? (int)(from / kOscVoiceStride) : -1;
}
uint32_t asIs(uint32_t x) { return x; }
uint32_t settingsOffset(uint32_t at) { return at - kOscSettingsAt; }
uint32_t outOf(uint32_t at) { return at == kOscOutAt ? 1 : kOscOther; }
uint8_t byteAt(Sh4& cpu, uint32_t at) {
    uint8_t b = 0;
    cpu.read(at, &b, 1);
    return b;
}

// How a hooked routine's arguments read into its call, one shape each: the voice's part at `Offset` in r4 and r5 read
void self(Sh4& c, OscCall& t, uint32_t) { t.voice = voiceOwning(c.r[4], 0); }
template <uint32_t Offset, uint32_t (*Read)(uint32_t)>
void partAnd(Sh4& c, OscCall& t, uint32_t) {
    t.voice = voiceOwning(c.r[4], Offset);
    t.a = Read(c.r[5]);
}
template <uint32_t Offset>
void partWord(Sh4& c, OscCall& t, uint32_t) {   // the voice's part at `Offset` and a word of the voice in r5
    t.voice = voiceOwning(c.r[4], Offset);
    t.a = c.r[5] - (c.r[4] - Offset);
}
void routine(Sh4& c, OscCall& t, uint32_t at) {
    self(c, t, at);
    t.a = at;
}
// A kind's routine, and the pitch ramp's value and factor as it finds them (v236: its update reads them through the
// targets, so the ramps' ticks before or after it show)
void kindRoutine(Sh4& c, OscCall& t, uint32_t at) {
    routine(c, t, at);
    t.b = c.rd32(c.r[4] + kOscRampValueAt);
    t.c = c.rd32(c.r[4] + kOscRampFactorAt);
}
void routineSettings(Sh4& c, OscCall& t, uint32_t at) {
    routine(c, t, at);
    t.b = settingsOffset(c.r[5]);
}
void renderBy(Sh4& c, OscCall& t, uint32_t at) {   // (out, the object, third)
    t.voice = voiceOwning(c.r[5], Vc2OscVoice::kObject);
    t.a = at;
    t.b = outOf(c.r[4]);
    t.c = c.r[6];
}
void seek(Sh4& c, OscCall& t, uint32_t at) {   // (the object, at), and the object's flags as the player finds them
    t.voice = voiceOwning(c.r[4], Vc2OscVoice::kObject);
    t.a = at;
    t.b = c.r[5];
    t.c = byteAt(c, c.r[4]);
}
void objectRoutine(Sh4& c, OscCall& t, uint32_t at) {   // a player's routine on the object (v238)
    t.voice = voiceOwning(c.r[4], Vc2OscVoice::kObject);
    t.a = at;
}
void eventAt(Sh4& c, OscCall& t, uint32_t at) {   // the event lookup's (the object, event, &position, &…) (v238)
    objectRoutine(c, t, at);
    t.b = c.r[5];
}

// How a hooked routine answers, one shape each: its r0, after what it leaves in fr0 or the object
uint32_t none(Sh4&, const OscDraws&) { return 0; }
uint32_t rendered(Sh4&, const OscDraws& d) { return d.rendered; }
uint32_t seekFlags(Sh4& c, const OscDraws& d) {   // the seek's object's byte 0, as a player may leave it
    c.write(c.r[4], &d.seekFlags, 1);
    return 0;
}
uint32_t positions(Sh4& c, const OscDraws& d) {   // the next of the drawn positions, and a word the caller drops
    const int32_t position = d.positions[d.romTaken++ % d.positions.size()];
    c.write(c.r[6], &position, sizeof position);
    c.write(c.r[7], &d.aside, sizeof d.aside);
    return 0;
}

// A hooked routine: where, what it logs as, how its arguments read, its answer
struct OscHook {
    uint32_t at;
    OscKind kind;
    void (*decode)(Sh4&, OscCall&, uint32_t);
    uint32_t (*answer)(Sh4&, const OscDraws&);
};
constexpr auto withN = partAnd<0, asIs>;
constexpr auto withSettings = partAnd<0, settingsOffset>;
// [FW] every routine the voice's frame and the kind's note-on call but the ramps' (v236: rom.4's own run) and LITE's
// and VariPhrase's installs and VariPhrase's note-on (v238: rom.4's own run, and the routines they call hooked)
const OscHook kHooks[] = {
    { 0x893b29b0, kBuildVibrato, self, none }, { 0x893b2c10, kUpdateVibrato, withN, none },
    { 0x893b2b70, kSetVibrato, withSettings, none }, { 0x893b29e0, kNoteOnVibrato, partWord<0>, none },
    { 0x893c2ad0, kKindCall, kindRoutine, none }, { 0x893c8280, kKindCall, kindRoutine, none },
    { 0x893c88b0, kKindCall, kindRoutine, none }, { 0x893c2ce0, kKindCall, kindRoutine, none },
    { 0x893afff0, kKindCall, kindRoutine, none }, { 0x893c6260, kKindCall, kindRoutine, none },
    { 0x893b0150, kKindCall, kindRoutine, none }, { 0x893c7c30, kKindCall, kindRoutine, none },
    { 0x893c22e0, kKindCall, kindRoutine, none }, { 0x893c8670, kKindCall, kindRoutine, none },
    { 0x893cc760, kObjectCall, objectRoutine, none }, { 0x893cc0d0, kEventPosition, eventAt, positions },
    { 0x893c8390, kKindSettings, routineSettings, none }, { 0x893b0280, kKindSettings, routineSettings, none },
    { 0x893c3460, kKindSettings, routineSettings, none },
    { 0x893ec450, kSetCarrier, partAnd<kCarrier, settingsOffset>, none },
    { 0x893ebed0, kInitCarrier, partWord<kCarrier>, none }, { 0x893ec700, kStartKind6, partWord<kKind6>, none },
    { 0x893c62e0, kRenderBy, renderBy, rendered }, { 0x893c61e0, kRenderBy, renderBy, rendered },
    // v238: the players' renders LITE's and VariPhrase's installs write (Vari off's, kind 1's mode 1, LITE's heads,
    // SOLO's, BACKING's, ENSEMBLE's, the period-jump player's)
    { 0x893c63c0, kRenderBy, renderBy, rendered }, { 0x893c66e0, kRenderBy, renderBy, rendered },
    { 0x893c9f60, kRenderBy, renderBy, rendered }, { 0x893c6e80, kRenderBy, renderBy, rendered },
    { 0x893c6e8c, kRenderBy, renderBy, rendered }, { 0x893c9450, kRenderBy, renderBy, rendered },
    { 0x893cadc0, kRenderBy, renderBy, rendered }, { 0x893ca1f0, kRenderBy, renderBy, rendered },
    { 0x893ca8b0, kRenderBy, renderBy, rendered },
    { kOscRendersAt, kRenderBy, renderBy, rendered }, { kOscRendersAt + 0x10, kRenderBy, renderBy, rendered },
    { 0x893ebfa0, kRenderCarrier, partAnd<kCarrier, outOf>, none },
    { 0x893ec730, kRenderKind6, partAnd<kKind6, outOf>, none },
    { 0x893c6360, kSeek, seek, seekFlags }, { 0x893cc830, kSeek, seek, seekFlags },
    { 0x893c6de0, kSeek, seek, seekFlags }, { 0x893cc630, kSeek, seek, seekFlags },
    { 0x893cd090, kSeek, seek, seekFlags }, { 0x893cd440, kSeek, seek, seekFlags },
};

} // namespace

uint32_t oscVoiceAt(int voice) { return kOscVoiceAt + kOscVoiceStride * (uint32_t)voice; }
uint32_t oscRenderAt(int k) { return kOscRendersAt + 0x10 * (uint32_t)k; }

void oscInstallRom(Vc2Fw& fw, const OscDraws& draws, std::vector<OscCall>& log) {
    for (const OscHook& h : kHooks) {
        fw.cpu.hook(h.at, [&draws, &log, h](Sh4& cpu) {
            OscCall call { h.kind, -1, 0, 0, 0 };
            h.decode(cpu, call, h.at);
            log.push_back(call);
            cpu.ret(h.answer(cpu, draws));
        });
    }
}

std::string oscCallsDifference(const std::vector<OscCall>& rom, const std::vector<OscCall>& port) {
    return vc2CallsDifference(rom, port, [](const OscCall& c) {
        char t[112];
        std::snprintf(t, sizeof t, "%d on voice %d (%08x %08x %08x)", c.kind, c.voice, c.a, c.b, c.c);
        return std::string(t);
    });
}
#endif
