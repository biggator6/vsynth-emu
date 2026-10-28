#pragma once
// Polyphonic playback of a sample the way the V-Synth XT plays an encoded
// wave.  The sample is encoded once, as the XT does after import:
//   SOLO              the XT's SOLO encoder (XtSoloEncoder, a bit-exact port)
//   BACKING/ENSEMBLE  the firmware's multi-band encoder and load-time band
//                     rebuild, run from the user's rom.2 (XtFirmware)
// Each note then runs its own firmware player (XtSoloVoice / XtMbPlayer) at
// the XT's 44.1 kHz, and the mix is resampled to the host rate.  No JUCE.
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "XtMultiBandPlayer.h"
#include "XtSoloVoice.h"

struct XtEncoded {
    int type = 0;                  // 1 SOLO, 3 BACKING, 5 ENSEMBLE
    int channels = 1;              // the players' channels (SOLO plays mono)
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
std::shared_ptr<XtEncoded> xtSynthEncode(const std::vector<std::vector<float>>& channels, double sampleRate,
                                         int type, int depth, std::string* err = nullptr);

class XtSynth {
public:
    static constexpr int kVoices = 16;
    struct Controls {
        float pitchSemis = 0, timeStretch = 1, formantSemis = 0, energy = 0;
        int rootKey = 60;
        float releaseMs = 30;
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
    void noteOff(int note);
    void allNotesOff();
    void render(float* left, float* right, int n);

private:
    struct Voice {
        bool active = false, releasing = false;
        int note = -1;
        long age = 0, tail = 0;
        float gain = 1, env = 1;
        XtMbPlayer mb;
        XtSoloVoice solo;
    };
    std::shared_ptr<const XtEncoded> cur, pending, retired;
    std::mutex swapLock;
    bool swapPending = false;
    Controls ctl;
    Voice voice[kVoices];
    long clock = 0;
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
};
