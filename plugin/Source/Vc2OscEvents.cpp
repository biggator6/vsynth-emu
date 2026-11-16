// xt_vc2osc's events (v230): each of the OSC voice's routines on one voice of both sides, rom.4's then the port's, and
// what differs after: the voices' words, the calls and the answer.  XT_VC2OSC_MAIN only.
#ifdef XT_VC2OSC_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2OscSides.h"
#include <bit>

namespace {
// [FW] the voice's routines
constexpr uint32_t kConstruct = 0x893ae070, kInstall = 0x893ae0f0, kUpdate = 0x893ae1c0, kReseek = 0x893aeed0;
constexpr uint32_t kNoteOn = 0x893ae370, kStart = 0x893ae610, kCount = 0x893ae6c0, kCountMatched = 0x893ae720;
constexpr uint32_t kSettings = 0x893ae780, kLegato = 0x893ae9b0, kRender = 0x893ae9f0, kTake = 0x893aee20;
// [FW] the block's words a note-on's draw steers: its kind, its byte +4, the depth, the wave's type, the records
constexpr int kKindWord = 0, kByteWord = 1, kDepthWord = 0x1c / 4, kWaveWord = 0xb0 / 4, kSampleWord = 0xd4 / 4;
constexpr int kMapWord = 0xdc / 4, kFixedWord = 0xe4 / 4;
constexpr uint32_t kFixedBit = 0x1000;   // the byte +0x138's 0x10, in its word
constexpr int kKinds = 8, kWaves = 10;

enum class Answer : uint8_t { none, r0, fr0 };

// rom.4's `routine` on `args`, then the port's `mine()` (giving its answer's bits), then what differs, the answers
// compared as `how` says
template <class Mine>
std::string bothSides(OscSides& s, uint32_t routine, std::initializer_list<uint32_t> args, Answer how, Mine mine) {
    if (!s.fw.call(routine, args)) return "rom.4's routine faults: " + s.fw.cpu.error();
    const uint32_t theirs = how == Answer::fr0 ? s.fw.cpu.fb[0][0] : s.fw.cpu.r[0];
    const uint32_t ours = mine();
    std::string what = oscDifference(s);
    if (!what.empty() || how == Answer::none || theirs == ours) return what;
    char t[96];
    std::snprintf(t, sizeof t, "the answer: %08x, against %08x", ours, theirs);
    return t;
}

std::string onConstruct(OscSides& s, std::mt19937&, int v) {
    return bothSides(s, kConstruct, { oscVoiceAt(v) }, Answer::r0, [&] {
        s.voices[(size_t)v].construct(s.parts[(size_t)v]);
        return oscVoiceAt(v);   // the constructor answers its voice
    });
}

std::string onInstall(OscSides& s, std::mt19937&, int v) {
    return bothSides(s, kInstall, { oscVoiceAt(v) }, Answer::none, [&] {
        s.voices[(size_t)v].install(s.parts[(size_t)v]);
        return 0u;
    });
}

std::string onUpdate(OscSides& s, std::mt19937& rng, int v) {
    const uint32_t n = rng() % 3 == 0 ? (uint32_t)rng() : rng() % 64;   // now and then high bits, which it drops
    ++s.n.updates;
    return bothSides(s, kUpdate, { oscVoiceAt(v), n }, Answer::none, [&] {
        s.voices[(size_t)v].update((int32_t)n, s.parts[(size_t)v]);
        return 0u;
    });
}

std::string onReseek(OscSides& s, std::mt19937&, int v) {
    return bothSides(s, kReseek, { oscVoiceAt(v) }, Answer::none, [&] {
        s.voices[(size_t)v].reseek(s.parts[(size_t)v]);
        return 0u;
    });
}

// A note-on's block: noise, its kind 0…7 (past 7 rom.4 reads from address 0), the wave's type, the depth, the records
std::array<uint32_t, kOscBlockWords> drawNoteBlock(std::mt19937& rng) {
    std::array<uint32_t, kOscBlockWords> b {};
    for (uint32_t& w : b) w = (uint32_t)rng();
    b[kKindWord] = rng() % kKinds;
    b[kWaveWord] = rng() % 8 == 0 ? (uint32_t)rng() : rng() % kWaves;
    b[kDepthWord] = std::bit_cast<uint32_t>(oscDrawFloat(rng));
    b[kSampleWord] = oscSampleAt((int)(rng() % kOscRecords));
    b[kMapWord] = oscMapAt((int)(rng() % kOscRecords));
    if (rng() % 2) b[kByteWord] = 0;
    if (rng() % 2) b[kFixedWord] &= ~kFixedBit;
    return b;
}

std::string onNoteOn(OscSides& s, std::mt19937& rng, int v) {
    const std::array<uint32_t, kOscBlockWords> block = drawNoteBlock(rng);
    const uint32_t legato = rng() % 3 == 0 ? (uint32_t)rng() : rng() % 2;
    s.fw.cpu.write(kOscBlockAt, block.data(), sizeof block);
    ++s.n.noteOns;
    const uint32_t kind = block[kKindWord];
    s.n.sampled += kind == 1 || kind == 2;
    s.n.plain += kind != 1 && kind != 2;
    return bothSides(s, kNoteOn, { oscVoiceAt(v), kOscBlockAt, legato }, Answer::none, [&] {
        s.voices[(size_t)v].noteOn(block.data(), legato, s.parts[(size_t)v], s.romShared);
        return 0u;
    });
}

std::string onStart(OscSides& s, std::mt19937& rng, int v) {
    return bothSides(s, kStart, { oscVoiceAt(v), (uint32_t)rng() }, Answer::none, [&] {
        s.voices[(size_t)v].start(s.parts[(size_t)v]);
        return 0u;
    });
}

std::string onCount(OscSides& s, std::mt19937& rng, int v) {
    const uint32_t x = rng() % 2 ? rng() % 8 : (uint32_t)rng();
    const bool matched = rng() % 2;
    return bothSides(s, matched ? kCountMatched : kCount, { oscVoiceAt(v), x }, Answer::none, [&] {
        if (matched) s.voices[(size_t)v].countMatched(x);
        else s.voices[(size_t)v].count(x);
        return 0u;
    });
}

std::string onSettings(OscSides& s, std::mt19937& rng, int v) {
    for (uint32_t& w : s.shared.settings) w = (uint32_t)rng();
    auto* bytes = reinterpret_cast<uint8_t*>(s.shared.settings.data());
    if (rng() % 2) bytes[4] = s.voices[(size_t)v].u8(0x58);   // the byte +4 as the voice holds it, half the time
    s.fw.cpu.write(kOscSettingsAt, s.shared.settings.data(), sizeof s.shared.settings);
    ++s.n.settings;
    return bothSides(s, kSettings, { oscVoiceAt(v), kOscSettingsAt }, Answer::none, [&] {
        s.voices[(size_t)v].settings(s.shared.settings.data(), s.parts[(size_t)v]);
        return 0u;
    });
}

std::string onLegato(OscSides& s, std::mt19937& rng, int v) {
    const uint32_t x = (uint32_t)rng();
    return bothSides(s, kLegato, { oscVoiceAt(v), x }, Answer::none, [&] {
        s.voices[(size_t)v].legato(x, s.parts[(size_t)v]);
        return 0u;
    });
}

std::string onRender(OscSides& s, std::mt19937& rng, int v) {
    for (float& x : s.shared.out) x = oscDrawFloat(rng);
    s.fw.cpu.write(kOscOutAt, s.shared.out.data(), sizeof s.shared.out);
    ++s.n.renders;
    std::string what = bothSides(s, kRender, { oscVoiceAt(v), kOscOutAt }, Answer::r0, [&] {
        return s.voices[(size_t)v].render(s.shared.out.data(), s.parts[(size_t)v]);
    });
    if (!what.empty()) return what;
    return vc2FirstDifference(s.fw, kOscOutAt, s.shared.out.data(), sizeof s.shared.out, "the output");
}

std::string onTake(OscSides& s, std::mt19937&, int v) {
    const int other = (v + 1) % kOscVoices;
    ++s.n.takes;
    return bothSides(s, kTake, { oscVoiceAt(v), oscVoiceAt(other) + Vc2OscVoice::kObject }, Answer::none, [&] {
        s.voices[(size_t)v].take(s.voices[(size_t)other]);
        return 0u;
    });
}

// The port's answer of a getter, as its bits: a float getter alone or given the parts, a whole one
template <float (Vc2OscVoice::*Get)() const>
uint32_t alone(const Vc2OscVoice& o, Vc2OscParts&) {
    return std::bit_cast<uint32_t>((o.*Get)());
}
template <float (Vc2OscVoice::*Get)(Vc2OscParts&) const>
uint32_t asking(const Vc2OscVoice& o, Vc2OscParts& p) {
    return std::bit_cast<uint32_t>((o.*Get)(p));
}
template <class Whole, Whole (Vc2OscVoice::*Get)() const>
uint32_t whole(const Vc2OscVoice& o, Vc2OscParts&) {
    return (uint32_t)(o.*Get)();
}

// The position getter's own draws, half the time: kind 1 or 2, the wave's type 0…2, the float position about its
// bounds (that path is a kind, a type and a value at once, too rare among the event's other draws)
void steerPosition(Vc2OscVoice& o, std::mt19937& rng) {
    constexpr uint32_t kKind = 0x54, kWave = 0x104, kPositionFloat = 0x270;   // [FW]
    if (rng() % 2) return;
    o.setU32(kKind, 1 + rng() % 2);
    o.setU32(kWave, rng() % 3);
    o.setF32(kPositionFloat, oscDrawPosition(rng));
}

// [FW] the getters, each with the port's: its address, whether it answers in fr0, the port's answer's bits, and its
// own draws (none for most)
struct Getter {
    uint32_t at;
    bool inFr0;
    uint32_t (*mine)(const Vc2OscVoice&, Vc2OscParts&);
    void (*steer)(Vc2OscVoice&, std::mt19937&) = nullptr;
};
constexpr Getter kGetters[] = {
    { 0x893aead0, true, alone<&Vc2OscVoice::formant> }, { 0x893aeb00, true, alone<&Vc2OscVoice::second> },
    { 0x893aeb20, true, asking<&Vc2OscVoice::pitch> }, { 0x893aecc0, true, asking<&Vc2OscVoice::kindFormant> },
    { 0x893aecf0, true, asking<&Vc2OscVoice::kindSecond> }, { 0x893aed10, true, alone<&Vc2OscVoice::level> },
    { 0x893aed40, false, whole<int32_t, &Vc2OscVoice::word168> },
    { 0x893aed60, false, whole<uint32_t, &Vc2OscVoice::position>, steerPosition },
};

std::string onGetter(OscSides& s, std::mt19937& rng, int v) {
    const Getter& g = kGetters[rng() % std::size(kGetters)];
    ++s.n.getters;
    if (g.steer) {
        g.steer(s.voices[(size_t)v], rng);
        oscWriteVoice(s, v);
    }
    return bothSides(s, g.at, { oscVoiceAt(v) }, g.inFr0 ? Answer::fr0 : Answer::r0,
                     [&] { return g.mine(s.voices[(size_t)v], s.parts[(size_t)v]); });
}

const OscEvent kEvents[] = { onConstruct, onInstall, onUpdate, onUpdate, onReseek, onNoteOn, onNoteOn, onStart, onCount,
                             onSettings, onLegato, onRender, onRender, onTake, onGetter, onGetter, onGetter };
} // namespace

std::span<const OscEvent> oscEvents() { return kEvents; }
#endif
