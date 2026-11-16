// xt_vc2tone's fakes: see Vc2ToneCheck.h (v228).  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"
#include "Vc2CheckCalls.h"
#include <algorithm>
#include <utility>

namespace {
using Piece = Vc2TonePiece;
constexpr int kPieceStride = 100;   // a part's code: its piece × this + its number
constexpr uint32_t kBufferStride = 0x100;
constexpr uint32_t kNoBuffer = 0xff;
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
// [FW] the words each part's routines read: the envelopes' +0x68, the LFOs' +0x80 and +0x7c, OSC 1's +0x54, +0xa40 and
// +0x1cc, the TVA's +0x40, a COSM's DSP routine (+0x148) and vtable (+0x160)
constexpr uint32_t kEnvelopeOn = 0x68, kLfoOn = 0x80, kLfoFollows = 0x7c, kOscKind = 0x54, kOscFlag = 0xa40;
constexpr uint32_t kOscValue = 0x1cc, kTvaLevel = 0x40, kCosmDsp = 0x148, kCosmVtable = 0x160;

// How a hook reads a call's arguments: from r5 as it is, fr4's bits, buffers by number, a constant 1, or the state of the
// tone it is given (the end routine's)
enum ToneArg : uint8_t { kArgNone, kArgR5, kArgFr4, kArgBuf5, kArgBufs56, kArgBufs567, kArgBuf7, kArgOne, kArgState };
constexpr uint32_t kStateWord = 0xc;   // [FW] the tone's +0xc
enum ToneAnswer : uint8_t { kNoAnswer, kAnsOver, kAnsStopped, kAnsHeld, kAnsInstallOsc, kAnsInstallCosm };

// A hooked routine: where, what it logs as, which register holds its part, how its arguments read, what it answers
struct ToneHook {
    uint32_t at;
    ToneKind kind;
    uint8_t partIn;
    ToneArg arg;
    ToneAnswer answer;
};
// [FW] every routine the tone's frame calls
constexpr ToneHook kHooks[] = {
    { 0x893ae070, kBuild, 4, kArgNone, kNoAnswer }, { 0x893aa030, kBuild, 4, kArgNone, kNoAnswer },
    { 0x893b20b0, kBuild, 4, kArgNone, kNoAnswer }, { 0x893adbc0, kBuild, 4, kArgNone, kNoAnswer },
    { 0x8950773a, kBuild, 4, kArgNone, kNoAnswer }, { 0x89507814, kDestroy, 4, kArgNone, kNoAnswer },
    { 0x893aa060, kDestroy, 4, kArgNone, kNoAnswer }, { 0x893ad720, kUnfollow, 5, kArgNone, kNoAnswer },
    { 0x893ada30, kUnlist, 4, kArgOne, kNoAnswer }, { 0x893ad9a0, kUnlist, 4, kArgNone, kNoAnswer },
    { 0x893abe80, kTickEnvelope, 4, kArgR5, kNoAnswer }, { 0x893ad100, kTickLfo, 4, kArgR5, kNoAnswer },
    { 0x893ad080, kClockLfo, 4, kArgFr4, kNoAnswer }, { 0x893ae1c0, kUpdateOsc, 4, kArgR5, kNoAnswer },
    { 0x893ade40, kUpdateMix, 4, kArgNone, kNoAnswer }, { kCosmUpdateAt, kUpdateCosm, 4, kArgFr4, kNoAnswer },
    { 0x893b2560, kTvaOver, 4, kArgNone, kAnsOver }, { 0x893b2610, kTvaStopped, 4, kArgNone, kAnsStopped },
    { 0x893b23d0, kUpdateTva, 4, kArgNone, kNoAnswer }, { kToneEndedAt, kEnded, 4, kArgState, kNoAnswer },
    { 0x893ae9f0, kRenderOsc, 4, kArgBuf5, kNoAnswer }, { 0x893ade50, kRenderMix, 4, kArgBufs567, kNoAnswer },
    { kCosmDspAt, kRenderCosm, 6, kArgBuf7, kNoAnswer }, { 0x893b2640, kRenderTva, 4, kArgBufs56, kNoAnswer },
    { 0x893ae6c0, kCountOsc, 4, kArgR5, kNoAnswer }, { 0x893ae720, kCountOscMatched, 4, kArgR5, kNoAnswer },
    { 0x893b25c0, kTvaHeld, 4, kArgNone, kAnsHeld }, { 0x893ae0f0, kInstallOsc, 4, kArgNone, kAnsInstallOsc },
    { 0x893ade90, kInstallMix, 4, kArgR5, kNoAnswer }, { 0x893aafe0, kInstallCosm, 4, kArgR5, kAnsInstallCosm },
};
constexpr uint32_t kLocks[] = { 0x893018b0, 0x893018c0 };   // [FW] the OS's lock and unlock: nothing here

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

// rom.4's scratch buffers and the output by number
uint32_t toneBuffer(uint32_t at) {
    if (at == kToneOutAt) return kToneOut;
    const uint32_t from = at - kToneBufferAt;
    return from % kBufferStride == 0 && from / kBufferStride < 3 ? 1 + from / kBufferStride : kNoBuffer;
}

uint32_t argOf(Sh4& cpu, ToneArg arg) {
    uint32_t state = 0;
    switch (arg) {
    case kArgNone: return 0;
    case kArgR5: return cpu.r[5];
    case kArgFr4: return cpu.fb[0][4];
    case kArgBuf5: return toneBuffer(cpu.r[5]);
    case kArgBufs56: return toneBuffer(cpu.r[5]) | toneBuffer(cpu.r[6]) << 8;
    case kArgBufs567: return toneBuffer(cpu.r[5]) | toneBuffer(cpu.r[6]) << 8 | toneBuffer(cpu.r[7]) << 16;
    case kArgBuf7: return toneBuffer(cpu.r[7]);
    case kArgOne: return 1;
    case kArgState:
        cpu.read(cpu.r[4] + kStateWord, &state, sizeof state);
        return state;
    }
    return 0;
}

uint32_t answerOf(const ToneDraws& d, ToneAnswer answer, int32_t part) {
    const size_t which = (size_t)(part % kPieceStride == 0 ? part / kPieceStride % 2 : 0);   // OSC 1, 2; COSM 1, 2
    switch (answer) {
    case kNoAnswer: return 0;
    case kAnsOver: return d.tvaOver ? 1 : 0;
    case kAnsStopped: return d.tvaStopped ? 1 : 0;
    case kAnsHeld: return d.tvaHeld ? 1 : 0;
    case kAnsInstallOsc: return d.installOsc[which];
    case kAnsInstallCosm: return d.installCosm[which];
    }
    return 0;
}

// A COSM's DSP routine is given its +0x58, its +0x10 and itself: anything else is logged in the call's second word
uint32_t cosmArgsOf(const Sh4& cpu) {
    constexpr uint32_t kD = 0x58, kState = 0x10;   // [FW] 0x893b4b74…0x893b4b80
    return cpu.r[4] == cpu.r[6] + kD && cpu.r[5] == cpu.r[6] + kState ? 0 : 1;
}
} // namespace

int32_t tonePartCode(Vc2TonePiece piece, int index) { return (int32_t)piece * kPieceStride + index; }

namespace {
int32_t oscCode(int osc) { return tonePartCode(static_cast<Piece>((int)Piece::osc1 + osc), 0); }
int32_t cosmCode(int cosm) { return tonePartCode(static_cast<Piece>((int)Piece::cosm1 + cosm), 0); }
const int32_t kMixCode = tonePartCode(Piece::mix, 0), kTvaCode = tonePartCode(Piece::tva, 0);
} // namespace

void TonePortParts::destroy(Vc2TonePiece piece) { log(kDestroy, tonePartCode(piece, 0)); }

void TonePortParts::ended() {
    log(kEnded, kToneNone, pool ? (uint32_t)pool->tones[(size_t)tone].head.state : 0);
}

void TonePortParts::tickEnvelope(int envelope, int32_t n) {
    log(kTickEnvelope, tonePartCode(Piece::envelopes, envelope), (uint32_t)n);
}

void TonePortParts::tickLfo(int lfo, int32_t n) { log(kTickLfo, tonePartCode(Piece::lfos, lfo), (uint32_t)n); }

void TonePortParts::clockLfo(int lfo, float x) {
    log(kClockLfo, tonePartCode(Piece::lfos, lfo), std::bit_cast<uint32_t>(x));
}

void TonePortParts::updateOsc(int osc, int32_t n) { log(kUpdateOsc, oscCode(osc), (uint32_t)n); }
void TonePortParts::updateCosm(int cosm, float x) { log(kUpdateCosm, cosmCode(cosm), std::bit_cast<uint32_t>(x)); }
void TonePortParts::renderOsc(int osc, float* buffer) { log(kRenderOsc, oscCode(osc), bufferOf(buffer)); }

void TonePortParts::renderMix(const float* a, const float* b, float* c) {
    log(kRenderMix, kMixCode, bufferOf(a) | bufferOf(b) << 8 | bufferOf(c) << 16);
}

void TonePortParts::renderCosm(int cosm, float* buffer) { log(kRenderCosm, cosmCode(cosm), bufferOf(buffer)); }

void TonePortParts::renderTva(const float* in, float* out) {
    log(kRenderTva, kTvaCode, bufferOf(in) | bufferOf(out) << 8);
}

void TonePortParts::countOsc(int osc, uint32_t x) { log(kCountOsc, oscCode(osc), x); }
void TonePortParts::countOscMatched(int osc, uint32_t x) { log(kCountOscMatched, oscCode(osc), x); }

uint32_t TonePortParts::installOsc(int osc) {
    log(kInstallOsc, oscCode(osc));
    return draws_.installOsc[(size_t)osc];
}

void TonePortParts::installMix(bool kind3) { log(kInstallMix, kMixCode, kind3 ? 1 : 0); }

uint32_t TonePortParts::installCosm(int cosm, uint32_t flag) {
    log(kInstallCosm, cosmCode(cosm), flag);
    return draws_.installCosm[(size_t)cosm];
}

bool TonePortParts::answered(ToneKind kind, bool answer) {
    log(kind, kTvaCode);
    return answer;
}

uint32_t TonePortParts::bufferOf(const float* p) const {
    const std::pair<const float*, uint32_t> buffers[] = {
        { buffers_.a, 1 }, { buffers_.b, 2 }, { buffers_.c, 3 }, { out_, kToneOut },
    };
    const auto* found = std::ranges::find(buffers, p, &std::pair<const float*, uint32_t>::first);
    return found == std::end(buffers) ? kNoBuffer : found->second;
}

void toneInstallRom(Vc2Fw& fw, const ToneDraws& draws, std::vector<ToneCall>& log) {
    for (const ToneHook& h : kHooks) {
        fw.cpu.hook(h.at, [&draws, &log, h](Sh4& cpu) {
            const auto [tone, part] = partAt(cpu.r[h.partIn]);
            const uint32_t b = h.kind == kRenderCosm ? cosmArgsOf(cpu) : 0;
            log.push_back({ h.kind, tone, part, argOf(cpu, h.arg), b });
            cpu.ret(answerOf(draws, h.answer, part));
        });
    }
    for (uint32_t at : kLocks) fw.cpu.hook(at, [](Sh4& cpu) { cpu.ret(0); });
    const uint32_t entry[3] = { 0, 0, kCosmUpdateAt };   // the this-delta, a word, the method
    fw.cpu.write(kCosmVtableAt + kVtableEntry, entry, sizeof entry);
}

void toneWriteRom(Vc2Fw& fw, uint32_t address, const ToneDraws& d) {
    const auto word = [&](uint32_t at, uint32_t v) { fw.cpu.write(at, &v, sizeof v); };
    const auto at = [&](Piece piece, uint32_t index) {
        const TonePartAt& p = *std::ranges::find(kParts, piece, &TonePartAt::piece);
        return address + p.offset + p.stride * index;
    };
    for (uint32_t e = 0; e < Vc2Tone::kEnvelopes; ++e) word(at(Piece::envelopes, e) + kEnvelopeOn, d.envelopeOn[e]);
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) {
        word(at(Piece::lfos, l) + kLfoOn, d.lfoOn[l]);
        word(at(Piece::lfos, l) + kLfoFollows, d.lfoFollows[l]);
    }
    word(at(Piece::osc1, 0) + kOscKind, (uint32_t)d.oscKind);
    word(at(Piece::osc1, 0) + kOscFlag, d.oscFlag);
    word(at(Piece::osc1, 0) + kOscValue, std::bit_cast<uint32_t>(d.oscValue));
    word(at(Piece::tva, 0) + kTvaLevel, std::bit_cast<uint32_t>(d.tvaLevel));
    word(at(Piece::cosm1, 0) + kCosmVtable, kCosmVtableAt);
    word(at(Piece::cosm1, 0) + kCosmDsp, kCosmDspAt);
}

std::string toneCallsDifference(const std::vector<ToneCall>& rom, const std::vector<ToneCall>& port) {
    return vc2CallsDifference(rom, port, [](const ToneCall& c) {
        char t[96];
        std::snprintf(t, sizeof t, "%d on tone %d part %d (%08x %08x)", c.kind, c.tone, c.part, c.a, c.b);
        return std::string(t);
    });
}
#endif
