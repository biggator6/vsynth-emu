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

// Tone i's own words and parts' words against rom.4's
std::string toneWordsDifference(ToneSides& s, int i) {
    const Vc2Tone& t = s.pool.tones[(size_t)i];
    const uint32_t at = s.pool.addressOf(i);
    std::string what = vc2RegionsDifference(s.fw, { { at, &t.head, sizeof t.head, "its head" },
                                                    { at + kTailAt, &t.tail, sizeof t.tail, "its tail" },
                                                    { at + kLinkAt, &t.link, sizeof t.link, "its link" } });
    if (what.empty()) what = toneModulatorsDifference(s, i);
    if (what.empty()) what = toneMixDifference(s, i);
    if (what.empty()) what = toneTvaDifference(s, i);
    if (what.empty()) what = toneCosmDifference(s, i);
    if (what.empty()) what = toneOscDifference(s, i);
    return what.empty() ? what : "tone " + std::to_string(i) + ", " + what;
}

// The input: every word small (each a usable envelope's or LFO's number), then the words the note-on decides by; the
// OSC blocks (v319) before the modulators', whose LFOs' tempo is OSC 1's block's +0x60
void drawToneInput(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    std::ranges::generate(in.words, [&] { return rng() % 8; });
    in.at(In::kStructure) = rng() % 5 == 0 ? (uint32_t)rng() : rng() % 4;
    toneDrawOscBlocks(s, rng);
    toneDrawModulatorBlocks(in, rng);
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) in.at(In::kLfos + In::kLfoStride * l + In::kKeySync) = rng() % 2;
    for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = rng() % 2;
    toneDrawCosmBlocks(in, rng);
    toneDrawMixBlock(in, rng);
    in.at(In::kTva) = rng() % Vc2Tone::kEnvelopes;
    toneDrawTvaBlock(in, rng);
    if (rng() % 6 == 0) {   // the fix-up's case: its kind half the time, else one whose type 10 the note-on must leave
        // [FW] kinds whose install takes no wave's type but kinds 4's and 7's flag at type 1 (Vc2OscVoiceKinds.cpp)
        constexpr std::array<uint32_t, 3> kOtherKinds { 0, 4, 7 };
        in.at(In::kMix) = kFixMix;
        const uint32_t kind = drawFlag(rng) ? kFixKind : kOtherKinds[rng() % kOtherKinds.size()];
        toneOscBlockOfKind(s, 1, kind, kFixWave, rng);
    }
}

// The fix-up's case against a legato: the earlier tone's OSC 2 of the fix-up's kind, at the wave before it or after it,
// so that whether the note-on fixes the input before its OSCs' note-ons decides whether the legato takes the player;
// from v319 that voice made an analog one on both sides, as its note would have left it
void drawFixedLegato(ToneSides& s, int from, std::mt19937& rng) {
    In& in = s.shared.input;
    toneMakeOscAnalog(s, from, 1, drawFlag(rng) ? kFixWave : kFixedWave, rng);
    in.at(In::kMix) = kFixMix;
    toneOscBlockOfKind(s, 1, kFixKind, kFixWave, rng);
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
            const Vc2OscVoice& was = s.parts[(size_t)from].oscs[o];   // v319: the real voice, its kind's words drawn
            toneOscBlockOfKind(s, (int)o, was.kind(), was.u32(Vc2OscVoice::kWave), rng);
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
    if (what.empty()) what = tonePoolsDifference(s);
    if (what.empty() && only < 0) what = toneFreeLfosDifference(s);   // the events that can reach the manager
    if (what.empty()) what = toneCallsDifference(s.rom, s.shared.calls);
    return what.empty() ? toneOscCallsDifference(s) : what;
}

void tonePrepare(ToneSides& s, int i, std::mt19937& rng) {
    toneDrawOscs(s, rng);
    toneDrawModulatorWords(s, i, rng);
    toneDrawTva(s, i, rng);
    toneDrawCosms(s, i, rng);
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
