// xt_vc2tone's rom.4 side: see Vc2ToneCheck.h (v228, v229).  What the tone calls that is not its parts' hooked, each row
// of the table with its own reading of the arguments: the tail's end and truncation routines, the truncation's wait and
// the OS's event (v317).  From v319 every part runs, rom.4's own, and what the OSC voices call beyond the tone is
// xt_vc2osc's table (Vc2OscFakes.cpp), each call on its voice, tone × 2 + its OSC.  The port's side is
// Vc2TonePortParts.cpp.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"
#include "Vc2CheckCalls.h"
#include <algorithm>
#include <utility>

namespace {
using Piece = Vc2TonePiece;
constexpr int kPieceStride = 100;   // a part's code: its piece × this + its number

// [FW] a tone's parts by offset: a single part, or an array of `count` at a stride
struct TonePartAt {
    uint32_t offset;
    Piece piece;
    uint32_t stride;
    uint32_t count;
};
constexpr TonePartAt kParts[] = {
    { 0x20, Piece::osc1, 0, 1 }, { 0xba4, Piece::osc2, 0, 1 }, { 0x1728, Piece::cosm1, 0, 1 },
    { 0x188c, Piece::cosm2, 0, 1 }, { 0x19f0, Piece::tva, 0, 1 }, { 0x1ab8, Piece::mix, 0, 1 },
    { 0x1b38, Piece::envelopes, 0x70, Vc2Tone::kEnvelopes }, { 0x2158, Piece::lfos, 0x98, Vc2Tone::kLfos },
};
const TonePartAt& tonePartOf(Piece piece) { return *std::ranges::find(kParts, piece, &TonePartAt::piece); }

constexpr uint32_t kTvaFade = 0x64;        // [FW] the TVA's fade, which the truncation's waits end
constexpr uint32_t kWaitAt = 0x893017f0;   // [FW] the OS's wait for an event, the truncation's
constexpr uint32_t kStateWord = 0xc;

// A tone's part at an address in rom.4's pool, as a call names it, and the tone's number
std::pair<int, int32_t> partAt(uint32_t address) {
    const uint32_t from = address - Vc2TonePool::kBase, offset = from % Vc2Tone::kSize;
    const int tone = (int)(from / Vc2Tone::kSize);
    if (offset == 0) return { tone, kToneNone };
    for (const TonePartAt& p : kParts) {
        const uint32_t span = p.stride == 0 ? 1 : p.stride * p.count, at = offset - p.offset;
        if (offset < p.offset || at >= span || (p.stride != 0 && at % p.stride != 0)) continue;
        return { tone, tonePartCode(p.piece, p.stride == 0 ? 0 : (int)(at / p.stride)) };
    }
    return { tone, kToneUnknown };
}

uint32_t word(Sh4& cpu, uint32_t at) {
    uint32_t v = 0;
    cpu.read(at, &v, sizeof v);
    return v;
}

// How a hooked routine's arguments read into its call, one shape each
void none(Sh4&, ToneCall&) {}
void state(Sh4& c, ToneCall& t) { t.a = word(c, c.r[4] + kStateWord); }   // the tone's, as the end routine finds it
void osArgs(Sh4& c, ToneCall& t) {   // the OS's event and flags (v317)
    t.a = c.r[4];
    t.b = c.r[5];
}

// A hooked routine: where, what it logs as, which register holds its part (0 none), how its arguments read; each
// answers 0
struct ToneHook {
    uint32_t at;
    ToneKind kind;
    uint8_t partIn;
    void (*decode)(Sh4&, ToneCall&);
};
// [FW] what the tone's frame, note-on and remaining methods call beyond its parts (every part rom.4's own from v319):
// the tail's two routines, and the OS's event, which the TVA sets (v317)
const ToneHook kHooks[] = {
    { kToneEndedAt, kEnded, 4, state },
    { kToneTruncatedAt, kTruncated, 4, none },
    { 0x89301830, kOsEvent, 0, osArgs },
};
constexpr uint32_t kLocks[] = { 0x893018b0, 0x893018c0 };   // [FW] the OS's lock and unlock: nothing here

// The wait a truncation makes: logged by its event, the tone's fade at 0 once the draws' count of waits has passed
void toneHookWait(Vc2Fw& fw, std::vector<ToneCall>& log, ToneFadeWait& fade) {
    fw.cpu.hook(kWaitAt, [&log, &fade](Sh4& cpu) {
        log.push_back({ kWaitFade, kToneNone, kToneNone, cpu.r[4], 0, 0, 0 });
        if (--fade.romLeft <= 0) {
            const float done = 0.0f;
            cpu.write(toneTvaAt(fade.tone) + kTvaFade, &done, sizeof done);
        }
        cpu.ret(0);
    });
}
} // namespace

int32_t tonePartCode(Vc2TonePiece piece, int index) { return (int32_t)piece * kPieceStride + index; }

uint32_t toneEnvelopeAt(uint32_t tone, int e) {
    return tone + tonePartOf(Piece::envelopes).offset + 0x70 * (uint32_t)e;
}

uint32_t toneLfoAt(uint32_t tone, int l) { return tone + tonePartOf(Piece::lfos).offset + 0x98 * (uint32_t)l; }
uint32_t toneTvaAt(uint32_t tone) { return tone + tonePartOf(Piece::tva).offset; }

uint32_t toneCosmAt(uint32_t tone, int cosm) {
    return tone + tonePartOf(cosm == 0 ? Piece::cosm1 : Piece::cosm2).offset;
}

uint32_t toneOscAt(uint32_t tone, int osc) { return tone + tonePartOf(osc == 0 ? Piece::osc1 : Piece::osc2).offset; }

int toneVoiceNumber(uint32_t at) {
    const auto [tone, part] = partAt(at);
    const bool inPool = at - Vc2TonePool::kBase < Vc2TonePool::kTones * Vc2Tone::kSize;
    if (!inPool || (part != tonePartCode(Piece::osc1, 0) && part != tonePartCode(Piece::osc2, 0))) return -1;
    return 2 * tone + (part == tonePartCode(Piece::osc2, 0) ? 1 : 0);
}

void toneInstallRom(Vc2Fw& fw, std::vector<ToneCall>& log, ToneFadeWait& fade, const OscDraws& oscDraws,
                    std::vector<OscCall>& oscLog) {
    toneHookWait(fw, log, fade);
    for (const ToneHook& h : kHooks) {
        fw.cpu.hook(h.at, [&log, h](Sh4& cpu) {
            const auto [tone, part] = h.partIn == 0 ? std::pair { kToneNone, kToneNone } : partAt(cpu.r[h.partIn]);
            ToneCall call { h.kind, tone, part, 0, 0, 0, 0 };
            h.decode(cpu, call);
            log.push_back(call);
            cpu.ret(0);
        });
    }
    for (uint32_t at : kLocks) fw.cpu.hook(at, [](Sh4& cpu) { cpu.ret(0); });
    oscInstallRom(fw, oscDraws, oscLog, toneVoiceNumber);
}

std::string toneCallsDifference(const std::vector<ToneCall>& rom, const std::vector<ToneCall>& port) {
    return vc2CallsDifference(rom, port, [](const ToneCall& c) {
        char t[128];
        std::snprintf(t, sizeof t, "%d on tone %d part %d (%08x %08x %08x %08x)", c.kind, c.tone, c.part, c.a, c.b,
                      c.c, c.d);
        return std::string(t);
    });
}
#endif
