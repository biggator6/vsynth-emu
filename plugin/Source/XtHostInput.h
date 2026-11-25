#pragma once
// The plugin's outside input on its way to the EXT IN OSCs (v207; HARDWARE_FIRMWARE.md, "The input's audio"): the host's
// frames at its rate brought to the chip's 44.1 kHz by XtSinc, the way XtSynth's render goes the other, each word an
// int32 at the XT's full scale (±1 to ±2^31, as the converter hands the chip's input slots), and held ahead of the blocks
// that take them, 32 at each XtSynth::busOut, by a latency fixed at prepare so that no block comes short.  It is
// XtSynth::input's feed (XtExtInFeed): XtSynth puts its block into the chip's slots with the Effects on, or has it back
// as the chip would hand it with them off (XtExtInCopy, by External In Type).  Not the firmware's: the XT's converter
// runs at 44.1 kHz.
#include "XtExtIn.h"
#include "XtSinc.h"

#include <array>
#include <cstdint>

class XtHostInput final : public XtExtInFeed {
public:
    XtHostInput() { prepare(44100.0); }
    // Not on the audio thread: the host's rate; what it held emptied, and latency() frames of silence ahead of the blocks
    void prepare(double hostRate);
    // The same, and this the feed of `feeds` (XtSynth::input), which is emptied
    void prepare(double hostRate, XtExtInput& feeds) {
        prepare(hostRate);
        feeds.setFeed(this);
        feeds.clear();
    }
    // Audio thread: External In Type (ID 0x78), for the copy with the Effects off
    void setType(int type) { copy_.setType(type); }
    // Audio thread, before XtSynth renders the same frames: n of the host's input, ±1, a channel a side (r the same as l
    // for a mono input; both nullptr for none, which is silence).  At most most() a call, or what it holds overruns.
    void push(const float* l, const float* r, int n);
    int most() const { return most_; }
    // How far behind what's pushed the blocks run, in frames at 44.1 kHz: from the host's frame to the chip's slots, or
    // to the copy; the EXT IN OSCs hear it a block and a frame later (XtSynth::busOut)
    int latency() const { return latency_; }
    // For the check: the 44.1 kHz frames made and not yet taken, blocks that came short of frames (zeros past them),
    // and frames pushed past what it holds
    uint32_t ready() const { return out_.held(); }
    long shortBlocks() const { return short_; }
    long overruns() const { return over_; }

    const int32_t* block() override;
    // v455: up to `frames` of what it holds taken and dropped, the oldest first: how many were; and the frames taken or
    // passed over since prepare, counted on (the VC-2's part reads each block's input at its own place: a block that ran
    // ahead of the host in a wait passes over the input it had no frames for, Vc2PartPlayer.h)
    int drop(int frames);
    uint32_t taken() const { return out_.taken() - takenFrom_; }
    void withoutChip(XtExtInput& to, const int32_t* lr) override { copy_.into(to, lr, 32); }

    // A host sample as the XT's int32: x × 2^31 toward zero, the limits past ±1, and silence for what isn't a number
    static int32_t wordOf(float x);

private:
    static constexpr int kHalf = XtSinc::kHalf;
    static constexpr int kChunk = 1024;                 // host frames taken into the history at a time
    static constexpr int kHistory = 2 * kHalf + kChunk; // what the history holds: the taps either side, and a chunk
    void produce();   // every 44.1 kHz frame the history now reaches, into out_, and the frames no tap needs dropped
    long long firstTap(long long frame) const;   // the first history frame (counted from base_'s start) frame j reads

    XtSinc sinc_;
    double step_ = 1.0;          // host frames a 44.1 kHz frame
    bool direct_ = true;         // the host at 44.1 kHz: a frame is a frame
    std::array<float, 2 * kHistory> history_ {};   // interleaved L/R host frames, from frame base_ on
    long long base_ = 0;         // the history's first frame, counting kHalf frames of silence before the host's first
    int held_ = 0;               // frames in the history
    long long next_ = 0;         // the next 44.1 kHz frame to make
    XtExtInput out_;             // the 44.1 kHz frames made and not yet taken
    XtExtInCopy copy_;
    int latency_ = 32, most_ = 1;
    long short_ = 0, over_ = 0;
    uint32_t takenFrom_ = 0;     // what out_ had taken at prepare (v455)
};
