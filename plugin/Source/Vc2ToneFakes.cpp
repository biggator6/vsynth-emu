// xt_vc2tone's rom.4 side: see Vc2ToneCheck.h (v228, v229).  Every part routine the tone calls hooked, each row of the
// table with its own reading of the arguments and its answer; a tone's part words to and from rom.4's memory.  The port's
// fake parts are Vc2TonePortParts.cpp.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"
#include "Vc2CheckCalls.h"
#include <algorithm>
#include <utility>

namespace {
using Piece = Vc2TonePiece;
constexpr int kPieceStride = 100;   // a part's code: its piece × this + its number
constexpr uint32_t kBufferStride = 0x100, kNoBuffer = 0xff, kNoNumber = 0xff, kNeither = 9;
// The hooked routines the tones' words point at, and COSM 1's vtable, in Vc2Fw's RAM
constexpr uint32_t kCosmUpdateAt = kToneHooksAt, kCosmDspAt = kToneHooksAt + 0x10;
constexpr uint32_t kCosmVtableAt = kToneHooksAt + 0x100;
constexpr uint32_t kVtableEntry = 0xc;   // [FW] 0x893b4a06: COSM 1's update is its vtable's +0xc

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

// [FW] the part words' offsets: an OSC voice's +0x54, +0xa40, +0x1cc, +0xa80, byte +0x58, +0x104, its bindings from
// +0xa4c; the TVA's +0x40, bindings from +0xa4, serial +0xc0; a COSM's DSP routine (+0x148) and vtable (+0x160); the
// player a legato takes (+0x15c); a COSM's DSP arguments (+0x58, +0x10)
constexpr uint32_t kOscKind = 0x54, kOscFlag = 0xa40, kOscValue = 0x1cc, kOscOther = 0xa80, kOscByte = 0x58;
constexpr uint32_t kOscWave = 0x104, kOscBinds = 0xa4c, kTvaLevel = 0x40, kTvaBinds = 0xa4, kTvaSerial = 0xc0;
constexpr uint32_t kCosmDsp = 0x148, kCosmVtable = 0x160, kPlayer = 0x15c, kStateWord = 0xc;
constexpr uint32_t kCosmD = 0x58, kCosmState = 0x10, kSetUpOther = 4, kSetUpByte = 0xc;

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

uint32_t numberAt(uint32_t address) {   // an envelope's or an LFO's number from its address
    const int32_t part = partAt(address).second;
    return part >= kPieceStride * (int)Piece::envelopes ? (uint32_t)(part % kPieceStride) : kNoNumber;
}

// rom.4's scratch buffers and the output by number; an input block by its offset in the input
uint32_t buf(uint32_t at) {
    if (at == kToneOutAt) return kToneOut;
    const uint32_t from = at - kToneBufferAt;
    return from % kBufferStride == 0 && from / kBufferStride < 3 ? 1 + from / kBufferStride : kNoBuffer;
}
uint32_t blk(uint32_t at) { return at - kToneInputAt; }
uint32_t word(Sh4& cpu, uint32_t at, uint32_t bytes = sizeof(uint32_t)) {
    uint32_t v = 0;
    cpu.read(at, &v, bytes);
    return v;
}

// How a hooked routine's arguments read into its call, one shape each
void none(Sh4&, ToneCall&) {}
void one(Sh4&, ToneCall& t) { t.a = 1; }
void r5(Sh4& c, ToneCall& t) { t.a = c.r[5]; }
void fr4(Sh4& c, ToneCall& t) { t.a = c.fb[0][4]; }
void block5(Sh4& c, ToneCall& t) { t.a = blk(c.r[5]); }
void blockR6(Sh4& c, ToneCall& t) {
    t.a = blk(c.r[5]);
    t.b = c.r[6];
}
void state(Sh4& c, ToneCall& t) { t.a = word(c, c.r[4] + kStateWord); }   // the tone's, as the end routine finds it
void buf5(Sh4& c, ToneCall& t) { t.a = buf(c.r[5]); }
void bufs56(Sh4& c, ToneCall& t) { t.a = buf(c.r[5]) | buf(c.r[6]) << 8; }
void cosmDsp(Sh4& c, ToneCall& t) {   // given its +0x58, its +0x10 and itself: anything else in the second word
    t.a = buf(c.r[7]);
    t.b = c.r[4] == c.r[6] + kCosmD && c.r[5] == c.r[6] + kCosmState ? 0 : 1;
}
void bindCosm(Sh4& c, ToneCall& t) {   // its envelopes' and LFOs' numbers, the last LFO on the stack
    const uint32_t last = word(c, c.r[15]);
    t.a = numberAt(c.r[5]) | numberAt(c.r[6]) << 8 | numberAt(c.r[7]) << 16 | numberAt(last) << 24;
}
void player(Sh4& c, ToneCall& t) {   // the tone a legato takes from, and which of its OSC voices' players
    const auto [tone, osc] = partAt(c.r[5] - kPlayer);
    t.a = (uint32_t)tone;
    t.b = osc == tonePartCode(Piece::osc1, 0) ? 0 : (osc == tonePartCode(Piece::osc2, 0) ? 1 : kNeither);
}
void setUp(Sh4& c, ToneCall& t) {   // the struct's two floats and its byte, and the flag
    t.a = word(c, c.r[5]);
    t.b = word(c, c.r[5] + kSetUpOther);
    t.c = word(c, c.r[5] + kSetUpByte, 1);
    t.d = c.r[6];
}

using Answer = uint32_t (*)(const Sh4&, const ToneDraws&, int32_t part);
size_t which(int32_t part) { return (size_t)(part / kPieceStride % 2); }   // OSC 1, 2; COSM 1, 2
uint32_t zero(const Sh4&, const ToneDraws&, int32_t) { return 0; }
uint32_t over(const Sh4&, const ToneDraws& d, int32_t) { return d.tvaOver ? 1 : 0; }
uint32_t stopped(const Sh4&, const ToneDraws& d, int32_t) { return d.tvaStopped ? 1 : 0; }
uint32_t held(const Sh4&, const ToneDraws& d, int32_t) { return d.tvaHeld ? 1 : 0; }
uint32_t installedOsc(const Sh4&, const ToneDraws& d, int32_t p) { return d.installOsc[which(p)]; }
uint32_t installedCosm(const Sh4&, const ToneDraws& d, int32_t p) { return d.installCosm[which(p)]; }

// A hooked routine: where, what it logs as, which register holds its part (0 none), how its arguments read, its answer
struct ToneHook {
    uint32_t at;
    ToneKind kind;
    uint8_t partIn;
    void (*decode)(Sh4&, ToneCall&);
    Answer answer;
};
// [FW] every routine the tone's frame and note-on call but the envelopes', the LFOs' (v268), the mix's (v269) and the free
// LFOs' (v271), which run
const ToneHook kHooks[] = {
    { 0x893ae070, kBuild, 4, none, zero }, { 0x893aa030, kBuild, 4, none, zero }, { 0x893b20b0, kBuild, 4, none, zero },
    { 0x893aa060, kDestroy, 4, none, zero }, { 0x893ada30, kUnlist, 4, one, zero },
    { 0x893ad9a0, kUnlist, 4, none, zero }, { 0x893ae1c0, kUpdateOsc, 4, r5, zero },
    { kCosmUpdateAt, kUpdateCosm, 4, fr4, zero }, { 0x893b2560, kTvaOver, 4, none, over },
    { 0x893b2610, kTvaStopped, 4, none, stopped }, { 0x893b23d0, kUpdateTva, 4, none, zero },
    { kToneEndedAt, kEnded, 4, state, zero }, { 0x893ae9f0, kRenderOsc, 4, buf5, zero },
    { kCosmDspAt, kRenderCosm, 6, cosmDsp, zero },
    { 0x893b2640, kRenderTva, 4, bufs56, zero }, { 0x893ae6c0, kCountOsc, 4, r5, zero },
    { 0x893ae720, kCountOscMatched, 4, r5, zero }, { 0x893b25c0, kTvaHeld, 4, none, held },
    { 0x893ae0f0, kInstallOsc, 4, none, installedOsc },
    { 0x893aafe0, kInstallCosm, 4, r5, installedCosm }, { 0x893aa7b0, kBindCosm, 4, bindCosm, zero },
    { 0x893ae370, kNoteOnOsc, 4, blockR6, zero }, { 0x893aee20, kTakePlayer, 4, player, zero },
    { 0x893ae610, kSetOsc, 4, block5, zero },
    { 0x893aa590, kSetUpCosm, 4, setUp, zero }, { 0x893aa0e0, kStartCosm, 4, blockR6, zero },
    { 0x893b2100, kNoteOnTva, 4, blockR6, zero }, { 0x893ada10, kEnlist, 4, none, zero },
};
constexpr uint32_t kLocks[] = { 0x893018b0, 0x893018c0 };   // [FW] the OS's lock and unlock: nothing here

// Each of a tone's part words with its offset and size, given to `fn(offset, word, bytes)`
template <class Words, class Fn>
void eachWord(Words& w, Fn fn) {
    for (size_t o = 0; o < w.osc.size(); ++o) {
        const uint32_t at = tonePartOf(o == 0 ? Piece::osc1 : Piece::osc2).offset;
        auto& v = w.osc[o];
        for (auto [off, x] : { std::pair { kOscKind, &v.kind }, { kOscFlag, &v.flag }, { kOscValue, &v.value },
                               { kOscOther, &v.other }, { kOscWave, &v.wave } }) {
            fn(at + off, *x, 4u);
        }
        fn(at + kOscByte, v.byte, 1u);
        for (size_t k = 0; k < v.binds.size(); ++k) fn(at + kOscBinds + 4 * (uint32_t)k, v.binds[k], 4u);
    }
    const uint32_t tva = tonePartOf(Piece::tva).offset;
    fn(tva + kTvaLevel, w.tvaLevel, 4u);
    for (size_t k = 0; k < 3; ++k) fn(tva + kTvaBinds + 4 * (uint32_t)k, w.tvaBinds[k], 4u);
    fn(tva + kTvaSerial, w.tvaBinds[3], 4u);
}
} // namespace

int32_t tonePartCode(Vc2TonePiece piece, int index) { return (int32_t)piece * kPieceStride + index; }

uint32_t toneEnvelopeAt(uint32_t tone, int e) {
    return tone + tonePartOf(Piece::envelopes).offset + 0x70 * (uint32_t)e;
}

uint32_t toneLfoAt(uint32_t tone, int l) { return tone + tonePartOf(Piece::lfos).offset + 0x98 * (uint32_t)l; }

void toneInstallRom(Vc2Fw& fw, const ToneDraws& draws, std::vector<ToneCall>& log) {
    for (const ToneHook& h : kHooks) {
        fw.cpu.hook(h.at, [&draws, &log, h](Sh4& cpu) {
            const auto [tone, part] = h.partIn == 0 ? std::pair { kToneNone, kToneNone } : partAt(cpu.r[h.partIn]);
            ToneCall call { h.kind, tone, part, 0, 0, 0, 0 };
            h.decode(cpu, call);
            log.push_back(call);
            cpu.ret(h.answer(cpu, draws, part));
        });
    }
    for (uint32_t at : kLocks) fw.cpu.hook(at, [](Sh4& cpu) { cpu.ret(0); });
    const uint32_t entry[3] = { 0, 0, kCosmUpdateAt };   // the this-delta, a word, the method
    fw.cpu.write(kCosmVtableAt + kVtableEntry, entry, sizeof entry);
}

void toneWriteRom(Vc2Fw& fw, uint32_t address, const ToneWords& w) {
    eachWord(w, [&](uint32_t off, const uint32_t& x, uint32_t bytes) { fw.cpu.write(address + off, &x, bytes); });
    for (Piece cosm : { Piece::cosm1, Piece::cosm2 }) {
        const uint32_t at = address + tonePartOf(cosm).offset;
        fw.cpu.write(at + kCosmVtable, &kCosmVtableAt, sizeof kCosmVtableAt);
        fw.cpu.write(at + kCosmDsp, &kCosmDspAt, sizeof kCosmDspAt);
    }
}

ToneWords toneReadRom(Vc2Fw& fw, uint32_t address) {
    ToneWords w;
    eachWord(w, [&](uint32_t off, uint32_t& x, uint32_t bytes) { x = word(fw.cpu, address + off, bytes); });
    return w;
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
