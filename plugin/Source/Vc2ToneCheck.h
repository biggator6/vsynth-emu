#pragma once
// xt_vc2tone's shared parts (v228): the calls each side logs, what the parts answer (drawn once for both sides), the
// port's fake parts, and rom.4's side: its tones' part words written from the draws and every part routine hooked to log
// and answer.  XT_VC2TONE_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2Tone.h"
#include <array>
#include <vector>

inline constexpr uint32_t kToneOutAt = 0x8C036000;     // the renders' output, in Vc2Fw's RAM
inline constexpr uint32_t kToneHooksAt = 0x8C042000;   // the hooked routines the tones' words point at, in Vc2Fw's RAM
inline constexpr uint32_t kToneEndedAt = kToneHooksAt + 0x20;   // the end routine, the tail's word on both sides
inline constexpr uint32_t kToneBufferAt = 0x89998b20;  // [FW] the renders' three scratch buffers, 0x100 bytes apart
inline constexpr int kToneNone = -1, kToneUnknown = -2;   // a call's part: the tone itself, or no part known

enum ToneKind : int32_t {
    kBuild, kDestroy, kUnfollow, kUnlist, kTickEnvelope, kTickLfo, kClockLfo, kUpdateOsc, kUpdateMix, kUpdateCosm,
    kTvaOver, kTvaStopped, kUpdateTva, kEnded, kRenderOsc, kRenderMix, kRenderCosm, kRenderTva, kCountOsc,
    kCountOscMatched, kTvaHeld, kInstallOsc, kInstallMix, kInstallCosm,
};

// A call as either side made it: what, on which tone of the pool and which of its parts (Vc2TonePiece × 100 + an
// envelope's or LFO's number), and its arguments: a count, a float's bits, the buffers by number (a 1, b 2, c 3, out 4)
struct ToneCall {
    int32_t kind, tone, part;
    uint32_t a, b;
    bool operator==(const ToneCall&) const = default;
};

// What the parts answer and hold for a call, drawn once for both sides
struct ToneDraws {
    std::array<bool, Vc2Tone::kEnvelopes> envelopeOn {};
    std::array<bool, Vc2Tone::kLfos> lfoOn {}, lfoFollows {};
    float oscValue = 0.0f, tvaLevel = 0.0f;
    int32_t oscKind = 0;
    uint32_t oscFlag = 0;
    bool tvaOver = false, tvaStopped = false, tvaHeld = false;
    std::array<uint32_t, 2> installOsc {}, installCosm {};
};

int32_t tonePartCode(Vc2TonePiece piece, int index);   // a part as a call names it
inline constexpr int kToneOut = 4;

// The port's parts: answers from the draws, the calls logged on the tone the check acts on
class TonePortParts final : public Vc2ToneParts {
public:
    TonePortParts(const ToneDraws& draws, const Vc2ToneBuffers& buffers, const float* out)
        : draws_(draws), buffers_(buffers), out_(out) {}
    void build(Vc2TonePiece piece) override { log(kBuild, tonePartCode(piece, 0)); }
    void destroy(Vc2TonePiece piece) override;
    bool lfoFollows(int lfo) override { return draws_.lfoFollows[(size_t)lfo]; }
    void unfollow(int lfo) override { log(kUnfollow, tonePartCode(Vc2TonePiece::lfos, lfo)); }
    void unlist(bool fromOne) override { log(kUnlist, kToneNone, fromOne ? 1 : 0); }
    bool envelopeOn(int envelope) override { return draws_.envelopeOn[(size_t)envelope]; }
    void tickEnvelope(int envelope, int32_t n) override;
    bool lfoOn(int lfo) override { return draws_.lfoOn[(size_t)lfo]; }
    void tickLfo(int lfo, int32_t n) override;
    void clockLfo(int lfo, float x) override;
    void updateOsc(int osc, int32_t n) override;
    void updateMix() override { log(kUpdateMix, tonePartCode(Vc2TonePiece::mix, 0)); }
    float oscValue(int) override { return draws_.oscValue; }
    void updateCosm(int cosm, float x) override;
    bool tvaOver() override { return answered(kTvaOver, draws_.tvaOver); }
    bool tvaStopped() override { return answered(kTvaStopped, draws_.tvaStopped); }
    void updateTva() override { log(kUpdateTva, tonePartCode(Vc2TonePiece::tva, 0)); }
    void ended() override;   // logged with the tone's state as the end routine finds it
    void renderOsc(int osc, float* buffer) override;
    void renderMix(const float* a, const float* b, float* c) override;
    void renderCosm(int cosm, float* buffer) override;
    void renderTva(const float* in, float* out) override;
    void countOsc(int osc, uint32_t x) override;
    void countOscMatched(int osc, uint32_t x) override;
    float tvaLevel() override { return draws_.tvaLevel; }
    bool tvaHeld() override { return answered(kTvaHeld, draws_.tvaHeld); }
    uint32_t installOsc(int osc) override;
    int32_t oscKind(int) override { return draws_.oscKind; }
    uint32_t oscFlag(int) override { return draws_.oscFlag; }
    void installMix(bool kind3) override;
    uint32_t installCosm(int cosm, uint32_t flag) override;

    std::vector<ToneCall> calls;
    int tone = 0;                          // the pool's tone the check acts on
    const Vc2TonePool* pool = nullptr;     // whose tones the end routine is given

private:
    void log(ToneKind kind, int32_t part, uint32_t a = 0, uint32_t b = 0) {
        calls.push_back({ kind, tone, part, a, b });
    }
    bool answered(ToneKind kind, bool answer);
    uint32_t bufferOf(const float* p) const;
    const ToneDraws& draws_;
    const Vc2ToneBuffers& buffers_;
    const float* out_;
};

// [FW] a tone's own words the check writes on both sides, by offset: its state, the tail's end routine and render flag
inline constexpr uint32_t kToneStateAt = 0xc, kToneEndedWordAt = 0x1b14, kToneRendersAt = 0x1b34;

struct ToneCounts {
    long events = 0, makes = 0, full = 0, inPlace = 0, unmakes = 0, back = 0, updates = 0, ends = 0, renders = 0;
};

// Both sides: rom.4 with its calls' log, the port's pool and parts, the draws, the buffers, the tones made
struct ToneSides {
    Vc2Fw& fw;
    Vc2TonePool& pool;
    TonePortParts& parts;
    ToneDraws& draws;
    Vc2ToneBuffers& buffers;
    std::vector<ToneCall>& rom;
    float* out;   // the port's output, the parts' too
    std::vector<int> live {};
    ToneCounts n {};
};

// The pool's words, the tones' own (every tone's, or tone `only`'s), then the calls; empty when nothing differs
std::string toneDifference(ToneSides& s, int only);
// Before an event on tone i: the parts' answers drawn and written to its parts, its number to the port's parts, the
// logs fresh
void tonePrepare(ToneSides& s, int i, std::mt19937& rng);
// A word of tone i's own set on both sides: `mine`, the port's, and rom.4's at the tone's `offset`
template <class Word>
void toneBothWord(ToneSides& s, int i, uint32_t offset, Word& mine, Word value) {
    mine = value;
    s.fw.cpu.write(s.pool.addressOf(i) + offset, &value, sizeof value);
}

// rom.4's side: every part routine hooked to log into `log` and answer from `draws`; the tone at `address`'s part words
// written from the draws (each part's flags and values, COSM 1's vtable and the DSP routines at hooked addresses)
void toneInstallRom(Vc2Fw& fw, const ToneDraws& draws, std::vector<ToneCall>& log);
void toneWriteRom(Vc2Fw& fw, uint32_t address, const ToneDraws& draws);
// The first call the two logs differ at; empty when they don't
std::string toneCallsDifference(const std::vector<ToneCall>& rom, const std::vector<ToneCall>& port);
