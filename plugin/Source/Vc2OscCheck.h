#pragma once
// xt_vc2osc's shared parts (v230, v232): the calls each side logs, what the hooked routines answer and the records the
// voice reads (drawn once for both sides), the port's fake parts, and rom.4's side: every routine the OSC voice calls
// hooked.  XT_VC2OSC_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2OscVoice.h"
#include <array>
#include <map>
#include <vector>

inline constexpr int kOscVoices = 2;
inline constexpr uint32_t kOscVoiceAt = 0x8C060000, kOscVoiceStride = 0x1000;   // the voices, in Vc2Fw's RAM
inline constexpr uint32_t kOscBlockAt = 0x8C058000;      // a note-on's block, the same
inline constexpr uint32_t kOscSettingsAt = 0x8C059000;   // the settings' block
inline constexpr uint32_t kOscOutAt = 0x8C05a000;        // a render's output
inline constexpr uint32_t kOscRecordsAt = 0x8C05b000;    // the headers, loop records, envelopes and LFOs the voice reads
inline constexpr uint32_t kOscRecordStride = 0x40;
inline constexpr int kOscRecords = 2;                    // of each kind
inline constexpr uint32_t kOscRendersAt = 0x8C05c000;    // hooked render routines the tail's +0xa44, +0xa48 point at
inline constexpr int kOscRenders = 2;
inline constexpr uint32_t kOscBlockWords = Vc2OscVoice::kBlockBytes / 4, kOscOther = 0xff;

enum OscKind : int32_t {
    kBuildVibrato, kUpdateVibrato, kSetVibrato, kNoteOnVibrato, kBuildRamp, kTickRamp, kStartRamp, kSetRamp,
    kLegatoRamp, kRampValue, kKindCall, kKindSettings, kSetCarrier, kInitCarrier, kStartKind6, kRenderBy,
    kRenderCarrier, kRenderKind6, kSeek,
};

// A call as either side made it: what, on which voice, and its arguments (a ramp's number, counts, floats' bits, a
// routine's address, a settings word's offset in the settings' block or a word's in the voice, the output as 1, a
// seek's object byte 0)
struct OscCall {
    int32_t kind, voice;
    uint32_t a, b, c;
    bool operator==(const OscCall&) const = default;
};

// The records the voice reads at the addresses its words name: the wave headers and loop records (Vc2OscVoice.h's
// offsets), and the bound envelopes' and LFOs' words by address
struct OscRecords {
    std::array<Vc2OscWaveHeader, kOscRecords> headers {};
    std::array<Vc2OscLoopRecord, kOscRecords> loops {};
    std::map<uint32_t, float> bound {};
};
uint32_t oscHeaderAt(int k);
uint32_t oscLoopAt(int k);
uint32_t oscEnvelopeAt(int k);
uint32_t oscLfoAt(int k);

// What the hooked routines answer, drawn once for both sides; the vibrato's two outputs and each ramp's factor, read
// in place (each ramp's its own, so a target reading the wrong ramp's differs); the flags byte a seek leaves in the
// object (a player may write it)
struct OscDraws {
    float rampValue = 0.0f, vibratoPitch = 0.0f, vibratoLevel = 0.0f;
    std::array<float, Vc2OscVoice::kRamps> rampFactors {};
    uint32_t rendered = 0;
    uint8_t seekFlags = 0;
};

// What the port's parts share: the answers, the records, the blocks' words, the output and the calls
struct OscShared {
    OscDraws draws {};
    OscRecords records {};
    std::array<uint32_t, kOscBlockWords> settings {};
    std::array<float, Vc2OscVoice::kOut> out {};
    std::vector<OscCall> calls {};
};

// A voice's port parts: answers from the shared draws, the calls logged as its voice's
class OscPortParts final : public Vc2OscParts {
public:
    OscPortParts(int voice, OscShared& shared, Vc2OscVoice& self) : voice_(voice), s_(shared), self_(self) {}
    void buildVibrato() override { log(kBuildVibrato); }
    void updateVibrato(int32_t n) override { log(kUpdateVibrato, (uint32_t)n); }
    void setVibrato(const uint32_t* settings) override { log(kSetVibrato, offsetOf(settings)); }
    void noteOnVibrato(const uint8_t* block) override { log(kNoteOnVibrato, wordOf(block)); }
    float vibratoPitch() override { return s_.draws.vibratoPitch; }
    float vibratoLevel() override { return s_.draws.vibratoLevel; }
    void buildRamp(int ramp, int32_t type) override { log(kBuildRamp, (uint32_t)ramp, (uint32_t)type); }
    void tickRamp(int ramp, int32_t n) override { log(kTickRamp, (uint32_t)ramp, (uint32_t)n); }
    void startRamp(int ramp, float a, float b) override { logFloats(kStartRamp, ramp, a, b); }
    void setRamp(int ramp, float a, float b) override { logFloats(kSetRamp, ramp, a, b); }
    void legatoRamp(int ramp, uint32_t x) override { log(kLegatoRamp, (uint32_t)ramp, x); }
    float rampValue(int ramp) override;
    float rampFactor(int ramp) override { return s_.draws.rampFactors[(size_t)ramp]; }
    void kindCall(uint32_t routine) override { log(kKindCall, routine); }
    void kindSettings(uint32_t routine, const uint32_t* from) override { log(kKindSettings, routine, offsetOf(from)); }
    void setCarrier(const uint32_t* p) override { log(kSetCarrier, offsetOf(p)); }
    void initCarrier(const uint8_t* p) override { log(kInitCarrier, wordOf(p)); }
    void startKind6(const uint8_t* p) override { log(kStartKind6, wordOf(p)); }
    uint32_t renderBy(uint32_t routine, float* out, uint32_t third) override;
    void renderCarrier(float* out) override { log(kRenderCarrier, outOf(out)); }
    void renderKind6(float* out) override { log(kRenderKind6, outOf(out)); }
    void seek(uint32_t routine, int32_t at) override;
    float boundFloat(uint32_t part, uint32_t offset) override;
    Vc2OscWaveHeader waveHeader(uint32_t at) override;
    Vc2OscLoopRecord loopRecord(uint32_t at) override;

private:
    void log(OscKind kind, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
        s_.calls.push_back({ kind, voice_, a, b, c });
    }
    void logFloats(OscKind kind, int ramp, float a, float b);
    uint32_t offsetOf(const uint32_t* p) const { return (uint32_t)(p - s_.settings.data()) * 4; }
    uint32_t wordOf(const uint8_t* p) const { return (uint32_t)(p - self_.own.data()) + Vc2OscVoice::kOwn; }
    uint32_t outOf(const float* p) const { return p == s_.out.data() ? 1 : kOscOther; }
    int voice_;
    OscShared& s_;
    Vc2OscVoice& self_;
};

// rom.4's side: every routine the voice calls hooked to log into `log` and answer from `draws`
void oscInstallRom(Vc2Fw& fw, const OscDraws& draws, std::vector<OscCall>& log);
// The records written into rom.4's memory where the voice's words point
void oscWriteRecords(Vc2Fw& fw, const OscRecords& r);
// The first call the two logs differ at; empty when they don't
std::string oscCallsDifference(const std::vector<OscCall>& rom, const std::vector<OscCall>& port);
uint32_t oscVoiceAt(int voice);
uint32_t oscRenderAt(int k);   // a hooked render routine for the tail's words
