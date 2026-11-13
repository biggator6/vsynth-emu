// vc2run <rom.4> mixer: the VC-2 part's mixer and its levels, run on rom.4's own routines (research/VC2_VOCAL_DESIGNER.md,
// "The part mixer, run").  The vocoder module's init entries; rom.4's default patch made a store (vc2patches' loadStore)
// and part 0's entry in the part table pointed at it; Direct Sound Level (0x195), zone 0's MFX and Reverb Send Levels
// (0x2e0, 0x2e2) and zone 0's and 1's OSC 1 Type (0x2e5) set; the level routine (0x893b1480, from 0x89360880) run and its
// targets read.  Then the live block mixer (0x893b1820(out), called each block from 0x89395b0e) on 32 input frames
// whose +4 word is a constant mic and +0x14 another, with an empty voice list and the vocoder's block call (0x893f0580)
// answered here with a constant stereo pair: each output frame's eight words (MAIN L, R, the Reverb and Chorus sends,
// MULTI L, R, DIR L, R, as XtSynth's frame) said across blocks, and through the suspend and resume pair (0x893b1390,
// 0x893b1410).  Fails on a fault or a store write its reader doesn't read back.
#include "vc2patches.h"
#include <cstdio>
#include <cstring>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kLevels = 0x893b1480, kMixer = 0x893b1820;      // [FW]
constexpr uint32_t kSuspend = 0x893b1390, kResume = 0x893b1410;    // [FW]
constexpr uint32_t kVocoderBlock = 0x893f0580;   // [FW] (vocoder, input, output): answered here
constexpr uint32_t kPartTable = 0x8950d8e0;      // [FW] a pointer per part to its entry: +0 1, +8 the store
constexpr uint32_t kInFrames = 0x8993eadc, kVoices = 0x899996d0;   // [FW] the input frames' pointer, the voice list
constexpr uint32_t kHasExtIn = 0x8993eacc, kBypass = 0x89934550;   // [FW]
// [FW] the levels: each target and where the mixer has got to, and the settings they're made from
constexpr uint32_t kMultiTarget = 0x8999f4d0, kMultiNow = 0x8999f4d8, kMainTarget = 0x8999f4dc, kMainNow = 0x8999f4e4;
constexpr uint32_t kReverbTarget = 0x8999f4e8, kReverbNow = 0x8999f4f0;
constexpr uint32_t kMfxSend = 0x8999f4d4, kDirect = 0x8999f4e0, kRevSend = 0x8999f4ec;
constexpr uint32_t kDirectId = 0x195, kMfxSendId = 0x2e0, kRevSendId = 0x2e2, kTypeId = 0x2e5;   // [FW]
constexpr uint32_t kFrames = kScratch + 0x20000, kOut = kScratch + 0x21000;
constexpr uint32_t kFrameBytes = 32, kFrameWords = 8, kBlock = 32, kMicWord = 4, kOtherWord = 0x14;
constexpr uint32_t kMarker = 0x12345678;          // in the output frames before a block, to show what it leaves
constexpr int32_t kMic = 0x10000000, kOther = 0x08000000;   // the input words, 1/8 and 1/16 of full scale
constexpr float kVocoderL = 1000, kVocoderR = -1000;        // the vocoder's output, as answered
constexpr int kBlocks = 300, kSuspendBlocks = 100;
constexpr const char* kWordNames[] = {"MAIN L", "MAIN R", "Rev", "Cho", "MULTI L", "MULTI R", "DIR L", "DIR R"};

struct Setting {
    const char* name;
    int direct, mfxSend, revSend, type0, type1;   // Direct Sound, zone 0's sends, zones 0's and 1's OSC 1 Type
};
constexpr Setting kSettings[] = {
    {"no EXTIN OSC: Direct 127, MFX Send 127, Reverb Send 64", 127, 127, 64, 1, 1},
    {"zone 1's OSC 1 EXTIN: Direct 64, MFX Send 100, Reverb Send 127", 64, 100, 127, 1, 2},
};

bool setLevels(Machine& m, const Setting& s) {
    const uint32_t zone0 = kPatchStore + kToneBase, zone1 = zone0 + kToneBlockBytes;
    if (!setStoreValue(m, kPatchStore, kDirectId, s.direct)) return false;
    if (!setStoreValue(m, zone0, kMfxSendId, s.mfxSend)) return false;
    if (!setStoreValue(m, zone0, kRevSendId, s.revSend)) return false;
    if (!setStoreValue(m, zone0, kTypeId, s.type0) || !setStoreValue(m, zone1, kTypeId, s.type1)) return false;
    return m.run(kLevels, {});
}

void sayLevels(Machine& m) {
    std::printf("    settings ÷127: Direct %.5f, MFX Send %.5f, Reverb Send %.5f; EXTIN present %d, bypass %d\n",
                m.f32(kDirect), m.f32(kMfxSend), m.f32(kRevSend), (int32_t)m.cpu.rd32(kHasExtIn),
                (int32_t)m.cpu.rd32(kBypass));
    std::printf("    targets (now): MULTI %.6g (%.6g), MAIN %.6g (%.6g), Reverb %.6g (%.6g)\n", m.f32(kMultiTarget),
                m.f32(kMultiNow), m.f32(kMainTarget), m.f32(kMainNow), m.f32(kReverbTarget), m.f32(kReverbNow));
}

void sayFrame(Machine& m, int block, uint32_t frame) {
    std::printf("    block %3d frame %2u:", block, frame);
    for (uint32_t w = 0; w < kFrameWords; ++w) {
        const uint32_t at = kOut + kFrameBytes * frame + 4 * w;
        if (m.cpu.rd32(at) == kMarker)
            std::printf(" %s left", kWordNames[w]);
        else
            std::printf(" %s %.6g", kWordNames[w], m.f32(at));
    }
    std::printf("\n");
}

struct Mix {
    long vocoderCalls = 0;
};

void hookVocoder(Machine& m, Mix& mix) {
    m.cpu.hook(kVocoderBlock, [&mix](Sh4& c) {
        ++mix.vocoderCalls;
        uint32_t l = 0, r = 0;
        std::memcpy(&l, &kVocoderL, 4);
        std::memcpy(&r, &kVocoderR, 4);
        for (uint32_t k = 0; k < kBlock; ++k) {
            c.wr32(c.r[6] + 8 * k, l);
            c.wr32(c.r[6] + 8 * k + 4, r);
        }
        c.ret(0);
    });
}

// The block's input frames: the mic at +4, another input at +0x14, the rest 0; and no voices
void setFrames(Machine& m) {
    const std::vector<uint8_t> zero(kFrameBytes * kBlock, 0);
    m.cpu.write(kFrames, zero.data(), zero.size());
    for (uint32_t k = 0; k < kBlock; ++k) {
        m.cpu.wr32(kFrames + kFrameBytes * k + kMicWord, (uint32_t)kMic);
        m.cpu.wr32(kFrames + kFrameBytes * k + kOtherWord, (uint32_t)kOther);
    }
    m.cpu.wr32(kInFrames, kFrames);
    m.cpu.wr32(kVoices, 0);
}

// `blocks` of the mixer, the first and last frames said at blocks 0, 1, 10 and the last; false on a fault
bool mixBlocks(Machine& m, int blocks) {
    const int said[] = {0, 1, 10, blocks - 1};
    for (int b = 0; b < blocks; ++b) {
        for (uint32_t w = 0; w < kFrameWords * kBlock; ++w) m.cpu.wr32(kOut + 4 * w, kMarker);
        if (!m.run(kMixer, {kOut})) return false;
        for (const int at : said)
            if (b == at) {
                sayFrame(m, b, 0);
                sayFrame(m, b, kBlock - 1);
            }
    }
    return true;
}

bool runSetting(Machine& m, Mix& mix, const Setting& s) {
    std::printf("\n%s\n", s.name);
    if (!setLevels(m, s)) return false;
    sayLevels(m);
    mix.vocoderCalls = 0;
    if (!mixBlocks(m, kBlocks)) return false;
    std::printf("    vocoder block calls: %ld in %d blocks\n", mix.vocoderCalls, kBlocks);
    std::printf("  suspended (0x893b1390):\n");
    mix.vocoderCalls = 0;
    if (!m.run(kSuspend, {})) return false;
    sayLevels(m);
    if (!mixBlocks(m, kSuspendBlocks)) return false;
    std::printf("    vocoder block calls: %ld in %d blocks\n", mix.vocoderCalls, kSuspendBlocks);
    std::printf("  resumed (0x893b1410):\n");
    if (!m.run(kResume, {})) return false;
    sayLevels(m);
    return mixBlocks(m, kSuspendBlocks);
}

} // namespace

int mixer(Machine& m, int, char**) {
    Mix mix;
    bool ok = initVocoderModule(m) && loadStore(m, {});
    const uint32_t entry = m.cpu.rd32(kPartTable);
    m.cpu.wr8(entry, 1);
    m.cpu.wr32(entry + 8, kPatchStore);
    setFrames(m);
    hookVocoder(m, mix);
    std::printf("the input: mic word %#x (%.4f of full scale), the other %#x; the vocoder answers L %g, R %g\n",
                (uint32_t)kMic, kMic * 0x1p-31, (uint32_t)kOther, kVocoderL, kVocoderR);
    for (const Setting& s : kSettings) ok = ok && runSetting(m, mix, s);
    if (ok) return 0;
    std::printf("mixer: faulted: %s\n", m.error().c_str());
    return 1;
}

} // namespace vc2
