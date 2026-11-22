// xt_vc2tone's part for the real OSC voices (v319): see Vc2ToneSides.h.  rom.4's own voices run in its tones, what they
// call beyond the tone hooked by xt_vc2osc's table (Vc2OscFakes.cpp) and answered by its fake parts on the port's side
// (Vc2OscPortParts.cpp), each call on its voice, tone × 2 + its OSC.  What a voice is given is drawn as xt_vc2osc draws
// it (Vc2OscDraws), so rom.4's routines meet what its converters could write.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2OscDraws.h"
#include "Vc2ToneSides.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace {
using In = Vc2ToneInput;
static_assert(sizeof(Vc2OscVoice) == Vc2OscVoice::kSize, "the voice compared whole, rom.4's 0xb84 bytes");
constexpr uint32_t kKind = 0x54, kWave = 0x104;   // [FW] the voice's kind and wave's type
constexpr uint32_t kAnalog = 3, kKind6 = 6, kKinds = 8;
constexpr uint32_t kTwoHeads = 1, kTwoHeadsWave = 2;   // [FW] LITE's two heads: kind 1, the wave's type 2 (v240)
// [FW] the block's wave's type and records (an analog's words and binding lie over the records: v242)
constexpr uint32_t kWaveWord = 0xb0 / 4, kHeaderWord = 0xd4 / 4, kLoopWord = 0xdc / 4, kPairWord = 0xe0 / 4;
constexpr uint32_t kLite = 1, kVari = 2;   // [FW] the kinds whose note-on reads the block's records
constexpr uint32_t kEnvelopes = Vc2Tone::kEnvelopes, kLfos = Vc2Tone::kLfos;
constexpr size_t kBinds = 4;
// [FW] the OSC block's words 0x893b3390's binding reads by the OSC's kind (its switch on +0): four envelopes' numbers,
// then four LFOs'; 0 is the kind's own word, which kind 6 reads for its second envelope and its LFOs; the kinds not
// here bind none
struct ToneOscBinds {
    uint32_t kind;
    std::array<uint32_t, 2 * kBinds> at;
};
constexpr ToneOscBinds kBindsByKind[] = {
    { 1, { 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58 } },
    { 2, { 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58 } },
    { 3, { 0x20, 0xbc, 0xd4, 0x38, 0x40, 0xc4, 0xd8, 0x58 } },
    { 5, { 0x20, 0xec, 0x100, 0x38, 0x40, 0xf0, 0x104, 0x58 } },
    { 6, { 0x20, 0, 0x30, 0x38, 0, 0, 0, 0 } },
};

// The mic ring's sides of xt_vc2tone's two
OscRingSides toneRingSides(ToneSides& s) {
    return { s.fw, s.shared.osc.ring, s.shared.osc.unvoiced, s.shared.osc.draws.clock };
}

uint32_t* oscBlockIn(In& in, int osc) { return &in.at(In::kOscs[(size_t)osc]); }

// The block's bound parts' numbers, its kind's: each a part of the tone, as the converters write them
void bindNumbersOfKind(In& in, int osc, std::mt19937& rng) {
    uint32_t* b = oscBlockIn(in, osc);
    for (const ToneOscBinds& row : kBindsByKind) {
        if (row.kind != b[0]) continue;
        for (size_t k = 0; k < row.at.size(); ++k) {
            if (row.at[k] != 0) b[row.at[k] / 4] = (uint32_t)(rng() % (k < kBinds ? kEnvelopes : kLfos));
        }
    }
}

// LITE's two heads take their period from the root's Hz, so their note-on reads a real wave's records (v240's steer)
void headsOnRealWave(ToneSides& s, In& in, int osc, std::mt19937& rng) {
    uint32_t* b = oscBlockIn(in, osc);
    if (b[0] != kTwoHeads || b[kWaveWord] != kTwoHeadsWave) return;
    oscRealWave(s.shared.osc.records, 0, rng);
    oscWriteRecords(s.fw, s.shared.osc.records);
    b[kHeaderWord] = oscHeaderAt(0);
    b[kLoopWord] = oscLoopAt(0);
}

// [FW] a pitch the targets sum to 0 cents at a ratio's scale of 1 (Vc2OscTargets): the block's words (each the voice's
// word 0x54 below) the pitch, its first envelope's depth (taken × 0.2), where the pitch ramp starts from and the
// vibrato's depth 0, the ratio's scale 1
void neutralPitch(std::span<uint32_t> words) {
    constexpr uint32_t kPitch = 0x5c, kPitchDepth = 0x70, kRampFrom = 0xbc, kVibratoDepth = 0xfc, kRatioScale = 0x60;
    const auto word = [&](uint32_t at) -> uint32_t& { return words[(at - Vc2OscVoice::kOwn) / 4]; };
    for (uint32_t at : { kPitch, kPitchDepth, kRampFrom, kVibratoDepth }) word(at) = 0;
    word(kRatioScale) = std::bit_cast<uint32_t>(1.0f);
}

// An analog's code given after its words (oscAnalogWords keeps Fat's sum finite for the code it draws alone): on an LA
// code its sub oscillator off, as 2^Fat's sum would be ∞ there, and ∞ × a phase of 0 a NaN that LA's end subtracts
// (v246), whose sign xtf::sub negates where FSUB keeps it
bool laWithSub(uint32_t code, float subOn) {
    constexpr uint32_t kLaFirst = 9, kLaCodes = 2;   // [FW] LA-SQUARE's and LA-SAW's codes
    return code - kLaFirst < kLaCodes && std::trunc(subOn) != 0.0f;
}
constexpr uint32_t kSubOn = 0x138;   // [FW] the sub oscillator's switch, a float

// [FW] a wave's type or code as its converter writes it, those the kind's install has a render for: LITE's 0…2,
// VariPhrase's 1, 3, 5, 6 and 8 (0x893af7a0's, 0x893af870's), the analog's codes 0…10 (0x893c2ad0's; xt_vc2osc's draws
// pass them now and then, for its routines' defaults).  On the others an install writes none, and rom.4 would render
// through the word the voice's last use left, 0 in a fresh one
constexpr std::array<uint32_t, 5> kVariTypes { 1, 3, 5, 6, 8 };
void installedWave(In& in, int osc, std::mt19937& rng) {
    constexpr uint32_t kLiteTypes = 3, kCodes = 11;
    uint32_t* b = oscBlockIn(in, osc);
    if (b[0] == kLite) b[kWaveWord] = rng() % kLiteTypes;
    if (b[0] == kVari) b[kWaveWord] = kVariTypes[rng() % kVariTypes.size()];
    if (b[0] != kAnalog || b[kWaveWord] < kCodes) return;
    b[kWaveWord] = rng() % kCodes;
    uint32_t& subOn = b[(kSubOn - Vc2OscVoice::kOwn) / 4];
    if (laWithSub(b[kWaveWord], std::bit_cast<float>(subOn))) subOn = 0;
}

// A player's block's records, as oscDrawNoteBlock points them
void pointAtRecords(In& in, int osc, std::mt19937& rng) {
    uint32_t* b = oscBlockIn(in, osc);
    if (b[0] != kLite && b[0] != kVari) return;
    b[kHeaderWord] = oscHeaderAt((int)(rng() % kOscRecords));
    b[kLoopWord] = oscLoopAt((int)(rng() % kOscRecords));
    b[kPairWord] = oscPairAt((int)(rng() % kOscRecords));
}

Vc2OscVoice& oscOfTone(ToneSides& s, int i, int osc) { return s.parts[(size_t)i].oscs[(size_t)osc]; }

void writeToneOsc(ToneSides& s, int i, int osc) {
    const Vc2OscVoice& v = oscOfTone(s, i, osc);
    s.fw.cpu.write(toneOscAt(s.pool.addressOf(i), osc), &v, sizeof v);
}
} // namespace

std::string toneOscDifference(ToneSides& s, int i) {
    const uint32_t tone = s.pool.addressOf(i);
    const Vc2OscVoice& one = oscOfTone(s, i, 0);
    const Vc2OscVoice& two = oscOfTone(s, i, 1);
    return vc2RegionsDifference(s.fw, { { toneOscAt(tone, 0), &one, sizeof one, "OSC 1's voice" },
                                        { toneOscAt(tone, 1), &two, sizeof two, "OSC 2's voice" } });
}

std::string toneOscCallsDifference(ToneSides& s) {
    if (s.shared.math->faulted()) return "rom.4's shared routines fault for the port";
    return oscCallsDifference(s.oscRom, s.shared.osc.calls);
}

void toneDrawOscs(ToneSides& s, std::mt19937& rng) {
    for (const OscCall& c : s.oscRom) {   // the last event's, before its log goes
        if ((size_t)c.kind < s.n.oscCalls.size()) ++s.n.oscCalls[(size_t)c.kind];
    }
    oscDrawAnswers(s.fw, s.shared.osc, rng);   // v321: the input stage's peak among them
    s.oscRom.clear();
}

void toneDrawOscRamp(In& in, std::mt19937& rng) {
    const Vc2RampSettings words = oscDrawRampSettings(rng);
    std::memcpy(&in.at(In::kOscSet), &words, sizeof words);
}

// The records as a real wave's (oscRealWave: the converter's rates, the header's cents and the loop's tune within an
// octave): xt_vc2osc's far tunes and cents take rom.4's exp2 past its tables, where it reads RAM a tone's note-on has
// written by the time the port's call reads it
void toneDrawOscBlocks(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    oscDrawRecords(s.shared.osc.records, rng);
    oscDrawLoopEvents(s.shared.osc.records, s.shared.osc.renders);   // v326
    for (int k = 0; k < kOscRecords; ++k) oscRealWave(s.shared.osc.records, k, rng);
    for (int osc = 0; osc < 2; ++osc) {
        const std::array<uint32_t, kOscBlockWords> block = oscDrawNoteBlock(rng);
        std::copy(block.begin(), block.end(), oscBlockIn(in, osc));
        installedWave(in, osc, rng);
        bindNumbersOfKind(in, osc, rng);
        headsOnRealWave(s, in, osc, rng);
    }
    // v324: with LITE's update its own, a tone's two OSC updates call out only on VariPhrase, so the order of the two
    // shows only where both are; one note-on in four makes them so, from the renders' generator (the run's draws stay)
    std::mt19937& renders = s.shared.osc.renders;
    if (renders() % 4 == 0) {
        for (int osc = 0; osc < 2; ++osc) {
            toneOscBlockOfKind(s, osc, kVari, kVariTypes[renders() % kVariTypes.size()], renders);
        }
        ++s.n.bothVari;
    }
    oscWriteRecords(s.fw, s.shared.osc.records);
    toneDrawOscRamp(in, rng);
}

void toneOscBlockOfKind(ToneSides& s, int osc, uint32_t kind, uint32_t wave, std::mt19937& rng) {
    In& in = s.shared.input;
    uint32_t* b = oscBlockIn(in, osc);
    b[0] = kind;
    if (kind == kAnalog) {
        for (const auto& [at, bits] : oscAnalogWords(rng)) b[(at - Vc2OscVoice::kOwn) / 4] = bits;
    }
    b[kWaveWord] = wave;   // the analog's code among its words
    uint32_t& subOn = b[(kSubOn - Vc2OscVoice::kOwn) / 4];
    if (kind == kAnalog && laWithSub(wave, std::bit_cast<float>(subOn))) subOn = 0;
    if (oscIsVoiced(kind)) oscDrawVoicedWords({ b, kOscBlockWords }, rng, Vc2OscVoice::kOwn);
    pointAtRecords(in, osc, rng);
    headsOnRealWave(s, in, osc, rng);
    bindNumbersOfKind(in, osc, rng);
}

void toneOscSteady(ToneSides& s, int osc, uint32_t wave, std::mt19937& rng) {
    constexpr uint32_t kExtIn = 4;   // [FW] its root the constant 440 Hz, its update a kind's routine by address
    toneOscBlockOfKind(s, osc, kExtIn, wave, rng);
    neutralPitch({ oscBlockIn(s.shared.input, osc), kOscBlockWords });
    s.fw.cpu.write(kToneInputAt, s.shared.input.words.data(), sizeof s.shared.input.words);
}

// Bound to the tone's own envelopes and LFOs, as a note's binding leaves it, and its install on both sides too, as an
// analog note's install pass leaves it (its renders' words); what differs shows in the event's compare
void toneMakeOscAnalog(ToneSides& s, int i, int osc, uint32_t code, std::mt19937& rng) {
    constexpr uint32_t kInstallAt = 0x893ae0f0;   // [FW] the voice's install
    constexpr uint32_t kCodeCopy = 0x248;         // [FW] the note-on's copy of the code, which the install reads
    Vc2OscVoice& v = oscOfTone(s, i, osc);
    for (const auto& [at, bits] : oscAnalogWords(rng)) v.setU32(at, bits);
    v.setU32(kKind, kAnalog);
    v.setU32(kWave, code);
    v.setU32(kCodeCopy, code);
    if (laWithSub(code, v.f32(kSubOn))) v.setF32(kSubOn, 0.0f);
    oscAnalogObject(v, rng);
    const uint32_t tone = s.pool.addressOf(i);
    for (size_t k = 0; k < kBinds; ++k) {
        v.tail.binds[k] = toneEnvelopeAt(tone, (int)(rng() % kEnvelopes));
        v.tail.binds[kBinds + k] = toneLfoAt(tone, (int)(rng() % kLfos));
    }
    writeToneOsc(s, i, osc);
    if (s.fw.call(kInstallAt, { toneOscAt(s.pool.addressOf(i), osc) })) s.parts[(size_t)i].installOsc(osc);
}

void toneOscBeforeNoteOn(ToneSides& s, int i, std::mt19937& rng) {
    for (int osc = 0; osc < 2; ++osc) {
        const uint32_t kind = oscBlockIn(s.shared.input, osc)[0];
        if (kind < kKinds) ++s.n.oscKinds[kind];
        if (kind != kAnalog) continue;
        oscAnalogObject(oscOfTone(s, i, osc), rng);   // v244: no NaN in the words the sub oscillator's note-on reads
        writeToneOsc(s, i, osc);
    }
}

void toneHoldOscVibratos(ToneSides& s, int i, int32_t n) {
    for (int osc = 0; osc < 2; ++osc) {
        Vc2VibratoWords& w = oscOfTone(s, i, osc).vibrato.w;
        oscHoldVibrato(w, (int16_t)n);   // v267: a count this long can't overshoot its fade
        s.fw.cpu.write(toneOscAt(s.pool.addressOf(i), osc), &w, sizeof w);
    }
}

void toneDrawOscReads(ToneSides& s, int i, std::mt19937& rng) {
    // [FW] the word 0x893aed40 reads, as a halfword, on kind 2; the positions 0x893aed60 reads: kind 2's and type 1's
    // samples ×16, type 0's, type 2's float (v321: renders advance them; a getter of a number past the OSCs' told the
    // voices apart only by chance)
    constexpr uint32_t kHalf = 0x168, kLite = 1, kVariPhrase = 2;
    constexpr uint32_t kPosition = 0x194, kPositionLite = 0x24c, kPositionFloat = 0x270;
    for (int osc = 0; osc < 2; ++osc) {
        Vc2OscVoice& v = oscOfTone(s, i, osc);
        if (v.kind() != kLite && v.kind() != kVariPhrase) continue;
        if (v.kind() == kVariPhrase) v.setU32(kHalf, (uint32_t)rng());
        if (rng() % 2) {
            v.setU32(kPosition, (uint32_t)rng());
            v.setU32(kPositionLite, (uint32_t)rng());
            v.setF32(kPositionFloat, oscDrawPosition(rng));
        }
        writeToneOsc(s, i, osc);
    }
}

std::string toneFeedMic(ToneSides& s, int i, std::mt19937& rng) {
    if (oscOfTone(s, i, 0).kind() != kKind6 && oscOfTone(s, i, 1).kind() != kKind6) return "";
    ++s.n.micBlocks;   // v266: the ring goes on under kind 6, as the block mixer feeds it
    return oscRingBlock(toneRingSides(s), rng);
}

void toneSteerRenders(ToneSides& s, int i) {
    bool extIn = false, variOff = false, heads = false;
    for (int osc = 0; osc < 2; ++osc) {
        Vc2OscVoice& v = oscOfTone(s, i, osc);
        extIn = extIn || oscRendersBy(v, kOscExtInRenders);
        variOff = variOff || oscRendersBy(v, kOscVariOffRenders);
        heads = heads || oscRendersBy(v, kOscHeadsRenders);   // v326
        oscSteerRenders(v, s.shared.osc);
        writeToneOsc(s, i, osc);
    }
    s.n.extInBlocks += extIn;
    s.n.variOffBlocks += variOff;
    s.n.headsBlocks += heads;
}

void toneDrawOscSettings(ToneSides& s, int i, uint32_t osc, std::mt19937& rng) {
    std::array<uint32_t, kOscBlockWords>& settings = s.shared.osc.settings;
    const int which = osc < 2 ? (int)osc : 0;
    Vc2OscVoice& v = oscOfTone(s, i, which);
    oscDrawSettingsBlock(settings, rng, v.u8(kOscBlockByte));
    if (osc < 2 && v.kind() == kAnalog) {   // v248: its settings read the analog's words, as its converter gives them
        for (const auto& [at, bits] : oscAnalogWords(rng)) settings[(at - Vc2OscVoice::kOwn) / 4] = bits;
    }
    if (osc < 2) {   // v324: LITE's settings read the loop record through the head its note-on leaves
        const uint32_t at = toneSettingsVoice(s, i, which);   // v328: a take's head kept
        s.n.liteSettings += oscSteerLiteSettings(v, settings, at, s.shared.osc.renders);
        writeToneOsc(s, i, which);
    }
    s.fw.cpu.write(kOscSettingsAt, settings.data(), sizeof settings);
    s.n.oscSettings += osc < 2;
}

std::string toneOscSetUp(ToneSides& s) {
    oscFindZeroTunes(s.fw, s.shared.osc.records);
    return oscRingSetUp(toneRingSides(s));
}

std::string toneOscCase(ToneSides& s, std::mt19937& rng) {
    oscDrawWave(s.fw, s.shared.osc);   // v323: the wave Vari off's renders play
    return oscRingCase(toneRingSides(s), rng);
}
#endif
