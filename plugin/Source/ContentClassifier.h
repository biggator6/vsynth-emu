#pragma once

// Auto's classifier (ContentClassifier.cpp): the encode type the plugin gives a wave whose Encode Type is Auto, by the
// wave's median pitch confidence and the peak-to-mean of its energy.  It is the plugin's own guess, not a port.  Moved as it
// stood from the legacy VariphraseEngine when v191 removed that engine; PluginEncodes.cpp's encodeWave calls it.

#include <atomic>
#include <vector>

namespace VSE {

struct VariphraseAnalysis {
    // V-Synth encode type (manual §3-6):
    //   LITE     — pure tones / single-oscillator content
    //   SOLO     — voiced speech / monophonic melody
    //   ENSEMBLE — polyphonic / chords
    //   BACKING  — drums / transient-rich content
    enum class ContentType { LITE, SOLO, ENSEMBLE, BACKING };

    ContentType contentType  = ContentType::LITE;
    float medianPitchConf    = 0.0f;  // median unbiased ACF confidence (0..1+)
    float peakToMeanEnergy   = 0.0f;  // peak RMS block / mean RMS block (transient ratio)

    // Event stamps for BACKING content: sample positions of detected onsets.
    std::vector<int> onsetSamples;
};

// analyzeContent: offline analysis of a full mono audio buffer.
//   Computes median ACF pitch confidence over all analysis frames,
//   classifies content type (LITE/SOLO/ENSEMBLE/BACKING), and detects
//   onset events for BACKING content.
//   sampleRate: Hz (used for band-energy voiced-speech detector)
//   stop: when given and it goes up, no further frame is analysed (its per-frame autocorrelation takes
//   over 0.04 s a second of audio, measured v185): an encode whose processor is closing.
VariphraseAnalysis analyzeContent(const float* mono,
                                  int numSamples,
                                  double sampleRate,
                                  const std::atomic<bool>* stop = nullptr);

} // namespace VSE
