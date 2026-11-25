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
inline constexpr uint32_t kOscPairsAt = 0x8C05e000;      // the records the block's +0xe0 points at (v238)
inline constexpr uint32_t kOscPairWord = 0x2d0;          // [FW] the word VariPhrase's note-on reads of one
inline constexpr uint32_t kOscBlockWords = Vc2OscVoice::kBlockBytes / 4, kOscOther = 0xff;

inline constexpr uint32_t kOscKindPAt = 0x8C05f000;      // P as a case's start gives the carrier and kind 6 (v266)
inline constexpr uint32_t kOscMicAt = 0x8C05f800;        // a block of the mic on its way into the ring (v266)
inline constexpr uint32_t kOscFramesAt = 0x8C057000;     // the effect chip's frames the EXT IN player plays (v322)
inline constexpr int kOscFrameWords = 32 * 8;            // [FW] a block's 32 frames of 8 words
inline constexpr uint32_t kOscWaveAt = 0x8C0B0000;       // the wave Vari off's renders play (v323)
inline constexpr uint32_t kOscWaveHalfwords = 16384;
// v332: the band players' waves, after it in the same wave memory, up to the events' table's region (0x8C0C0000)
inline constexpr uint32_t kOscBandAt = kOscWaveAt + kOscWaveHalfwords * 2, kOscBandBytes = 0x8000;
inline constexpr uint32_t kOscEventsAt = 0x8C0C8000;     // the events' table mode 1 walks (v327; v328 32 KB up)
// v334: SOLO's waves, above the interpreter's stack (kVc2Sp), in the wave memory the port's parts hold from kOscWaveAt
// (the events' table's region and the stack's between, which no port reads through the wave memory).  v335: 64 KB
// above the stack's top, which nothing writes: the period-jump player's read behind a wave's first sample reads what
// is below its image, there 0 on both sides (the stack's words rom.4's alone)
inline constexpr uint32_t kOscSoloAt = 0x8C0F0000, kOscSoloBytes = 0x8000;
// v335: the period-jump player's waves (its family's, OscBandWave::periods), 64 KB above SOLO's and 64 KB below the
// wave memory's end, zero on both sides: its read lags and leads the walk by up to about a wave
inline constexpr uint32_t kOscJumpAt = 0x8C108000, kOscJumpBytes = 0x8000;
inline constexpr uint32_t kOscWaveBytes = kOscJumpAt + kOscJumpBytes + 0x10000 - kOscWaveAt;

// v330: a kind's routine and its settings by address went, the last VariPhrase's; a player's set-up came back.  v331:
// the event lookup went, the voice's own
enum OscKind : int32_t { kRenderBy, kOscKinds };   // v335: the seeks and object calls went, the voice's own

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
// v332: a band wave in the wave memory (Vc2OscBandDraws.cpp): where it lies and its room, mono or stereo, its frames
// and events, and the addresses a note's head gives: the vpdt, the cached loop events, the loop record, the header
// and each band's data
struct OscBandWave {
    uint32_t at = 0, room = 0;
    bool stereo = false, looped = false;
    bool solo = false;   // v334: SOLO's layout (one band, the events SOLO's), its band 0 the samples
    int frames = 0, events = 0;
    uint32_t vpdt = 0, cached = 0, loop = 0, header = 0;
    std::array<uint32_t, 6> bands {};
    std::vector<int32_t> positions {};   // v333: its events' (×16), which the band part's seeks are drawn about
    // v335: in SOLO's layout, its records periods alone (16…215 samples, none brief, short or of two entries), the
    // period-jump player's: its read keeps within a period of the walk, so a period past half the wave or the glide
    // to a far longer one below the record takes its reads out of the wave, where rom.4 reads other memory
    bool periods = false;
};
// v332: an object as rom.4's set-up of a band player left it, BACKING's or ENSEMBLE's; from v334 any player's by the
// wave's type (BACKING 3, ENSEMBLE 5, SOLO 1 and 6)
struct OscBandState {
    uint32_t type = 0;
    std::array<uint8_t, Vc2OscVoice::kObjectBytes> object {};
};
uint32_t oscVoiceAt(int voice);
// The records' addresses and draws (Vc2OscRecords.cpp, v238; v230's, moved): the headers, loop records, envelopes,
// LFOs and the block's +0xe0 records
uint32_t oscHeaderAt(int k);
uint32_t oscLoopAt(int k);
Vc2OscLoopRecord oscLoopFrom(const uint8_t* bytes);   // v333: one from its bytes, as a band wave holds its own
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

// What the hooked routines answer, drawn once for both sides.  The ramps are the voice's own from v236, the vibrato
// from v267, the event lookup from v331, the players' seeks from v335 (the flags byte a seek left went with them)
struct OscDraws {
    uint32_t rendered = 0;
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
    // v323: what rom.4 has at kOscWaveAt; from v332 the band waves above Vari off's (kOscBandAt), from v334 SOLO's
    // (kOscSoloAt)
    std::vector<int16_t> wave = std::vector<int16_t>(kOscWaveBytes / 2);
    // v332: the band players' draws, from a generator of their own seeded from the run's, and the states rom.4's own
    // set-ups left on the run's two band waves before any hook, which a render through a band player is steered to
    std::mt19937 bandRng {};
    std::vector<OscBandState> bandStates {};
    long bandSteers = 0;   // the renders steered so
    // v333: those two band waves, which a note-on's block of a band player is given; the note-ons and re-seeks steered
    std::vector<OscBandWave> bandWaves {};
    long bandSetUps = 0, bandSeeks = 0;
    // v334: SOLO's the same, from a generator of its own (so the band players' draws stay as they were); its states
    // among bandStates by their type
    std::mt19937 soloRng {};
    std::vector<OscBandWave> soloWaves {};
    long soloSteers = 0, soloSetUps = 0, soloSeeks = 0;
    // v357: LITE's heads' samples (oscSteerRenders) one time in four at SOLO's waves, not Vari off's, so a head reads
    // past the wave memory's first quarter, as before v334 grew it: a generator of its own, so the renders' draws stay
    std::mt19937 headsRng {};
    // v335: the period-jump player's the same, its waves of periods in kOscJumpAt's room, so SOLO's draws stay
    std::mt19937 jumpRng {};
    std::vector<OscBandWave> jumpWaves {};
    long jumpSteers = 0, jumpSetUps = 0, jumpSeeks = 0;
    // v337: the analog's renders' generator (Vc2OscAnalogRenderDraws.cpp), the blocks through one of its 19 renders,
    // their words drawn finite, the NOISE holds steered and the blocks turned to the check's renders, their words no XT's
    std::mt19937 analogRng {};
    long analogBlocks = 0, analogFinite = 0, analogHolds = 0, analogOutside = 0;
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
    uint32_t renderBy(uint32_t routine, float* out, uint32_t third) override;
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

private:
    void log(OscKind kind, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
        s_.calls.push_back({ kind, voice_, a, b, c });
    }
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
