#include "OfflineRenderer.h"
#include "VariphraseEngine.h"

#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <cmath>

// ─── libsndfile I/O ──────────────────────────────────────────────────────────
// If libsndfile is not available, swap this for a minimal WAV reader/writer.

#ifdef HAVE_SNDFILE
#include <sndfile.h>

static bool readWav(const std::string& path,
                    std::vector<float>& samples, int& sr, int& channels) {
    SF_INFO info {};
    SNDFILE* f = sf_open(path.c_str(), SFM_READ, &info);
    if (!f) {
        std::cerr << "Cannot open: " << path << " — " << sf_strerror(nullptr) << "\n";
        return false;
    }
    sr       = info.samplerate;
    channels = info.channels;
    samples.resize(info.frames * channels);
    sf_readf_float(f, samples.data(), info.frames);
    sf_close(f);
    return true;
}

static bool writeWav(const std::string& path,
                     const std::vector<float>& samples, int sr, int channels) {
    SF_INFO info {};
    info.samplerate = sr;
    info.channels   = channels;
    info.format     = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    SNDFILE* f = sf_open(path.c_str(), SFM_WRITE, &info);
    if (!f) {
        std::cerr << "Cannot write: " << path << " — " << sf_strerror(nullptr) << "\n";
        return false;
    }
    sf_writef_float(f, samples.data(), samples.size() / channels);
    sf_close(f);
    return true;
}

#else

// ── Minimal WAV reader (PCM 16-bit / float 32-bit, mono or stereo) ───────────

struct WavHeader {
    char     riff[4];
    uint32_t chunkSize;
    char     wave[4];
    char     fmt[4];
    uint32_t fmtSize;
    uint16_t audioFormat;  // 1=PCM, 3=float
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char     data[4];
    uint32_t dataSize;
};

static bool readWav(const std::string& path,
                    std::vector<float>& samples, int& sr, int& channels) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; return false; }

    WavHeader h;
    fread(&h, sizeof(h), 1, f);
    sr       = h.sampleRate;
    channels = h.numChannels;

    size_t nSamples = h.dataSize / (h.bitsPerSample / 8);
    samples.resize(nSamples);

    if (h.audioFormat == 1 && h.bitsPerSample == 16) {
        std::vector<int16_t> raw(nSamples);
        fread(raw.data(), sizeof(int16_t), nSamples, f);
        for (size_t i = 0; i < nSamples; ++i)
            samples[i] = raw[i] / 32768.0f;
    } else if (h.audioFormat == 3 && h.bitsPerSample == 32) {
        fread(samples.data(), sizeof(float), nSamples, f);
    } else {
        std::cerr << "Unsupported WAV format (use 16-bit PCM or 32-bit float)\n";
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

static bool writeWav(const std::string& path,
                     const std::vector<float>& samples, int sr, int channels) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { std::cerr << "Cannot write: " << path << "\n"; return false; }

    WavHeader h;
    memcpy(h.riff, "RIFF", 4);
    memcpy(h.wave, "WAVE", 4);
    memcpy(h.fmt,  "fmt ", 4);
    memcpy(h.data, "data", 4);

    h.fmtSize      = 16;
    h.audioFormat  = 3;  // float
    h.numChannels  = static_cast<uint16_t>(channels);
    h.sampleRate   = static_cast<uint32_t>(sr);
    h.bitsPerSample= 32;
    h.blockAlign   = static_cast<uint16_t>(channels * 4);
    h.byteRate     = sr * channels * 4;
    h.dataSize     = static_cast<uint32_t>(samples.size() * sizeof(float));
    h.chunkSize    = 36 + h.dataSize;

    fwrite(&h, sizeof(h), 1, f);
    fwrite(samples.data(), sizeof(float), samples.size(), f);
    fclose(f);
    return true;
}

#endif // HAVE_SNDFILE

// ─── Render ──────────────────────────────────────────────────────────────────

int renderOffline(const RenderConfig& config) {
    std::vector<float> inputSamples;
    int sr, channels;

    std::cout << "Reading: " << config.inputPath << "\n";
    if (!readWav(config.inputPath, inputSamples, sr, channels))
        return 1;

    std::cout << "  " << sr << " Hz, " << channels << " ch, "
              << inputSamples.size() / channels << " frames\n";

    // Convert to mono if needed
    std::vector<float> monoInput;
    if (channels == 2) {
        monoInput.resize(inputSamples.size() / 2);
        for (size_t i = 0; i < monoInput.size(); ++i)
            monoInput[i] = 0.5f * (inputSamples[i*2] + inputSamples[i*2+1]);
    } else {
        monoInput = inputSamples;
    }

    // Set up engine
    VSE::VariphraseEngine engine;
    engine.prepare(sr, config.blockSize);
    engine.setAlgorithm(config.algorithm);
    engine.setParams(config.params);

    std::cout << "Processing with algorithm: "
              << static_cast<int>(config.algorithm) << "\n";
    std::cout << "  Pitch:   " << config.params.pitchShiftSemitones << " st\n";
    std::cout << "  Time:    " << config.params.timeStretchRatio << "x\n";
    std::cout << "  Formant: " << config.params.formantShiftSemitones << " st\n";

    std::vector<float> output = engine.processOffline(monoInput);

    std::cout << "Writing: " << config.outputPath << "\n";
    if (!writeWav(config.outputPath, output, sr, 1))
        return 1;

    std::cout << "Done.\n";
    return 0;
}

// ─── CLI ──────────────────────────────────────────────────────────────────────

#ifdef OFFLINE_RENDERER_MAIN

int main(int argc, char** argv) {
    RenderConfig cfg;
    cfg.params.timeStretchRatio = 1.0f;  // defaults

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input"   && i+1 < argc) cfg.inputPath  = argv[++i];
        else if (arg == "--output"  && i+1 < argc) cfg.outputPath = argv[++i];
        else if (arg == "--pitch"   && i+1 < argc) cfg.params.pitchShiftSemitones   = std::stof(argv[++i]);
        else if (arg == "--time"    && i+1 < argc) cfg.params.timeStretchRatio       = std::stof(argv[++i]);
        else if (arg == "--formant" && i+1 < argc) cfg.params.formantShiftSemitones  = std::stof(argv[++i]);
        else if (arg == "--robot")                  cfg.params.robotMode = true;
        else if (arg == "--block"   && i+1 < argc) cfg.blockSize = std::stoi(argv[++i]);
        else if (arg == "--algo"    && i+1 < argc) {
            std::string a = argv[++i];
            if (a == "passthrough") cfg.algorithm = VSE::Algorithm::Passthrough;
            else if (a == "pv")     cfg.algorithm = VSE::Algorithm::PhaseVocoder;
            else if (a == "sms")    cfg.algorithm = VSE::Algorithm::SinusoidalPlusResidual;
            else if (a == "lpc")    cfg.algorithm = VSE::Algorithm::LPCSourceFilter;
            else if (a == "hybrid") cfg.algorithm = VSE::Algorithm::Hybrid;
        }
        else if (arg == "--help") {
            std::cout << "Usage: variphrase_render --input <wav> --output <wav>\n"
                      << "  --pitch <semitones>   Pitch shift (-24 to +24)\n"
                      << "  --time <ratio>        Time stretch (0.25 to 4.0)\n"
                      << "  --formant <semitones> Formant shift (-12 to +12)\n"
                      << "  --algo <name>         passthrough|pv|sms|lpc|hybrid\n"
                      << "  --robot               Enable robot mode\n"
                      << "  --block <samples>     Processing block size (default 512)\n";
            return 0;
        }
    }

    if (cfg.inputPath.empty() || cfg.outputPath.empty()) {
        std::cerr << "Usage: variphrase_render --input <wav> --output <wav> [options]\n"
                  << "Run with --help for full options.\n";
        return 1;
    }

    return renderOffline(cfg);
}

#endif // OFFLINE_RENDERER_MAIN
