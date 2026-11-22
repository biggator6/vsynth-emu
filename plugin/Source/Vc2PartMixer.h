#pragma once
// The VC-2's part mixer (v227): rom.4's block mixer 0x893b1820, once a block from the part's block (0x89395b0e), with
// its words (0x8993eac4…0x8993eae7), its buffers (0x8999f1d0…0x8999f4ff) and the bypass word (0x89934550) word for
// word.  It reads the input frames' +4 word (the mic) and +0x14 into two buffers, runs the input stage on the second,
// kind 6's ring and the unvoiced detector on the first, has each voice render into the vocoder's buffer (a carrier)
// or the other, copies the direct signal into the detector's ring, runs the vocoder on it unless bypassed, and writes
// each frame of the bus: MAIN and the Reverb send from the direct signal, Chorus 0, MULTI from the vocoder's buffer,
// DIR untouched, at three levels easing toward their targets.  The direct signal is the other buffer's right channel,
// or with the EXTIN flag (an OSC Type EXTIN, Vc2MixerParams) the mic itself.  Its level routine 0x893b1480, the targets
// 0x893b1ee0, their setters, suspend and resume, the flag's setter and getter and the reset 0x893b1680 too.  The calls
// go through Vc2MixerParts and Vc2MixerVoice.  Bit for bit: xt_vc2mixer against rom.4's routines in Sh4Interp, the
// calls hooked.  HARDWARE_FIRMWARE.md, "The part mixer"; research/VC2_VOCAL_DESIGNER.md, "The part mixer, run".
// Audio thread.
#include <cstdint>
#include <span>

// The mixer's words as rom.4 lays them out from 0x8993eac4
struct Vc2MixerWords {
    float carried = 0.0f;        // eac4: a voice rendered a carrier this block (1) or none (0), the vocoder's glue's
    int32_t stereo = 0;          // eac8: the settings wrapper's (0x893b1be0), which nothing reads
    int32_t extin = 0;           // eacc: an OSC Type the level routine reads is EXTIN
    int32_t suspendMark = -1;    // ead0: suspended, the flag as it was (0, 1); −1 not
    int32_t suspended = 0;       // ead4: suspended
    int32_t updatedAt = 0;       // ead8: the voices' update's block (0x893b16a0's)
    uint32_t inputAt = 0;        // eadc: rom.4's pointer to the input frames, which the port is given instead
    int32_t scheduledAt = 0;     // eae0: the voices' schedule's block (0x893b1710's)
    int32_t blocks = 0;          // eae4: blocks mixed
};
static_assert(sizeof(Vc2MixerWords) == 0x24, "rom.4's layout, 0x8993eac4…0x8993eae8");

// A block's frames: stereo pairs, 32 of them
struct Vc2MixerFrame {
    float left, right;
};

// The mixer's buffers and levels as rom.4 lays them out from 0x8999f1d0
struct Vc2MixerBuffers {
    static constexpr int kBlock = 32;
    Vc2MixerFrame direct[kBlock];    // 1d0: the voices' other buffer; its right channel the direct signal
    Vc2MixerFrame carrier[kBlock];   // 2d0: the carriers', then the vocoder's output in place
    float mic[kBlock];               // 3d0: the input frames' +4 word, × 2^-31
    float second[kBlock];            // 450: their +0x14 word, the input stage's
    float multiTarget;               // 4d0
    float mfxSend;                   // 4d4: MFX Send Level ÷ 127
    float multi;                     // 4d8: MULTI's level as it eases
    float mainTarget;                // 4dc
    float directSound;               // 4e0: Direct Sound Level ÷ 127
    float main;                      // 4e4
    float reverbTarget;              // 4e8
    float reverbSend;                // 4ec: Reverb Send Level ÷ 127
    float reverb;                    // 4f0
    float saved[3];                  // 4f4…4fc: the targets a suspend keeps: MULTI's, Reverb's, MAIN's
};
static_assert(sizeof(Vc2MixerBuffers) == 0x330, "rom.4's layout, 0x8999f1d0…0x8999f500");

// The three offsets at 0x89997290, the tone generator's sums × 1/127 (0x893a2e50): Direct Sound's, MFX Send's, and
// the output stage's level, which the vocoder reads
struct Vc2MixerOffsets {
    float direct, mfx, level;
};

// What the level routine reads, as the parameters hold them
struct Vc2MixerParams {
    // OSC Type (0x2e5) read through 0x89320a50(obj, 0, k), k 0 and 1: OSC 1's and OSC 2's by that routine's reading,
    // zone 0's and 1's OSC 1 by the mixer's run (VC2_VOCAL_DESIGNER.md); unsettled. 2 is EXTIN
    int32_t oscType[2];
    int32_t direct;        // Direct Sound Level (0x195, through the part's block), 0…127
    int32_t reverbSend;    // Reverb Send Level (0x2e2, through 0x89320a50(obj, 0, 0))
    int32_t mfxSend;       // MFX Send Level (0x2e0, the same)
};

// What the mixer calls, in its order
class Vc2MixerParts {
public:
    virtual ~Vc2MixerParts() = default;
    virtual void inputStage(const float* second) = 0;   // 0x893ebb00(buf, 32): the tracker's block and Auto Note
    virtual void micRing(const float* mic) = 0;         // 0x893ec820(buf, 32): kind 6's ring
    virtual bool engine() = 0;                          // the vocoder object's +0x20: an engine plays
    virtual void unvoiced(const float* mic) = 0;        // 0x893ed040: the unvoiced detector
    virtual float* detectorRing() = 0;                  // 0x893ed410: where the detector's mic ring takes the block
    virtual void vocoder(const float* ring, Vc2MixerFrame* frames) = 0;   // 0x893f0580: the stereo driver
    virtual void resetTracker() = 0;                    // 0x893ee040, the reset's
    virtual void resetUnvoiced() = 0;                   // 0x893ecf60
    virtual void resetRing() = 0;                       // 0x893ec7d0
};

// A voice of the part as the mixer asks it: by its methods at +0x54, +0x18, +0x60 and +0x24
class Vc2MixerVoice {
public:
    virtual ~Vc2MixerVoice() = default;
    virtual bool carrier() = 0;                         // +0x54: it renders a carrier
    virtual void renderCarrier(Vc2MixerFrame* frames) = 0;   // +0x18: into the vocoder's buffer
    virtual bool held() = 0;                            // +0x60: without the EXTIN flag, it renders nothing more
    virtual void render(Vc2MixerFrame* frames) = 0;     // +0x24: into the other buffer
};

// A tone of the part as the voices' update and schedule ask it (v311): by its methods at +0x0c, +0x30 and +0x3c, v228's
class Vc2MixerTone {
public:
    virtual ~Vc2MixerTone() = default;
    virtual void update(int32_t n) = 0;      // +0x0c: n ticks of its envelopes, LFOs and OSC voices
    virtual void clock(float seconds) = 0;   // +0x30: its LFOs' clock
    virtual void count(uint32_t x) = 0;      // +0x3c: its OSC voices' count
};

// What the voices' update and schedule call beyond the tones (v311)
class Vc2MixerTaskParts {
public:
    virtual ~Vc2MixerTaskParts() = default;
    virtual void freeLfosClock(float seconds) = 0;   // 0x893ad870 on the free LFOs 0x8a0c6d84 (v271's)
    virtual void glue(float carried) = 0;            // 0x893f07a0 on the vocoder 0x89999758 (v220's)
};

class Vc2PartMixer {
public:
    static constexpr int kBlock = Vc2MixerBuffers::kBlock;
    static constexpr int kFrameWords = 8;   // a bus frame: MAIN L, R, Reverb, Chorus, MULTI L, R, DIR L, R
    // 0x893b1820: 32 frames of 8 int32 in, 32 of 8 floats out (DIR left as it was)
    void block(const int32_t* in, float* out, std::span<Vc2MixerVoice* const> voices, Vc2MixerParts& parts);
    void levels(const Vc2MixerParams& p);   // 0x893b1480's arithmetic, a patch's: the flag, settings and targets
    void targets();                         // 0x893b1ee0, the converter's: nothing while bypassed
    void setDirect(float v) { b.directSound = v; }      // 0x893b1eb0
    void setReverbSend(float v) { b.reverbSend = v; }   // 0x893b1ec0
    void setMfxSend(float v) { b.mfxSend = v; }         // 0x893b1ed0
    void suspend();                         // 0x893b1390: the targets kept and zeroed, the vocoder bypassed
    void resume();                          // 0x893b1410
    void setExtin(int32_t on) { w.extin = on; }   // 0x893b1370, which nothing calls
    int32_t extin() const { return w.extin; }     // 0x893b1380, the same
    void reset(Vc2MixerParts& parts);       // 0x893b1680: the tracker's, the detector's and the ring's resets
    // v311: the voices' update 0x893b16a0 (the blocks since its last in 10 ms ticks, at least 1, to each tone's
    // update, then the glue) and schedule 0x893b1710 (the blocks since its last in seconds, a block's when none, to
    // the free LFOs' clock and each tone's clock and count of 1); the tones given as the list stands
    void updateTones(std::span<Vc2MixerTone* const> tones, Vc2MixerTaskParts& parts);
    void scheduleTones(std::span<Vc2MixerTone* const> tones, Vc2MixerTaskParts& parts);

    Vc2MixerWords w {};
    Vc2MixerBuffers b {};
    int32_t bypass = 0;                     // 0x89934550: the vocoder bypassed (suspended, a patch changing)
    Vc2MixerOffsets offsets {};             // 0x89997290, the tone generator's

private:
    void walk(std::span<Vc2MixerVoice* const> voices, bool askHeld);   // the voices, a path's way
    void frames(const float* direct, int stride, float* out, float ease);   // the levels' ease and the bus frames
};
