#pragma once

/**
 * OfflineRenderer.h — WAV-file-in, WAV-file-out renderer for batch testing.
 *
 * Allows the analysis pipeline to exercise the engine without a VST host.
 * Build as a standalone CLI tool:
 *
 *   g++ -std=c++20 OfflineRenderer.cpp VariphraseEngine.cpp PhaseVocoder.cpp \
 *       -o variphrase_render -lsndfile
 *
 * Usage:
 *   ./variphrase_render --input test.wav --output out.wav \
 *       --pitch 7 --time 1.5 --formant -2 --algo pv
 */

#include "VariphraseEngine.h"
#include <string>

struct RenderConfig {
    std::string inputPath;
    std::string outputPath;
    VSE::VariphraseParams params;
    VSE::Algorithm        algorithm = VSE::Algorithm::PhaseVocoder;
    int                   blockSize = 512;
};

/**
 * Render inputPath → outputPath using the VariphraseEngine.
 * Returns 0 on success, non-zero on error.
 * Requires libsndfile for WAV I/O (link with -lsndfile).
 */
int renderOffline(const RenderConfig& config);

// CLI entry point — define OFFLINE_RENDERER_MAIN to build as a standalone tool
#ifdef OFFLINE_RENDERER_MAIN
int main(int argc, char** argv);
#endif
