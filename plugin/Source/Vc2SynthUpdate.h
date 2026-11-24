#pragma once
// The part's update task's parts, the part's own (v349, Vc2Synth whole's ninth version): what rom.4's update task's
// pass (vc2UpdateTaskPass, v310) calls on the part.  A node's update by what it is: the static M's node (its class
// 0x898cd460, whose +0x0c is M's init 0x893a2ee0 on the node less 0x170), M's init on the static M, the Matrix core's
// refresh reading its sources as its links then stand, answering 1 (it stays on the queue); a voice's by its class,
// the note voice's (0x898cd3dc: 0x893970b0, Vc2VoiceClass) and the plain voice's (0x89391730), each answering 0.  The
// pitch source 0x89394a60 on 0x89989748: its +8 the robot depth (0x89997fdc), then with Auto Note's On Switch
// (0x8993fe50) set or the depth above 0 the notes' task posted 0x100 (0x89391e40).  The voices' update 0x893b16a0: the
// part mixer's on the sounding list's tones in its order (each its +0x0c with the ticks), then the vocoder's glue
// (0x893f07a0) with the tracker's period.  The queue's lock is the OS's, nothing on one thread.  Bit for bit:
// xt_vc2synth's notes part's update passes.  HARDWARE_FIRMWARE.md, "The part's update task".  The update task.
#include "Vc2PartMixer.h"
#include "Vc2Tone.h"
#include "Vc2UpdateTask.h"
#include <array>
#include <vector>

struct Vc2Synth;

class Vc2SynthUpdate final : public Vc2UpdateTaskParts, public Vc2MixerTaskParts {
public:
    explicit Vc2SynthUpdate(Vc2Synth& s);
    // The queue's lock: the OS's
    void wait(uint32_t) override {}
    void signal(uint32_t) override {}
    void post(uint32_t) override {}
    uint32_t nodeUpdate(uint32_t node, uint32_t time) override;
    void pitchSource() override;
    void voicesUpdate() override;
    // v350: the voices' schedule 0x893b1710, the notes' task's: the part mixer's on the sounding list's tones (the
    // free LFOs' clock, then each tone's +0x30 and +0x3c)
    void schedule();
    // The mixer's tasks' calls beyond the tones: the free LFOs' clock (the schedule's), the vocoder's glue
    void freeLfosClock(float seconds) override;
    void glue(float carried) override;

private:
    // A tone of the pool as the mixer's tasks take it: its +0x0c, +0x30 and +0x3c on its host
    class Tone final : public Vc2MixerTone {
    public:
        Tone(Vc2Synth& s, int index) : s_(s), index_(index) {}
        void update(int32_t n) override;
        void clock(float seconds) override;
        void count(uint32_t x) override;

    private:
        Vc2Synth& s_;
        int index_;
    };
    // The sounding list's tones in its order (0x899996d0's, by their +0 links); how many
    size_t sounding();
    Vc2Synth& s_;
    std::vector<Tone> tones_;   // one a tone of the pool, made with the part
    std::array<Vc2MixerTone*, Vc2TonePool::kTones> listed_ {};
};
