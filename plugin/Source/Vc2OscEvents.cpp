// xt_vc2osc's events (v230): each of the OSC voice's routines on one voice of both sides, rom.4's then the port's, and
// what differs after: the voices' words, the calls and the answer; the getters', the targets' and the kind's note-on's
// in Vc2OscGetters.cpp (v232).  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2OscSides.h"
#include <bit>

namespace {
// [FW] the voice's routines
constexpr uint32_t kConstruct = 0x893ae070, kInstall = 0x893ae0f0, kUpdate = 0x893ae1c0, kReseek = 0x893aeed0;
constexpr uint32_t kNoteOn = 0x893ae370, kStart = 0x893ae610, kCount = 0x893ae6c0, kCountMatched = 0x893ae720;
constexpr uint32_t kSettings = 0x893ae780, kLegato = 0x893ae9b0, kRender = 0x893ae9f0, kTake = 0x893aee20;
// [FW] the block's words a note-on's draw steers: its kind, its byte +4, the wave's type, the records (its floats by
// oscBlockFloats)
constexpr int kKindWord = 0, kByteWord = 1, kWaveWord = 0xb0 / 4, kHeaderWord = 0xd4 / 4;
constexpr int kLoopWord = 0xdc / 4, kFixedWord = 0xe4 / 4;
// [FW] v238: VariPhrase's note-on's +0xe0 record, and its range's start and where it seeks to (the block's +0xe8, +0xf0)
constexpr int kPairWord = 0xe0 / 4, kFromWord = 0xe8 / 4, kSeekToWord = 0xf0 / 4;
constexpr uint32_t kFixedBit = 0x1000;   // the byte +0x138's 0x10, in its word
constexpr int kKinds = 8, kWaves = 10;
constexpr uint32_t kCountSign = 0x8000;   // [FW] the update's count is its low half, signed (exts.w)
using Answer = OscAnswer;

std::string onConstruct(OscSides& s, std::mt19937&, int v) {
    return oscBothSides(s, kConstruct, { oscVoiceAt(v) }, Answer::r0, [&] {
        s.voices[(size_t)v].construct(s.parts[(size_t)v]);
        return oscVoiceAt(v);   // the constructor answers its voice
    });
}

std::string onInstall(OscSides& s, std::mt19937& rng, int v) {
    // v238: one time in three a player's kind (LITE's and VariPhrase's own installs, by the wave's type), one in six
    // a kind past 7 (the frame's default row, which clears the flag), one in six the analog (v248: its own install by
    // the code and the sub oscillator's switch), else the kind as the event finds it
    const uint32_t pick = rng() % 6;
    if (pick < 2) oscSteerPlayers(s, rng, v);
    if (pick == 2) {
        s.voices[(size_t)v].setU32(Vc2OscVoice::kKind, kKinds + rng() % 4);
        oscWriteVoice(s, v);
    }
    if (pick == 3) oscMakeAnalog(s, rng, v);
    else oscSteerAnalog(s, rng, v);   // a kind-3 voice found: its words so (a settings' copy leaves noise at +0x138)
    return oscBothSides(s, kInstall, { oscVoiceAt(v) }, Answer::none, [&] {
        s.voices[(size_t)v].install(s.parts[(size_t)v]);
        return 0u;
    });
}

std::string onUpdate(OscSides& s, std::mt19937& rng, int v) {
    // now and then high bits, which it drops; its low half never negative (v236: the ramps' tick reads past the factor's
    // table on a negative count, which the tone never gives)
    const uint32_t n = rng() % 3 == 0 ? (uint32_t)rng() & ~kCountSign : rng() % 64;
    // v248: one time in four the analog (its own update); a kind-3 voice's words drawn as its converter could give
    // them, as a note-on's are (noise there could be NaNs, whose FTRC and difference the interpreter takes otherwise)
    if (rng() % 4 == 0) oscMakeAnalog(s, rng, v);
    else oscSteerAnalog(s, rng, v);
    ++s.n.updates;
    return oscBothSides(s, kUpdate, { oscVoiceAt(v), n }, Answer::none, [&] {
        s.voices[(size_t)v].update((int32_t)n, s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}

std::string onReseek(OscSides& s, std::mt19937&, int v) {
    return oscBothSides(s, kReseek, { oscVoiceAt(v) }, Answer::none, [&] {
        s.voices[(size_t)v].reseek(s.parts[(size_t)v]);
        return 0u;
    });
}

// A note-on's block: noise, its kind 0…7 (past 7 rom.4 reads from address 0), the wave's type, the depth, the records
std::array<uint32_t, kOscBlockWords> drawNoteBlock(std::mt19937& rng) {
    std::array<uint32_t, kOscBlockWords> b {};
    for (uint32_t& w : b) w = (uint32_t)rng();
    oscBlockFloats(b, rng);   // the depth (+0x1c) among them
    b[kKindWord] = rng() % kKinds;
    b[kWaveWord] = rng() % 8 == 0 ? (uint32_t)rng() : rng() % kWaves;
    b[kHeaderWord] = oscHeaderAt((int)(rng() % kOscRecords));
    b[kLoopWord] = oscLoopAt((int)(rng() % kOscRecords));
    b[kPairWord] = oscPairAt((int)(rng() % kOscRecords));   // v238: VariPhrase's note-on reads its +0x2d0
    if (rng() % 2) b[kSeekToWord] = b[kFromWord];             // v238: no seek when the third is the first
    if (rng() % 2) b[kByteWord] = 0;
    if (rng() % 2) b[kFixedWord] &= ~kFixedBit;
    constexpr uint32_t kAnalog = 3;   // v242: the analog's block words, as its converter could give them
    if (b[kKindWord] == kAnalog) {
        for (const auto& [at, bits] : oscAnalogWords(rng)) b[(at - Vc2OscVoice::kOwn) / 4] = bits;
    }
    return b;
}

// v240: LITE's two heads (kind 1, the wave's type 2) take their period from the root's Hz, so their note-on reads a
// real wave's records: at 0 Hz the walk's start is 0 × ∞, a NaN, whose FTRC xtf takes otherwise than the interpreter
void steerTwoHeads(OscSides& s, std::mt19937& rng, std::array<uint32_t, kOscBlockWords>& b) {
    if (b[kKindWord] != 1 || b[kWaveWord] != 2) return;
    oscRealWave(s.shared.records, 0, rng);
    oscWriteRecords(s.fw, s.shared.records);
    b[kHeaderWord] = oscHeaderAt(0);
    b[kLoopWord] = oscLoopAt(0);
}

std::string onNoteOn(OscSides& s, std::mt19937& rng, int v) {
    std::array<uint32_t, kOscBlockWords> block = drawNoteBlock(rng);
    steerTwoHeads(s, rng, block);
    constexpr uint32_t kAnalog = 3;   // v244: the analog's object words, no NaN for its sub oscillator's note-on
    if (block[kKindWord] == kAnalog) {
        oscAnalogObject(s.voices[(size_t)v], rng);
        oscWriteVoice(s, v);
    }
    const uint32_t legato = rng() % 3 == 0 ? (uint32_t)rng() : rng() % 2;
    s.fw.cpu.write(kOscBlockAt, block.data(), sizeof block);
    ++s.n.noteOns;
    const uint32_t kind = block[kKindWord];
    s.n.sampled += kind == 1 || kind == 2;
    s.n.plain += kind != 1 && kind != 2;
    return oscBothSides(s, kNoteOn, { oscVoiceAt(v), kOscBlockAt, legato }, Answer::none, [&] {
        s.voices[(size_t)v].noteOn(block.data(), legato, s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}

// The six words a ramp's start or legato update is given, drawn into rom.4's RAM (v236: the ramps read them)
const uint32_t* drawRampWords(OscSides& s, std::mt19937& rng) {
    const Vc2RampSettings words = oscDrawRampSettings(rng);
    std::memcpy(s.shared.ramp.data(), &words, sizeof words);
    s.fw.cpu.write(kOscRampAt, s.shared.ramp.data(), sizeof s.shared.ramp);
    return s.shared.ramp.data();
}

std::string onStart(OscSides& s, std::mt19937& rng, int v) {
    const uint32_t* ramp = drawRampWords(s, rng);
    return oscBothSides(s, kStart, { oscVoiceAt(v), kOscRampAt }, Answer::none, [&] {
        s.voices[(size_t)v].start(ramp, s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}

std::string onCount(OscSides& s, std::mt19937& rng, int v) {
    const uint32_t x = rng() % 2 ? rng() % 8 : (uint32_t)rng();
    const bool matched = rng() % 2;
    return oscBothSides(s, matched ? kCountMatched : kCount, { oscVoiceAt(v), x }, Answer::none, [&] {
        if (matched) s.voices[(size_t)v].countMatched(x);
        else s.voices[(size_t)v].count(x);
        return 0u;
    });
}

std::string onSettings(OscSides& s, std::mt19937& rng, int v) {
    for (uint32_t& w : s.shared.settings) w = (uint32_t)rng();
    oscBlockFloats(s.shared.settings, rng);
    auto* bytes = reinterpret_cast<uint8_t*>(s.shared.settings.data());
    if (rng() % 2) bytes[4] = s.voices[(size_t)v].u8(0x58);   // the byte +4 as the voice holds it, half the time
    s.fw.cpu.write(kOscSettingsAt, s.shared.settings.data(), sizeof s.shared.settings);
    ++s.n.settings;
    return oscBothSides(s, kSettings, { oscVoiceAt(v), kOscSettingsAt }, Answer::none, [&] {
        s.voices[(size_t)v].settings(s.shared.settings.data(), s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}

std::string onLegato(OscSides& s, std::mt19937& rng, int v) {
    const uint32_t* ramp = drawRampWords(s, rng);
    return oscBothSides(s, kLegato, { oscVoiceAt(v), kOscRampAt }, Answer::none, [&] {
        s.voices[(size_t)v].legato(ramp);
        return 0u;
    });
}

std::string onRender(OscSides& s, std::mt19937& rng, int v) {
    for (float& x : s.shared.out) x = oscDrawFloat(rng);
    s.fw.cpu.write(kOscOutAt, s.shared.out.data(), sizeof s.shared.out);
    ++s.n.renders;
    std::string what = oscBothSides(s, kRender, { oscVoiceAt(v), kOscOutAt }, Answer::r0, [&] {
        return s.voices[(size_t)v].render(s.shared.out.data(), s.parts[(size_t)v]);
    });
    if (!what.empty()) return what;
    return vc2FirstDifference(s.fw, kOscOutAt, s.shared.out.data(), sizeof s.shared.out, "the output");
}

std::string onTake(OscSides& s, std::mt19937&, int v) {
    const int other = (v + 1) % kOscVoices;
    ++s.n.takes;
    return oscBothSides(s, kTake, { oscVoiceAt(v), oscVoiceAt(other) + Vc2OscVoice::kObject }, Answer::none, [&] {
        s.voices[(size_t)v].take(s.voices[(size_t)other]);
        return 0u;
    });
}

const OscEvent kEvents[] = { onConstruct, onInstall, onUpdate, onUpdate, onReseek, onNoteOn, onNoteOn, onStart,
                             onCount, onSettings, onLegato, onRender, onRender, onTake, oscOnKindNoteOn,
                             oscOnGetter, oscOnGetter, oscOnGetter, oscOnGetter };
} // namespace

std::span<const OscEvent> oscEvents() { return kEvents; }
#endif
