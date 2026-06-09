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

// ── WAV reader — chunk-walking, handles 16/24/32-bit PCM and float32 ─────────
//
// Uses RIFF chunk walking rather than a fixed-size header struct.  This
// correctly handles DAW-exported files that have:
//   • extended fmt chunks (fmtSize 18 or 40, common in 24-bit and 48 kHz files)
//   • extra chunks (LIST, fact, bext, etc.) between fmt and data
//   • PCM 16-bit, PCM 24-bit, PCM 32-bit, and IEEE float 32-bit formats
//
// Stereo-to-mono downmix (L+R average) is handled in renderOffline().

static bool readWav(const std::string& path,
                    std::vector<float>& samples, int& sr, int& channels) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) { std::cerr << "Cannot open: " << path << "\n"; return false; }

    // Read RIFF header
    char riff[4], wave[4];
    uint32_t riffSz;
    if (fread(riff, 1, 4, f) < 4 || fread(&riffSz, 4, 1, f) < 1 ||
        fread(wave, 1, 4, f) < 4 ||
        std::string(riff, 4) != "RIFF" || std::string(wave, 4) != "WAVE") {
        std::cerr << "Not a valid WAV file: " << path << "\n";
        fclose(f); return false;
    }

    // Walk chunks
    uint16_t audioFormat = 0, numChannels = 0, bitsPerSample = 0;
    uint32_t sampleRate  = 0, dataSize = 0;
    long     dataOffset  = -1;

    while (true) {
        char id[4]; uint32_t sz;
        if (fread(id, 1, 4, f) < 4 || fread(&sz, 4, 1, f) < 1) break;

        if (std::string(id, 4) == "fmt ") {
            uint8_t buf[40] = {};
            fread(buf, 1, std::min(sz, 40u), f);
            audioFormat  = uint16_t(buf[0])  | (uint16_t(buf[1]) << 8);
            numChannels  = uint16_t(buf[2])  | (uint16_t(buf[3]) << 8);
            sampleRate   = uint32_t(buf[4])  | (uint32_t(buf[5]) << 8)
                         | (uint32_t(buf[6]) << 16) | (uint32_t(buf[7]) << 24);
            bitsPerSample= uint16_t(buf[14]) | (uint16_t(buf[15]) << 8);
            if (sz > 40) fseek(f, sz - 40, SEEK_CUR);
        } else if (std::string(id, 4) == "data") {
            dataSize   = sz;
            dataOffset = ftell(f);
            break;  // data chunk is always last; stop here
        } else {
            fseek(f, sz + (sz & 1), SEEK_CUR);  // skip unknown chunk (word-aligned)
        }
    }

    if (dataOffset < 0 || sampleRate == 0 || numChannels == 0 || bitsPerSample == 0) {
        std::cerr << "Malformed WAV (missing fmt or data chunk): " << path << "\n";
        fclose(f); return false;
    }

    // Validate format
    const bool isPCM   = (audioFormat == 1);
    const bool isFloat = (audioFormat == 3);
    if (!isPCM && !isFloat) {
        std::cerr << "Unsupported WAV audioFormat " << audioFormat
                  << " in " << path << " (expected 1=PCM or 3=float)\n";
        fclose(f); return false;
    }
    if (isFloat && bitsPerSample != 32) {
        std::cerr << "Only 32-bit float WAV is supported (got " << bitsPerSample << "-bit float)\n";
        fclose(f); return false;
    }
    if (isPCM && bitsPerSample != 16 && bitsPerSample != 24 && bitsPerSample != 32) {
        std::cerr << "Unsupported PCM bit depth " << bitsPerSample
                  << " (supported: 16, 24, 32)\n";
        fclose(f); return false;
    }

    sr       = static_cast<int>(sampleRate);
    channels = static_cast<int>(numChannels);

    const size_t bytesPerSample = bitsPerSample / 8;
    const size_t nSamples = dataSize / bytesPerSample;
    samples.resize(nSamples);

    fseek(f, dataOffset, SEEK_SET);

    if (isPCM && bitsPerSample == 16) {
        std::vector<int16_t> raw(nSamples);
        fread(raw.data(), 2, nSamples, f);
        for (size_t i = 0; i < nSamples; ++i)
            samples[i] = raw[i] / 32768.0f;

    } else if (isPCM && bitsPerSample == 24) {
        std::vector<uint8_t> raw(nSamples * 3);
        fread(raw.data(), 1, nSamples * 3, f);
        for (size_t i = 0; i < nSamples; ++i) {
            int32_t v = (int32_t(raw[i*3+2]) << 16)
                      | (int32_t(raw[i*3+1]) <<  8)
                      |  int32_t(raw[i*3+0]);
            if (v & 0x800000) v |= 0xFF000000;  // sign-extend to 32 bits
            samples[i] = v / 8388608.0f;         // 2^23
        }

    } else if (isPCM && bitsPerSample == 32) {
        std::vector<int32_t> raw(nSamples);
        fread(raw.data(), 4, nSamples, f);
        for (size_t i = 0; i < nSamples; ++i)
            samples[i] = raw[i] / 2147483648.0f;  // 2^31

    } else {  // float32
        fread(samples.data(), 4, nSamples, f);
    }

    fclose(f);
    return true;
}

static bool writeWav(const std::string& path,
                     const std::vector<float>& samples, int sr, int channels) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { std::cerr << "Cannot write: " << path << "\n"; return false; }

    // Write minimal 44-byte WAV header (PCM IEEE float, canonical layout)
    auto w2 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    auto w4 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };

    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(float));
    fwrite("RIFF", 1, 4, f);  w4(36 + dataBytes);
    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);  w4(16);
    w2(3);                                              // audioFormat = IEEE float
    w2(static_cast<uint16_t>(channels));
    w4(static_cast<uint32_t>(sr));
    w4(static_cast<uint32_t>(sr * channels * 4));       // byteRate
    w2(static_cast<uint16_t>(channels * 4));            // blockAlign
    w2(32);                                             // bitsPerSample
    fwrite("data", 1, 4, f);  w4(dataBytes);
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

    // ── Offline encode pass (V-Synth-style) ──────────────────────────────────
    // Analyse the full input buffer ONCE to determine content type.
    // This mirrors the V-Synth's encode step: the content type (SOLO /
    // ENSEMBLE / BACKING / LITE) is determined globally before playback so
    // that the real-time processing loop can use a stable routing decision
    // rather than unreliable per-frame ACF thresholding.
    {
        auto analysis = VSE::VariphraseEngine::analyzeContent(
            monoInput.data(), static_cast<int>(monoInput.size()), sr);
        engine.setAnalysis(analysis);

        static const char* kContentNames[] = { "LITE", "SOLO", "ENSEMBLE", "BACKING" };
        const int ct = static_cast<int>(analysis.contentType);
        std::cout << "  Content: " << kContentNames[ct]
                  << "  medianConf=" << analysis.medianPitchConf
                  << "  peakToMean=" << analysis.peakToMeanEnergy << "\n";
    }

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
