#pragma once
// The part's sounding tones as the part mixer takes them, the part's own (v351, Vc2Synth whole's eleventh version; v349's
// and v350's moved here from Vc2SynthUpdate): the sounding list (0x899996d0) in its order, by the tones' +0 links, each
// a tone of the pool on its host.  As the voices' update and schedule ask a tone (Vc2MixerTone: its +0x0c, +0x30 and
// +0x3c) and as the block asks it (Vc2MixerVoice: its +0x54, then +0x18 into the carriers' buffer; else its +0x60, then
// +0x24 into the other buffer), the tones' render buffers a, b and c (0x89998b20) between.  Bit for bit: xt_vc2synth's
// notes part's update passes, notes' task's passes and blocks.  HARDWARE_FIRMWARE.md, "The part's block".  The block
// and the update task.
#include "Vc2PartMixer.h"
#include "Vc2Tone.h"
#include <array>
#include <span>
#include <vector>

struct Vc2Synth;
class Vc2SynthTone;

class Vc2SynthTones {
public:
    explicit Vc2SynthTones(Vc2Synth& s);
    std::span<Vc2MixerTone* const> tasksTones();   // the sounding list's, for the voices' update and schedule
    std::span<Vc2MixerVoice* const> voices();      // the same, for the block

private:
    // A tone of the pool, its methods on its host
    class Tone final : public Vc2MixerTone, public Vc2MixerVoice {
    public:
        Tone(Vc2Synth& s, int index) : s_(s), index_(index) {}
        void update(int32_t n) override;                       // +0x0c
        void clock(float seconds) override;                    // +0x30
        void count(uint32_t x) override;                       // +0x3c
        bool carrier() override;                               // +0x54
        void renderCarrier(Vc2MixerFrame* frames) override;    // +0x18
        bool held() override;                                  // +0x60
        void render(Vc2MixerFrame* frames) override;           // +0x24

    private:
        Vc2Tone& tone();
        Vc2SynthTone& host();
        Vc2Synth& s_;
        int index_;
    };
    // The sounding list's tones in its order into both lists; how many
    size_t walk();
    Vc2Synth& s_;
    std::vector<Tone> tones_;   // one a tone of the pool, made with the part
    std::array<Vc2MixerTone*, Vc2TonePool::kTones> tasks_ {};
    std::array<Vc2MixerVoice*, Vc2TonePool::kTones> voices_ {};
};
