#pragma once
// The VC-2's part as the editor sees it (v456; research/UI_DESIGN_VC2.md, "6. The mic's state", d-12): what the player
// writes after each block it hears, and the editor reads on its timer, each word a relaxed atomic, so neither waits on
// the other.  The input stage's largest peak of the last eight blocks (Vc2Unvoiced::peak, rom.4's 0x899d9a5c) and
// whether the block was voiced (0x893ed3e0), the tracked period and its lock (Vc2Tracker), Auto Note's note, the voices
// sounding (Vc2PartVoices' count, 0x893912e0), the output's peak over the block, and the frames since the input's peak
// last reached −60 dBFS.  `playing` says a part has written it.  PluginVc2Play keeps it, so a part made again writes the
// same one.  v459 (d-14): the vocoder engine's bands as they play, LEGACY's 10 or HIRESO's 32, none with the vocoder
// off: each band's mic envelope as the engine's analysis holds it, and the band's output over the block (its peak).
#include <array>
#include <atomic>
#include <cstdint>

struct Vc2PartView {
    static constexpr float kQuiet = 0.001f;           // −60 dBFS
    static constexpr int64_t kQuietFrames = 44100;    // the input under kQuiet that long, a second, is no mic
    static constexpr int kMostBands = 32;             // [FW] HIRESO's bands, the most an engine has
    std::atomic<bool> playing { false }, voiced { false };
    std::atomic<float> inputPeak { 0.0f }, period { 0.0f }, outputPeak { 0.0f };
    std::atomic<int32_t> lock { 0 }, autoNote { -1 }, sounding { 0 };
    std::atomic<int64_t> quietFrames { 0 };
    std::atomic<int32_t> bands { 0 };                                         // v459: the engine's, 0 with it off
    std::array<std::atomic<float>, kMostBands> micBands {}, outBands {};      // v459: by band
};
