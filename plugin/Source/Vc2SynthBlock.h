#pragma once
// The part's block's parts, the part's own (v351, Vc2Synth whole's eleventh version): what rom.4's part mixer
// 0x893b1820 calls on the part, once a block (the audio task's, from 0x89395b0e; its DMA and interrupts are the
// plugin's processBlock).  The input stage 0x893ebb00 (the tracker's block, then Auto Note) on the frames' +0x14
// words; kind 6's mic ring 0x893ec820 with the tracker's period; with no engine the unvoiced detector 0x893ed040; the
// detector's mic block 0x893ed410; the vocoder's stereo driver 0x893f0580 with the output stage's word 0x89997298 (the
// tone generator's third level); the resets 0x893ee040, 0x893ecf60 and 0x893ec7d0.  Auto Note's host: its note-on
// 0x89391dc0, none while source 3's key table on source 4's part 0 node's channel holds a key (0x89392670 over
// 0…127), else the note + 12 at 0x8993d034 and the notes' task posted 0x020; its note-off 0x89391e20, the note + 12 at
// 0x8993d038 and 0x040 posted.  Bit for bit: xt_vc2synth's notes part's blocks.  HARDWARE_FIRMWARE.md, "The part's
// block".  The block.
#include "Vc2AutoNote.h"
#include "Vc2PartMixer.h"

struct Vc2Synth;

class Vc2SynthBlock final : public Vc2MixerParts, public Vc2AutoNoteHost {
public:
    explicit Vc2SynthBlock(Vc2Synth& s) : s_(s) {}
    void inputStage(const float* second) override;
    void micRing(const float* mic) override;
    bool engine() override;
    void unvoiced(const float* mic) override;
    float* detectorRing() override;
    void vocoder(const float* ring, Vc2MixerFrame* frames) override;
    void resetTracker() override;
    void resetUnvoiced() override;
    void resetRing() override;
    bool noteOn(int32_t note, float level) override;
    void noteOff(int32_t note) override;

private:
    Vc2Synth& s_;
};
