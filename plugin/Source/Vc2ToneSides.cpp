// xt_vc2tone's sides: their compare and what each event draws on both, see Vc2ToneSides.h (v228, v229).
// XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include <algorithm>
#include <cstring>

namespace {
using In = Vc2ToneInput;
// [FW] the pool's words
constexpr uint32_t kHeadAt = 0x8993fb48;
constexpr uint32_t kSerialAt = 0x8993fb4c;
// [FW] the LFOs' constructors' generator; a tone's own words by offset: the tail, the pool's link
constexpr uint32_t kChaosAt = 0x89999668;
constexpr uint32_t kTailAt = 0x1af8;
constexpr uint32_t kLinkAt = 0x2618;
constexpr int kListed = 3;                     // the most tones the lists hold
constexpr uint32_t kNoSerial = 0x7ffffff0;      // a serial no tone of a case reaches
constexpr uint32_t kFixMix = 4, kFixKind = 3, kFixWave = 10, kFixedWave = 2;   // [FW] the note-on's fix-up's case

bool drawFlag(std::mt19937& rng) { return rng() % 2 == 0; }
float drawFloat(std::mt19937& rng) { return 4.0f * vc2Unit(rng) - 2.0f; }
uint32_t drawBits(std::mt19937& rng) { return std::bit_cast<uint32_t>(drawFloat(rng)); }

// The first part word rom.4's memory and the port's mirror differ at, as text; empty when none
std::string partWordsDifference(const ToneWords& rom, const ToneWords& mine) {
    std::array<uint32_t, sizeof(ToneWords) / sizeof(uint32_t)> a {}, b {};
    std::memcpy(a.data(), &rom, sizeof rom);
    std::memcpy(b.data(), &mine, sizeof mine);
    const auto [x, y] = std::ranges::mismatch(a, b);
    if (x == a.end()) return "";
    char t[96];
    std::snprintf(t, sizeof t, "its part words, word %d: %08x, against %08x", (int)(x - a.begin()), *y, *x);
    return t;
}

// Tone i's own words and part words against rom.4's
std::string toneWordsDifference(ToneSides& s, int i) {
    const Vc2Tone& t = s.pool.tones[(size_t)i];
    const uint32_t at = s.pool.addressOf(i);
    std::string what = vc2RegionsDifference(s.fw, { { at, &t.head, sizeof t.head, "its head" },
                                                    { at + kTailAt, &t.tail, sizeof t.tail, "its tail" },
                                                    { at + kLinkAt, &t.link, sizeof t.link, "its link" } });
    if (what.empty()) what = toneModulatorsDifference(s, i);
    if (what.empty()) what = toneMixDifference(s, i);
    if (what.empty()) what = toneTvaDifference(s, i);
    if (what.empty()) what = partWordsDifference(toneReadRom(s.fw, at), s.parts[(size_t)i].words);
    return what.empty() ? what : "tone " + std::to_string(i) + ", " + what;
}

ToneWords drawWords(std::mt19937& rng) {
    ToneWords w;
    for (ToneOscWords& o : w.osc) {
        o.kind = rng() % 8;
        o.flag = drawFlag(rng) ? rng() % 2 : (uint32_t)rng();
        o.value = drawBits(rng);
        o.other = drawBits(rng);
        o.byte = rng() % 256;
        o.wave = rng() % 16;
        std::ranges::generate(o.binds, [&] { return (uint32_t)rng(); });
        o.cost = drawBits(rng);
    }
    w.cosmCost = drawBits(rng);
    return w;
}

// The input: every word small (each a usable envelope's or LFO's number), then the words the note-on decides by
void drawToneInput(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    std::ranges::generate(in.words, [&] { return rng() % 8; });
    in.at(In::kStructure) = rng() % 5 == 0 ? (uint32_t)rng() : rng() % 4;
    toneDrawModulatorBlocks(in, rng);
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) in.at(In::kLfos + In::kLfoStride * l + In::kKeySync) = rng() % 2;
    for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = rng() % 2;
    for (uint32_t block : In::kCosms) in.at(block) = rng() % 23;
    toneDrawMixBlock(in, rng);
    in.at(In::kTva) = rng() % Vc2Tone::kEnvelopes;
    toneDrawTvaBlock(in, rng);
    if (rng() % 6 == 0) {   // the fix-up's case
        in.at(In::kMix) = kFixMix;
        in.at(In::kOscs[1]) = kFixKind;
        in.at(In::kOscs[1] + In::kWave) = kFixWave;
    }
}

// The fix-up's case against a legato: the earlier tone's OSC 2 of the fix-up's kind, at the wave before it or after it,
// so that whether the note-on fixes the input before its OSCs' note-ons decides whether the legato takes the player
void drawFixedLegato(ToneSides& s, int from, std::mt19937& rng) {
    In& in = s.shared.input;
    ToneWords& was = s.parts[(size_t)from].words;
    was.osc[1].kind = kFixKind;
    was.osc[1].wave = drawFlag(rng) ? kFixWave : kFixedWave;
    toneWriteRom(s.fw, s.pool.addressOf(from), was);
    in.at(In::kMix) = kFixMix;
    in.at(In::kOscs[1]) = kFixKind;
    in.at(In::kOscs[1] + In::kWave) = kFixWave;
    in.at(In::kOscs[1] + In::kLegato) = 1;
}

// The legato's tone: none, a tone made (its OSCs' kind and +0xb0 matched now and then), or a serial no tone has (with
// the legato's switches off, or rom.4 would read from address 0)
void drawPrevious(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    const uint32_t pick = rng() % 3;
    in.at(In::kPrevious) = pick == 0 ? (uint32_t)-1 : kNoSerial;
    if (pick == 1) {
        const int from = s.live[rng() % s.live.size()];
        in.at(In::kPrevious) = (uint32_t)s.pool.tones[(size_t)from].head.serial;
        for (size_t o = 0; o < In::kOscs.size(); ++o) {
            if (rng() % 2 != 0) continue;
            const ToneOscWords& was = s.parts[(size_t)from].words.osc[o];
            in.at(In::kOscs[o]) = was.kind;
            in.at(In::kOscs[o] + In::kWave) = was.wave;
        }
        if (rng() % 4 == 0) drawFixedLegato(s, from, rng);
    }
    if (pick == 2) {
        for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = 0;
    }
}

// The tones the note-on's walk back reaches, up to three (the part's real lists from v273): now and then one of them
// with tone i's id, and each node's +0x14 itself, now and then 0 (passed over) or another tone
void drawListedWords(ToneSides& s, int i, std::mt19937& rng) {
    const std::array<uint32_t, 2> marks = s.shared.lists.marks();
    std::vector<int> listed;
    uint32_t node = marks[0] != 0 ? marks[0] : marks[1];
    for (; node != 0 && s.pool.holds(node) && listed.size() < kListed; node = s.pool.at(node).head.prev) {
        listed.push_back(s.pool.indexOf(node));
    }
    if (!listed.empty() && drawFlag(rng)) {
        const int j = listed[rng() % listed.size()];
        toneBothWord(s, j, kToneIdAt, s.pool.tones[(size_t)j].head.id, s.pool.tones[(size_t)i].head.id);
    }
    for (int t : listed) {
        const uint32_t pick = rng() % 6;
        const int other = s.live[rng() % s.live.size()];
        const uint32_t self = pick == 0 ? 0 : s.pool.addressOf(pick == 1 ? other : t);
        toneBothWord(s, t, kToneSelfAt, s.pool.tones[(size_t)t].head.self, self);
    }
}
} // namespace

std::string toneDifference(ToneSides& s, int only) {
    const Vc2TonePool& p = s.pool;
    const float& chaos = s.shared.chaos;
    std::string what = vc2RegionsDifference(s.fw, { { kHeadAt, &p.head, sizeof p.head, "the pool's head" },
                                                    { kSerialAt, &p.serial, sizeof p.serial, "the serial" },
                                                    { kChaosAt, &chaos, sizeof chaos, "the LFOs' generator" } });
    for (int i = 0; i < Vc2TonePool::kTones && what.empty(); ++i) {
        if (only < 0 || i == only) what = toneWordsDifference(s, i);
    }
    const auto& in = s.shared.input.words;
    if (what.empty()) what = vc2FirstDifference(s.fw, kToneInputAt, in.data(), sizeof in, "the input");
    if (what.empty()) what = toneListsDifference(s);
    if (what.empty() && only < 0) what = toneFreeLfosDifference(s);   // the events that can reach the manager
    return what.empty() ? toneCallsDifference(s.rom, s.shared.calls) : what;
}

void tonePrepare(ToneSides& s, int i, std::mt19937& rng) {
    ToneDraws& d = s.shared.draws;
    std::ranges::generate(d.installOsc, [&] { return (uint32_t)rng(); });
    std::ranges::generate(d.installCosm, [&] { return drawFlag(rng) ? rng() % 2 : (uint32_t)rng(); });
    std::ranges::generate(d.word168, [&] { return (uint32_t)rng(); });
    std::ranges::generate(d.position, [&] { return (uint32_t)rng(); });
    s.parts[(size_t)i].words = drawWords(rng);
    toneWriteRom(s.fw, s.pool.addressOf(i), s.parts[(size_t)i].words);
    toneDrawModulatorWords(s, i, rng);
    toneDrawTva(s, i, rng);
    s.rom.clear();
    s.shared.calls.clear();
    ++s.n.events;
}

void tonePrepareNoteOn(ToneSides& s, int i, std::mt19937& rng) {
    tonePrepare(s, i, rng);
    drawToneInput(s, rng);
    drawPrevious(s, rng);
    s.fw.cpu.write(kToneInputAt, s.shared.input.words.data(), sizeof s.shared.input.words);
    drawListedWords(s, i, rng);
}
#endif
