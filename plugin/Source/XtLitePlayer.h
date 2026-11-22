#pragma once
// What plays a LITE wave (type 0, the voice's kind 1) with Vari Sw on: the firmware's mode 2 (rom.2 0x8950a7f0 mono, 0x8950a7fc
// stereo; note-on 0x8950b8d0, update 0x8950bc60, seek 0x8950a780; the knob update's 0x8950bcc0, blockUpdate).  A LITE wave has no analysis, so its time and pitch come apart
// by two read heads: the walk (the time) moves a position through the wave; each head reads from a point at or below it at the
// formant's rate under a triangular window whose phase runs at the pitch's, and when its clock passes the period (88200 over the
// root frequency, two of its cycles) it splices back to the last half period below the walk.  The two windows are half a period
// apart.  The heads' sum takes the gain, × 65536, and a one-pole filter takes out what follows the input slowly, its rate
// root × pitch × π/2 / 44100.  Bit for bit: plain float arithmetic under FE_TOWARDZERO, as the other players, FMAC a product then
// a sum.  HARDWARE_FIRMWARE.md, "LITE waves".
//
// Its state is the voice's player object (voice +0xd4, here "P"): +0xe8, +0xec the forward and backward Loop Mode; +0xf0 the
// samples; +0xf4, +0xf8 the heads; +0xfc the filter's rate, +0x100, +0x104 its state (L, R); +0x108, +0x10c the heads' fractions;
// +0x110 the walk's position; +0x114…+0x120 the windows' phases and clocks (A, then B); +0x124 the last sample; +0x128…+0x134 the
// forward loop (start, end, length, rate); +0x138 the first sample; +0x13c…+0x148 the backward loop (its end, start, length,
// rate); +0x14c the period, +0x150 its inverse.  The walk, pitch, formant and gain are +0x7c, +0x70, +0x74 and +0x84.
#include "XtBeat.h"
#include "XtLoop.h"
#include "XtWord.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// What a head reads, as an int16 (v326): rom.2's wave as the plugin holds it, floats ±1, past it 0; or the VC-2's wave
// memory as rom.4 reads it, halfwords from the address `base` (`count` of them): sample i of a channel at `first` + i ×
// `frame` bytes, the address wrapping as the SH-4's, past the memory 0
struct XtLiteSamples {
    const std::vector<float>* floats = nullptr;
    const int16_t* memory = nullptr;
    uint32_t base = 0, count = 0, first = 0, frame = 2;
    int32_t at(int32_t i) const;
    explicit operator bool() const { return floats != nullptr || memory != nullptr; }
};

class XtLitePlayer {
public:
    // Set before noteOn, as XtLinearPlayer's: the loop (the record's points, modes and rates), the Start Offset in samples, the
    // tempo and the beat's bits.
    void setLoop(const XtLoop& l) { loopCfg = l; }
    void setStart(int32_t samples) { startCfg = samples; }
    void setTempo(uint32_t tempo, uint32_t orig) { tempoCfg = tempo; origCfg = orig; }
    void setBeatFlags(bool sync, bool trip) {
        if (sync && !(flags0 & 2)) beat_.clk = 0;
        if (trip && !(flags0 & 4)) beat_.trip = 1;
        flags0 = (uint8_t)((sync ? 2 : 0) | (trip ? 4 : 0));
    }
    void addClock(int n) { beat_.clk = (int8_t)(beat_.clk + n); }
    void setTrip(int step) { beat_.trip = (int8_t)step; }
    bool syncOn() const { return flags0 & 2; }
    bool tripOn() const { return flags0 & 4; }
    const XtBeat& beat() const { return beat_; }

    // A wave's samples, ±1 (`right` empty for mono), which must outlive the note; the pitch, formant and walk as the note-on's
    // converters give them (0x894f3a00, 0x894f3b30, 0x894f3ac0), and the voice's root frequency (+0x9a4).
    void noteOn(const std::vector<float>& left, const std::vector<float>& right, float pitch, float formant, float walk, float root);
    // Each update (0x8950bc60): the pitch, the formant and the walk, and the filter's rate from the pitch.
    void setRates(float pitch, float formant, float walk, float root);
    // The three alone, the filter's rate left as it was: Playback Mode LEGATO puts a note's own back on an earlier note's player (0x894f2d00)
    void takeRates(float pitch_, float formant_, float walk_) { pitch = pitch_; formant = formant_; walk = walk_; }
    void setGainXt(float target, bool now = false) { gainTarget = target; if (now) gain = target; }
    float gainNow() const { return gain; }
    // 0x8950a780: the walk and both heads to a position in 1/16 sample, the windows back to their start, the beat again.
    void seek(int32_t pos16);
    // Each knob update, after the bits (0x8950bcc0, mode 2, at 0x8950bdda): the first and last samples again from the voice's own block
    // (+0xc4 the Start Offset and +0xc8 the frames, as its note started), the loops' points from the wave's loop record (`l`), and their
    // modes by the Loop bit as it stands (`l.on`).  Not the loops' rates.  A LEGATO copy brings the earlier note's player whole, so from
    // its next update it runs to this note's own start.
    void blockUpdate(const XtLoop& l, int32_t start, int32_t frames);
    // 32 frames, L/R pairs, in the XT's units (int16 × 65536 × the gain).
    void block(float* lr);
    // v326, the VC-2's heads (rom.4's 0x893c6e80 and 0x893c6e8c, Vc2OscHeads.cpp), which run this player on rom.4's
    // object's words: the block after the beat, given the walk step's word the beat left (+0x34); the heads' samples;
    // rom.4's backward reads, each sample as it is (rom.2's take it negated); and each word the block reads or writes,
    // its place in rom.2's object (the map above), its member here and its bytes
    void walkBlock(float* lr, int32_t word) {
        walkGiven = true;
        walkWord = word;
        block(lr);
        walkGiven = false;
    }
    void setSamples(const XtLiteSamples& left, const XtLiteSamples& right) {
        x = left;
        y = right;
    }
    void setUpright(bool on) { upright = on; }
    static std::span<const XtWord> words();

    // For the check: +0x38 (as the note-on left it), +0x110, +0xf4, +0xf8, +0x108, +0x10c, +0x100.
    int32_t position() const { return pos16; }
    float walkPosition() const { return walkPos; }
    int32_t headA() const { return hA; }
    int32_t headB() const { return hB; }
    float fractionA() const { return fracA; }
    float fractionB() const { return fracB; }
    float filterL() const { return dcL; }
    // +0x138, +0x124, +0xe8, +0xec: what each knob update sets again (blockUpdate)
    float firstSample() const { return firstF; }
    float lastSample() const { return lastF; }
    int fwdLoopMode() const { return fwdMode; }
    int bwdLoopMode() const { return bwdMode; }

private:
    XtLiteSamples x, y;
    bool upright = false, walkGiven = false;
    int32_t walkWord = 0;
    int32_t sample(const XtLiteSamples& v, int32_t i) const { return v.at(i); }
    float window(float& win, float& clk, int32_t& head, float at, float rate) const;
    template <bool Forward> void frame(float* lr, float at, float rate, int32_t end);
    void out(float* lr, float aL, float aR, float bL, float bR);

    float pitch = 1.0f, formant = 1.0f, walk = 1.0f, gainTarget = 0.0f, gain = 0.0f;
    float dcRate = 0.0f, dcL = 0.0f, dcR = 0.0f, fracA = 0.0f, fracB = 0.0f, walkPos = 0.0f;
    float winA = 0.0f, clkA = 0.0f, winB = 0.0f, clkB = 0.0f;
    float lastF = 0.0f, firstF = 0.0f, period = 1.0f, invPeriod = 1.0f;
    float fwdStartF = 0.0f, fwdEndF = 0.0f, fwdLenF = 0.0f, fwdRate = 1.0f;
    float bwdEndF = 0.0f, bwdStartF = 0.0f, bwdLenF = 0.0f, bwdRate = 1.0f;
    int32_t hA = 0, hB = 0, pos16 = 0;
    int fwdMode = 0, bwdMode = 0;
    XtLoop loopCfg;
    int32_t startCfg = 0;
    XtBeat beat_;
    uint32_t tempoCfg = 1200000, origCfg = 1200000;
    uint8_t flags0 = 0;
    int32_t lp[4] = { 0, 0x7fffffff, 0x7fffffff, 0 };
};

// v326: the words the block reads and writes, by their places in rom.2's object (the map above): the pitch, the
// formant, the gain's target and the gain; the loops' modes, the heads, the filter's rate and state, the heads'
// fractions, the walk's position, the windows' phases and clocks, the last sample, the forward loop and its rate, the
// first sample, the backward loop and its rate, the period and its inverse
inline std::span<const XtWord> XtLitePlayer::words() {
    static constexpr std::array<XtWord, 30> kWords { {
        { 0x70, offsetof(XtLitePlayer, pitch), sizeof pitch },
        { 0x74, offsetof(XtLitePlayer, formant), sizeof formant },
        { 0x80, offsetof(XtLitePlayer, gainTarget), sizeof gainTarget },
        { 0x84, offsetof(XtLitePlayer, gain), sizeof gain },
        { 0xe8, offsetof(XtLitePlayer, fwdMode), sizeof fwdMode },
        { 0xec, offsetof(XtLitePlayer, bwdMode), sizeof bwdMode },
        { 0xf4, offsetof(XtLitePlayer, hA), sizeof hA },
        { 0xf8, offsetof(XtLitePlayer, hB), sizeof hB },
        { 0xfc, offsetof(XtLitePlayer, dcRate), sizeof dcRate },
        { 0x100, offsetof(XtLitePlayer, dcL), sizeof dcL },
        { 0x104, offsetof(XtLitePlayer, dcR), sizeof dcR },
        { 0x108, offsetof(XtLitePlayer, fracA), sizeof fracA },
        { 0x10c, offsetof(XtLitePlayer, fracB), sizeof fracB },
        { 0x110, offsetof(XtLitePlayer, walkPos), sizeof walkPos },
        { 0x114, offsetof(XtLitePlayer, winA), sizeof winA },
        { 0x118, offsetof(XtLitePlayer, clkA), sizeof clkA },
        { 0x11c, offsetof(XtLitePlayer, winB), sizeof winB },
        { 0x120, offsetof(XtLitePlayer, clkB), sizeof clkB },
        { 0x124, offsetof(XtLitePlayer, lastF), sizeof lastF },
        { 0x128, offsetof(XtLitePlayer, fwdStartF), sizeof fwdStartF },
        { 0x12c, offsetof(XtLitePlayer, fwdEndF), sizeof fwdEndF },
        { 0x130, offsetof(XtLitePlayer, fwdLenF), sizeof fwdLenF },
        { 0x134, offsetof(XtLitePlayer, fwdRate), sizeof fwdRate },
        { 0x138, offsetof(XtLitePlayer, firstF), sizeof firstF },
        { 0x13c, offsetof(XtLitePlayer, bwdEndF), sizeof bwdEndF },
        { 0x140, offsetof(XtLitePlayer, bwdStartF), sizeof bwdStartF },
        { 0x144, offsetof(XtLitePlayer, bwdLenF), sizeof bwdLenF },
        { 0x148, offsetof(XtLitePlayer, bwdRate), sizeof bwdRate },
        { 0x14c, offsetof(XtLitePlayer, period), sizeof period },
        { 0x150, offsetof(XtLitePlayer, invPeriod), sizeof invPeriod },
    } };
    return kWords;
}
