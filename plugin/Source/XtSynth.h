#pragma once
// Polyphonic playback of a sample the way the V-Synth XT plays an encoded
// wave.  The sample is encoded once, as the XT does after import:
//   SOLO              the XT's SOLO encoder (XtSoloEncoder, a bit-exact port)
//   BACKING/ENSEMBLE  the firmware's multi-band encoder and load-time band
//                     rebuild, run from the user's rom.2 (XtFirmware)
// Each note then runs its own firmware player (XtSoloVoice / XtMbPlayer) at
// the XT's 44.1 kHz under the XT's amp envelope (XtEnvelope), and the mix is
// resampled to the host rate.  No JUCE.
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "XtEnvelope.h"
#include "XtMultiBandPlayer.h"
#include "XtSoloVoice.h"

struct XtEncoded {
    int type = 0;                  // 1 SOLO, 3 BACKING, 5 ENSEMBLE
    int channels = 1;              // the wave's channels (1 or 2)
    long frames = 0;               // at 44.1 kHz
    std::vector<uint8_t> vpdt;     // multi-band
    XtMbBands bands;
    XtMbWave wave;
    XtSoloPlan solo;               // SOLO
};

// The user's rom.2: loads the encoders' and players' tables and opens the
// firmware for the interpreter.  False if the file is not a usable image.
bool xtSynthSetRom(const std::string& path);
bool xtSynthRomReady();

// Encodes planar float channels at `sampleRate`.  type: 1 SOLO, 3 BACKING,
// 5 ENSEMBLE; depth: Encode Depth 0..127 (multi-band attack detection).
// The sample is imported as the XT imports a WAV: to int16 (sources wider
// than 16 bits keep their top 16, as the XT's 24/32-bit readers do), then
// the XT's own converter to 44.1 kHz (xtFwImport) when the rate differs.
// `bits`: the source's bits per sample.
std::shared_ptr<XtEncoded> xtSynthEncode(const std::vector<std::vector<float>>& channels, double sampleRate,
                                         int type, int depth, std::string* err = nullptr, int bits = 16);

class XtSynth {
public:
    static constexpr int kVoices = 16;
    struct Controls {
        float pitchSemis = 0, timeStretch = 1, formantSemis = 0, energy = 0;
        int rootKey = 60;
        // amp envelope knobs, 0..127 as on the XT (release 16 = 30 ms);
        // velocity → time sensitivities −63..+63; Time KF −200..+200
        int attack = 0, decay = 0, sustain = 127, release = 16;
        int aSens = 0, dSens = 0, rSens = 0, timeKF = 0;
    };

    XtSynth();
    ~XtSynth();
    void prepare(double hostRate);
    // Any thread: the next render() starts using this sample (voices stop).
    void setSample(std::shared_ptr<const XtEncoded> e);
    bool hasSample() const { return cur != nullptr; }

    // Audio thread
    void setControls(const Controls& c);
    void noteOn(int note, float velocity);
    void noteOff(int note, float velocity = 0.5f);
    void allNotesOff();
    void render(float* left, float* right, int n);

private:
    struct Voice {
        bool active = false, releasing = false;
        int note = -1;
        long age = 0, tail = 0;
        float gain = 1;               // velocity
        int vel = 64, relVel = 64;     // 0..127, for the envelope's times
        XtEnvelope env;
        // The TVA that applies the envelope (0x894f5360 / 0x894f5720): gain,
        // its target and per-sample step, frames since the last update.
        float cur = 0, target = 0, step = 0, elapsed = 0;
        XtMbPlayer mb;
        XtSoloVoice solo;
    };
    std::shared_ptr<const XtEncoded> cur, pending, retired;
    std::mutex swapLock;
    bool swapPending = false;
    Controls ctl;
    Voice voice[kVoices];
    long clock = 0;
    // The XT's control clock: the tone update (envelope step, TVA targets)
    // runs between blocks once per 441 frames (10 ms).
    static constexpr int kTickFrames = 441;
    int tickCount = 0;                // frames since the last update
    double hostRate = 44100;

    // 44.1 kHz mix, produced in 32-frame blocks, read by the resampler
    std::vector<float> src;        // interleaved L/R
    size_t srcFrames = 0;
    double readPos = 0;            // in source frames
    double step = 1;               // 44100 / hostRate
    std::vector<float> kernel;     // polyphase windowed sinc
    int kHalf = 16, kPhases = 512;

    void pickUpSample();
    void mixBlock(float* lr);      // 32 frames
    void voiceRates(Voice& v) const;
    void tvaUpdate(Voice& v);
    XtEnvParams envParams(const Voice& v) const;
};
