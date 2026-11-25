#pragma once
// The part's tones (v340, Vc2Synth whole's first version): the deriving class of each tone's parts' host, 24 in
// Vc2Synth on the part's own words, and what their OSC voices read beyond the tone, the part's.  A tone's tail
// routines (+0x1b14, +0x1b18), the truncation's wait and the OS's event go to the part's notes' task, which the tasks'
// version gives (Vc2SynthToneWorld).  The voices' outside: no render by an address (every one an install writes is the
// voice's own from v337; any other none, silence), the OS's millisecond word, the mic ring, the input stage's
// unvoiced decision and peak, the effect chip's frames, and the wave memory, its headers and loop records, as the part
// is given the waves the process has loaded (vc2Waves(), v504, the plugin's wiring).  HARDWARE_FIRMWARE.md,
// "Vc2Synth's tones".  The notes' task and the audio thread, as the tone.
#include "Vc2ToneHost.h"

class Vc2Unvoiced;
class Vc2WaveMemory;

// What a tone's host reaches in the part beyond its own parts
class Vc2SynthToneWorld {
public:
    virtual ~Vc2SynthToneWorld() = default;
    virtual const Vc2ToneHost& host(int tone) = 0;
    virtual void osEvent(uint32_t event, uint32_t flags) = 0;   // 0x89301830
    virtual void ended(int tone) = 0;        // the tail's +0x1b14 routine, the notes'
    virtual void truncated(int tone) = 0;    // its +0x1b18
    virtual void waitFade(int tone) = 0;     // 0x893017f0(0x1000): the notes' task asleep until the fade is rendered
    // What the voices read from the part: the OS's millisecond word (0x893018d0's), the mic ring, the input stage, the
    // chip's frames for the block (none: silence)
    virtual int32_t osClock() = 0;
    virtual Vc2MicRing& micRing() = 0;
    virtual const Vc2Unvoiced& inputStage() = 0;
    virtual const int32_t* inputFrames() = 0;
    // the waves given: their memory by rom.4's address (none: no waves), and the samples' span, the waves' pool
    virtual const Vc2WaveMemory* waveMemory() = 0;
    virtual Vc2OscSamples waveSamples() = 0;
};

// An OSC voice's reads beyond its tone, the part's
class Vc2SynthOscOutside final : public Vc2OscParts {
public:
    Vc2SynthOscOutside(Vc2SynthToneWorld& world, const Vc2TonePool& pool) : w_(world), pool_(pool) {}
    uint32_t renderBy(uint32_t routine, float* out, uint32_t third) override;
    int32_t osClock() override { return w_.osClock(); }
    Vc2MicRing& micRing() override { return w_.micRing(); }
    const float* unvoiced() override;
    float inputPeak() override;
    const int32_t* inputFrames() override;
    Vc2OscSamples waveSamples() override { return w_.waveSamples(); }
    float boundFloat(uint32_t part, uint32_t offset) override;   // another tone's envelope's or LFO's word
    uint32_t address() override { return 0; }                    // the link's, never asked here
    Vc2OscWaveHeader waveHeader(uint32_t at) override;
    Vc2OscLoopRecord loopRecord(uint32_t at) override;
    long recordsPast = 0;   // v362: the loop records read at an address the waves don't hold (xt_vc2synth's count)

private:
    Vc2SynthToneWorld& w_;
    const Vc2TonePool& pool_;
};

class Vc2SynthTone final : public Vc2ToneHost {
public:
    Vc2SynthTone(int tone, const Vc2ToneCommon& common, Vc2SynthToneWorld& world);
    void ended() override { w_.ended(tone()); }
    void waitFade() override { w_.waitFade(tone()); }
    void truncated() override { w_.truncated(tone()); }
    long recordsPast() const { return outside_.recordsPast; }   // v362

protected:
    const Vc2ToneHost& hostOf(int tone) override { return w_.host(tone); }
    void osEvent(uint32_t event, uint32_t flags) override { w_.osEvent(event, flags); }
    Vc2OscParts& oscOutside(int) override { return outside_; }

private:
    Vc2SynthToneWorld& w_;
    Vc2SynthOscOutside outside_;
};
