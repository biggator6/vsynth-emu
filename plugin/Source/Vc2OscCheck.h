#pragma once
// xt_vc2osc's shared parts (v230, v232): the calls each side logs, what the hooked routines answer and the records the
// voice reads (drawn once for both sides), the port's fake parts, and rom.4's side: every routine the OSC voice calls
// hooked.  From v319 xt_vc2tone's too, whose tones' real voices call the same.  XT_VC2OSC_MAIN or XT_VC2TONE_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2OscVoice.h"
#include <array>
#include <bit>
#include <cstring>
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
inline constexpr uint32_t kOscRampAt = 0x8C05d000;       // the six words a ramp's start or legato update reads (v236)
// [FW] the pitch ramp's value and factor in a voice (+0x90c + 0x3c, + 0x54), which a kind's call logs as it finds them
inline constexpr uint32_t kOscRampValueAt = 0x948, kOscRampFactorAt = 0x960;
inline constexpr uint32_t kOscPairsAt = 0x8C05e000;      // the records the block's +0xe0 points at (v238)
inline constexpr uint32_t kOscPairWord = 0x2d0;          // [FW] the word VariPhrase's note-on reads of one
inline constexpr uint32_t kOscBlockWords = Vc2OscVoice::kBlockBytes / 4, kOscOther = 0xff;

inline constexpr uint32_t kOscKindPAt = 0x8C05f000;      // P as a case's start gives the carrier and kind 6 (v266)
inline constexpr uint32_t kOscMicAt = 0x8C05f800;        // a block of the mic on its way into the ring (v266)
inline constexpr uint32_t kOscFramesAt = 0x8C057000;     // the effect chip's frames the EXT IN player plays (v322)
inline constexpr int kOscFrameWords = 32 * 8;            // [FW] a block's 32 frames of 8 words
inline constexpr uint32_t kOscWaveAt = 0x8C0B0000;       // the wave Vari off's renders play (v323)
inline constexpr uint32_t kOscWaveHalfwords = 16384;
inline constexpr uint32_t kOscEventsAt = 0x8C0C8000;     // the events' table mode 1 walks (v327; v328 32 KB up)

enum OscKind : int32_t { kKindCall, kKindSettings, kRenderBy, kSeek, kEventPosition, kOscKinds };

// A call as either side made it: what, on which voice, and its arguments (a ramp's number, counts, floats' bits, a
// routine's address, a settings word's offset in the settings' block or a word's in the voice, the output as 1, a
// seek's object byte 0)
struct OscCall {
    int32_t kind, voice;
    uint32_t a, b, c;
    bool operator==(const OscCall&) const = default;
};

// v327: the events' table mode 1 walks, at kOscEventsAt: its events' count, stride and first half word (the object's
// +0x8, +0x7) and each event's position ×16; the region about it as rom.4's memory holds it, from `from`.  From v328
// laid out as mode 1's set-up lays it out, by the header's format (`wide`) and the samples' frame (`stereo`)
struct OscEventTable {
    int count = 0;
    uint8_t stride = 0, first = 0;
    bool wide = false, stereo = false;
    std::vector<int32_t> positions {};
    uint32_t from = 0;
    std::vector<uint8_t> bytes {};
    bool holds(uint32_t at) const { return at - from < bytes.size(); }
    uint32_t wordAt(uint32_t at) const {
        uint32_t w = 0;
        std::memcpy(&w, bytes.data() + (at - from), sizeof w);
        return w;
    }
};
// The records the voice reads at the addresses its words name: the wave headers and loop records (Vc2OscVoice.h's
// offsets), and the bound envelopes' and LFOs' words by address (from v327 the events' table's too)
struct OscRecords {
    std::array<Vc2OscWaveHeader, kOscRecords> headers {};
    std::array<Vc2OscLoopRecord, kOscRecords> loops {};
    std::map<uint32_t, float> bound {};
    // v240: the octaves n past exp2's table whose word rom.4 reads there is 0 or a NaN (Vc2OscRecords.cpp's drawTune)
    std::vector<int32_t> zeroTunes {};
    OscEventTable events {};
};
uint32_t oscVoiceAt(int voice);
// The records' addresses and draws (Vc2OscRecords.cpp, v238; v230's, moved): the headers, loop records, envelopes,
// LFOs and the block's +0xe0 records
uint32_t oscHeaderAt(int k);
uint32_t oscLoopAt(int k);
uint32_t oscEnvelopeAt(int k);
uint32_t oscLfoAt(int k);
uint32_t oscPairAt(int k);
void oscDrawRecords(OscRecords& r, std::mt19937& rng);
// v326: each +0xe0 record's four loop events' positions, ×16, which the beat's block reads through the object's +0x6c:
// two loops of 64…863 frames from frame 3000…3499, about the heads' (oscDrawHeadsWords); from the renders' generator.
// From v327 also the four events' numbers and the two loops' modes before each position
void oscDrawLoopEvents(OscRecords& r, std::mt19937& rng);
// Its zeroTunes, read from rom.4's memory once at the start
void oscFindZeroTunes(Vc2Fw& fw, OscRecords& r);
// v240: header and loop record `k` drawn again as a real wave's: a rate the converter gives, its cents and the tune
// within an octave, so the note-on's root Hz is finite and above 0
void oscRealWave(OscRecords& r, int k, std::mt19937& rng);

// What the hooked routines answer, drawn once for both sides; the flags byte a seek leaves in the object (a player may
// write it); the event lookup's positions, each side taking the next (v238).  The ramps are the voice's own from v236,
// the vibrato from v267
struct OscDraws {
    uint32_t rendered = 0;
    uint8_t seekFlags = 0;
    std::array<int32_t, 3> positions {};
    uint32_t aside = 0;          // what the lookup leaves in its second word, which its caller drops
    mutable size_t romTaken = 0;
    int32_t clock = 0;           // the OS's millisecond word a carrier's init reads (v266)
    float peak = 0.0f;           // the input stage's peak (0x899d9a5c), which kind 5's update reads (v321)
    std::array<int32_t, kOscFrameWords> frames {};   // the effect chip's frames, the EXT IN player's (v322)
};

// What the port's parts share: the answers, the records, the blocks' words, the output and the calls; the mic ring and
// the unvoiced decision kind 6 reads (v266)
struct OscShared {
    OscDraws draws {};
    // v322: the renders' draws (the EXT IN player's frames and level, from v323 Vari off's wave and words) from a
    // generator of their own, so the run's other draws stay as they were; seeded from the run's.  `extIn` until v323
    std::mt19937 renders {};
    std::vector<int16_t> wave = std::vector<int16_t>(kOscWaveHalfwords);   // v323: what rom.4 has at kOscWaveAt
    size_t positionsTaken = 0;   // the port's side of the event lookup's positions (v238)
    Vc2MicRing ring {};
    std::array<float, Vc2Kind6::kBlock> unvoiced {};
    OscRecords records {};
    std::array<uint32_t, kOscBlockWords> settings {};
    std::array<uint32_t, sizeof(Vc2RampSettings) / 4> ramp {};
    std::array<float, Vc2OscVoice::kOut> out {};
    std::vector<OscCall> calls {};
};

// A voice's port parts: answers from the shared draws, the calls logged as its voice's
class OscPortParts final : public Vc2OscParts {
public:
    OscPortParts(int voice, OscShared& shared, Vc2OscVoice& self) : voice_(voice), s_(shared), self_(self) {}
    void kindCall(uint32_t routine) override {   // with the pitch ramp's value and factor, as rom.4's hook logs them
        const Vc2RampWords& pitch = self_.ramps[Vc2OscVoice::kPitchRamp].w;
        log(kKindCall, routine, std::bit_cast<uint32_t>(pitch.value), std::bit_cast<uint32_t>(pitch.factor));
    }
    void kindSettings(uint32_t routine, const uint32_t* from) override { log(kKindSettings, routine, offsetOf(from)); }
    uint32_t renderBy(uint32_t routine, float* out, uint32_t third) override;
    void seek(uint32_t routine, int32_t at) override;
    int32_t osClock() override { return s_.draws.clock; }
    Vc2MicRing& micRing() override { return s_.ring; }
    const float* unvoiced() override { return s_.unvoiced.data(); }
    float inputPeak() override { return s_.draws.peak; }
    const int32_t* inputFrames() override { return s_.draws.frames.data(); }
    Vc2OscSamples waveSamples() override { return { s_.wave.data(), kOscWaveAt, (uint32_t)s_.wave.size() }; }
    float boundFloat(uint32_t part, uint32_t offset) override;
    Vc2OscWaveHeader waveHeader(uint32_t at) override;
    Vc2OscLoopRecord loopRecord(uint32_t at) override;
    uint32_t address() override { return oscVoiceAt(voice_); }
    int32_t eventPosition(uint32_t event) override;

private:
    void log(OscKind kind, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
        s_.calls.push_back({ kind, voice_, a, b, c });
    }
    uint32_t offsetOf(const uint32_t* p) const { return (uint32_t)(p - s_.settings.data()) * 4; }
    uint32_t outOf(const float* p) const { return p == s_.out.data() ? 1 : kOscOther; }
    int voice_;
    OscShared& s_;
    Vc2OscVoice& self_;
};

// rom.4's side: every routine the voice calls hooked to log into `log` and answer from `draws`, each call on the voice
// `voiceOf` numbers by its address (−1 none): xt_vc2osc's two, oscVoiceNumber, or from v319 xt_vc2tone's 48
void oscInstallRom(Vc2Fw& fw, const OscDraws& draws, std::vector<OscCall>& log, int (*voiceOf)(uint32_t));
int oscVoiceNumber(uint32_t at);
// The records written into rom.4's memory where the voice's words point
void oscWriteRecords(Vc2Fw& fw, const OscRecords& r);
// The first call the two logs differ at; empty when they don't
std::string oscCallsDifference(const std::vector<OscCall>& rom, const std::vector<OscCall>& port);
uint32_t oscRenderAt(int k);   // a hooked render routine for the tail's words
