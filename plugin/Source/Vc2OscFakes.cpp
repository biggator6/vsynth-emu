// xt_vc2osc's rom.4 side: see Vc2OscCheck.h (v230, v232).  Every routine the OSC voice calls hooked, each row of the
// table with its own reading of the arguments and its answer; the records the voice reads written where its words
// point.  The port's fake parts are Vc2OscPortParts.cpp.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2OscCheck.h"
#include <algorithm>

namespace {
// [FW] the voice's offsets a callee is given: its three ramps, the object, the carrier, kind 6's object
constexpr std::array<uint32_t, Vc2OscVoice::kRamps> kRampAt { 0x90c, 0x970, 0x9d4 };
constexpr uint32_t kCarrier = 0xa90, kKind6 = 0xafc, kNone = 0xffffffff;
// [FW] where a record's fields are (Vc2OscVoice.h's): the wave header's, the loop record's
constexpr uint32_t kHeaderFlag = 2, kHeaderRate = 8, kHeaderKey = 0xa, kHeaderCents = 0x18, kLoopFine = 0x1b;
constexpr uint32_t kLoopsFrom = 0x100, kEnvelopesFrom = 0x200, kLfosFrom = 0x400, kBoundStride = 0x80;

// The voice owning the part at `offset` a callee is given by `at`; −1 when no voice does
int voiceOwning(uint32_t at, uint32_t offset) {
    const uint32_t from = at - offset - kOscVoiceAt;
    const bool one = from % kOscVoiceStride == 0 && from / kOscVoiceStride < (uint32_t)kOscVoices;
    return one ? (int)(from / kOscVoiceStride) : -1;
}
uint32_t rampOf(uint32_t at) {   // which ramp of its voice, kNone when none
    const auto k = std::ranges::find(kRampAt, (at - kOscVoiceAt) % kOscVoiceStride);
    return k == kRampAt.end() ? kNone : (uint32_t)(k - kRampAt.begin());
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
void ramp(Sh4& c, OscCall& t, uint32_t) {
    const uint32_t r = rampOf(c.r[4]);
    t.voice = r == kNone ? -1 : voiceOwning(c.r[4], kRampAt[r]);
    t.a = r;
}
void rampN(Sh4& c, OscCall& t, uint32_t at) {
    ramp(c, t, at);
    t.b = c.r[5];
}
void rampFloats(Sh4& c, OscCall& t, uint32_t at) {
    ramp(c, t, at);
    t.b = c.fb[0][4];
    t.c = c.fb[0][5];
}
void routine(Sh4& c, OscCall& t, uint32_t at) {
    self(c, t, at);
    t.a = at;
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

// How a hooked routine answers, one shape each: its r0, after what it leaves in fr0 or the object
uint32_t none(Sh4&, const OscDraws&) { return 0; }
uint32_t rampAnswer(Sh4& c, const OscDraws& d) {
    c.fb[0][0] = std::bit_cast<uint32_t>(d.rampValue);
    return 0;
}
uint32_t rendered(Sh4&, const OscDraws& d) { return d.rendered; }
uint32_t seekFlags(Sh4& c, const OscDraws& d) {   // the seek's object's byte 0, as a player may leave it
    c.write(c.r[4], &d.seekFlags, 1);
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
// [FW] every routine the voice's frame and the kind's note-on call
const OscHook kHooks[] = {
    { 0x893b29b0, kBuildVibrato, self, none }, { 0x893b2c10, kUpdateVibrato, withN, none },
    { 0x893b2b70, kSetVibrato, withSettings, none }, { 0x893b29e0, kNoteOnVibrato, partWord<0>, none },
    { 0x893b0dc0, kBuildRamp, rampN, none }, { 0x893b0f30, kTickRamp, rampN, none },
    { 0x893b0e10, kStartRamp, rampFloats, none }, { 0x893b0f20, kSetRamp, rampFloats, none },
    { 0x893b0e30, kLegatoRamp, rampN, none }, { 0x893b1160, kRampValue, ramp, rampAnswer },
    { 0x893af7a0, kKindCall, routine, none }, { 0x893af870, kKindCall, routine, none },
    { 0x893c2ad0, kKindCall, routine, none }, { 0x893c8280, kKindCall, routine, none },
    { 0x893c88b0, kKindCall, routine, none }, { 0x893c2ce0, kKindCall, routine, none },
    { 0x893afff0, kKindCall, routine, none }, { 0x893c6260, kKindCall, routine, none },
    { 0x893b0150, kKindCall, routine, none }, { 0x893c7c30, kKindCall, routine, none },
    { 0x893af420, kKindCall, routine, none }, { 0x893c22e0, kKindCall, routine, none },
    { 0x893c8390, kKindSettings, routineSettings, none }, { 0x893b0280, kKindSettings, routineSettings, none },
    { 0x893c3460, kKindSettings, routineSettings, none },
    { 0x893ec450, kSetCarrier, partAnd<kCarrier, settingsOffset>, none },
    { 0x893ebed0, kInitCarrier, partWord<kCarrier>, none }, { 0x893ec700, kStartKind6, partWord<kKind6>, none },
    { 0x893c62e0, kRenderBy, renderBy, rendered }, { 0x893c61e0, kRenderBy, renderBy, rendered },
    { kOscRendersAt, kRenderBy, renderBy, rendered }, { kOscRendersAt + 0x10, kRenderBy, renderBy, rendered },
    { 0x893ebfa0, kRenderCarrier, partAnd<kCarrier, outOf>, none },
    { 0x893ec730, kRenderKind6, partAnd<kKind6, outOf>, none },
    { 0x893c6360, kSeek, seek, seekFlags }, { 0x893cc830, kSeek, seek, seekFlags },
    { 0x893c6de0, kSeek, seek, seekFlags }, { 0x893cc630, kSeek, seek, seekFlags },
    { 0x893cd090, kSeek, seek, seekFlags }, { 0x893cd440, kSeek, seek, seekFlags },
};

template <class T>
void put(Vc2Fw& fw, uint32_t at, T v) {
    fw.cpu.write(at, &v, sizeof v);
}
} // namespace

uint32_t oscVoiceAt(int voice) { return kOscVoiceAt + kOscVoiceStride * (uint32_t)voice; }
uint32_t oscRenderAt(int k) { return kOscRendersAt + 0x10 * (uint32_t)k; }
uint32_t oscHeaderAt(int k) { return kOscRecordsAt + kOscRecordStride * (uint32_t)k; }
uint32_t oscLoopAt(int k) { return kOscRecordsAt + kLoopsFrom + kOscRecordStride * (uint32_t)k; }
uint32_t oscEnvelopeAt(int k) { return kOscRecordsAt + kEnvelopesFrom + kBoundStride * (uint32_t)k; }
uint32_t oscLfoAt(int k) { return kOscRecordsAt + kLfosFrom + kBoundStride * (uint32_t)k; }

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

void oscWriteRecords(Vc2Fw& fw, const OscRecords& r) {
    for (int k = 0; k < kOscRecords; ++k) {
        const Vc2OscWaveHeader& h = r.headers[(size_t)k];
        put(fw, oscHeaderAt(k) + kHeaderFlag, h.flag);
        put(fw, oscHeaderAt(k) + kHeaderRate, h.rate);
        put(fw, oscHeaderAt(k) + kHeaderKey, h.rootKey);
        put(fw, oscHeaderAt(k) + kHeaderCents, h.cents);
        put(fw, oscLoopAt(k), r.loops[(size_t)k].tune);
        put(fw, oscLoopAt(k) + kLoopFine, r.loops[(size_t)k].fine);
    }
    for (const auto& [at, x] : r.bound) put(fw, at, x);
}

std::string oscCallsDifference(const std::vector<OscCall>& rom, const std::vector<OscCall>& port) {
    return vc2CallsDifference(rom, port, [](const OscCall& c) {
        char t[112];
        std::snprintf(t, sizeof t, "%d on voice %d (%08x %08x %08x)", c.kind, c.voice, c.a, c.b, c.c);
        return std::string(t);
    });
}
#endif
