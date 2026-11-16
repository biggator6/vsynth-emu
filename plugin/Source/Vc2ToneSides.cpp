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
// [FW] a tone's own words by offset: the tail, the envelopes' and LFOs' numbers, the pool's link; a free object's float
constexpr uint32_t kTailAt = 0x1af8;
constexpr uint32_t kEnvelopeNumber = 4;
constexpr uint32_t kLfoNumber = 0x34;
constexpr uint32_t kLinkAt = 0x2618;
constexpr uint32_t kFreeValue = 0x10;
constexpr int kListed = 3;                      // the most tones the lists hold
constexpr uint32_t kNoSerial = 0x7ffffff0;      // a serial no tone of a case reaches
constexpr uint32_t kFixMix = 4, kFixKind = 3, kFixWave = 10;   // [FW] the note-on's fix-up's case

bool drawFlag(std::mt19937& rng) { return rng() % 2 == 0; }
float drawFloat(std::mt19937& rng) { return 4.0f * vc2Unit(rng) - 2.0f; }
uint32_t drawBits(std::mt19937& rng) { return std::bit_cast<uint32_t>(drawFloat(rng)); }

// A part's numbers (the envelopes' or the LFOs') against rom.4's words at the part's address + `offset`
template <size_t N>
std::string numbersDifference(Vc2Fw& fw, uint32_t tone, uint32_t (*partAt)(uint32_t, int), uint32_t offset,
                              const std::array<int32_t, N>& mine, const char* what) {
    for (size_t k = 0; k < N; ++k) {
        std::string differs = vc2FirstDifference(fw, partAt(tone, (int)k) + offset, &mine[k], sizeof(int32_t), what);
        if (!differs.empty()) return differs;
    }
    return "";
}

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
    if (what.empty()) what = numbersDifference(s.fw, at, toneEnvelopeAt, kEnvelopeNumber, t.envelopeIndex, "a number");
    if (what.empty()) what = numbersDifference(s.fw, at, toneLfoAt, kLfoNumber, t.lfoIndex, "an LFO's number");
    if (what.empty()) what = partWordsDifference(toneReadRom(s.fw, at), s.parts[(size_t)i].words);
    return what.empty() ? what : "tone " + std::to_string(i) + ", " + what;
}

ToneWords drawWords(std::mt19937& rng) {
    ToneWords w;
    std::ranges::generate(w.envelopeOn, [&] { return rng() % 2; });
    std::ranges::generate(w.lfoOn, [&] { return rng() % 2; });
    std::ranges::generate(w.lfoFollow, [&] { return drawFlag(rng) ? 0u : (uint32_t)rng(); });
    std::ranges::generate(w.lfoListed, [&] { return rng() % 2; });
    for (ToneOscWords& o : w.osc) {
        o.kind = rng() % 8;
        o.flag = drawFlag(rng) ? rng() % 2 : (uint32_t)rng();
        o.value = drawBits(rng);
        o.other = drawBits(rng);
        o.byte = rng() % 256;
        o.wave = rng() % 16;
        std::ranges::generate(o.binds, [&] { return (uint32_t)rng(); });
    }
    constexpr uint32_t kZeros[] = { 0x00000000, 0x80000000 };   // the level test's edge, +0 and −0
    const uint32_t pick = rng() % 8;
    w.tvaLevel = pick < 2 ? kZeros[pick] : (pick < 4 ? (uint32_t)rng() : std::bit_cast<uint32_t>(vc2Unit(rng) - 0.5f));
    std::ranges::generate(w.tvaBinds, [&] { return (uint32_t)rng(); });
    return w;
}

// The input: every word small (each a usable envelope's or LFO's number), then the words the note-on decides by
void drawToneInput(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    std::ranges::generate(in.words, [&] { return rng() % 8; });
    in.at(In::kStructure) = rng() % 5 == 0 ? (uint32_t)rng() : rng() % 4;
    in.at(In::kLfoValue) = drawBits(rng);
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) in.at(In::kLfos + In::kLfoStride * l + In::kKeySync) = rng() % 2;
    for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = rng() % 2;
    for (uint32_t block : In::kCosms) in.at(block) = rng() % 23;
    in.at(In::kMix) = rng() % 6;
    in.at(In::kTva) = rng() % Vc2Tone::kEnvelopes;
    if (rng() % 6 == 0) {   // the fix-up's case
        in.at(In::kMix) = kFixMix;
        in.at(In::kOscs[1]) = kFixKind;
        in.at(In::kOscs[1] + In::kWave) = kFixWave;
    }
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
    }
    if (pick == 2) {
        for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = 0;
    }
}

// The two lists: up to three tones made, split between the first and the second (either may be empty), each list linked
// through its tones' +4; now and then one of them with tone i's id
void buildLists(ToneSides& s, int i, std::mt19937& rng) {
    std::vector<int> listed = s.live;
    std::shuffle(listed.begin(), listed.end(), rng);
    listed.resize(std::min(listed.size(), (size_t)(rng() % (kListed + 1))));
    if (!listed.empty() && drawFlag(rng)) {
        const int j = listed[rng() % listed.size()];
        toneBothWord(s, j, kToneIdAt, s.pool.tones[(size_t)j].head.id, s.pool.tones[(size_t)i].head.id);
    }
    const size_t split = listed.empty() ? 0 : rng() % (listed.size() + 1);   // the first list's tones, then the second's
    for (size_t k = 0; k < listed.size(); ++k) {   // a node's +0x14 itself, now and then 0 (passed over) or another tone
        const int t = listed[k];
        const bool last = k + 1 == split || k + 1 == listed.size();
        const uint32_t next = last ? 0 : s.pool.addressOf(listed[k + 1]);
        const uint32_t pick = rng() % 6;
        const int other = s.live[rng() % s.live.size()];
        const uint32_t self = pick == 0 ? 0 : s.pool.addressOf(pick == 1 ? other : t);
        toneBothWord(s, t, kToneLinkAt, s.pool.tones[(size_t)t].head.prev, next);
        toneBothWord(s, t, kToneSelfAt, s.pool.tones[(size_t)t].head.self, self);
    }
    const auto head = [&](size_t k, bool has) { return has ? s.pool.addressOf(listed[k]) : 0u; };
    s.shared.lists = { head(0, split > 0), head(split, split < listed.size()) };
    s.fw.cpu.write(kToneListsAt, s.shared.lists.data(), sizeof s.shared.lists);
}
} // namespace

std::string toneDifference(ToneSides& s, int only) {
    const Vc2TonePool& p = s.pool;
    std::string what = vc2RegionsDifference(s.fw, { { kHeadAt, &p.head, sizeof p.head, "the pool's head" },
                                                    { kSerialAt, &p.serial, sizeof p.serial, "the serial" } });
    for (int i = 0; i < Vc2TonePool::kTones && what.empty(); ++i) {
        if (only < 0 || i == only) what = toneWordsDifference(s, i);
    }
    const auto& in = s.shared.input.words;
    if (what.empty()) what = vc2FirstDifference(s.fw, kToneInputAt, in.data(), sizeof in, "the input");
    for (uint32_t l = 0; l < Vc2Tone::kLfos && what.empty(); ++l) {
        const auto& marks = s.shared.free[l];
        what = vc2FirstDifference(s.fw, kToneFreeAt + kToneFreeStride * l, marks.data(), sizeof marks, "a free object");
    }
    return what.empty() ? toneCallsDifference(s.rom, s.shared.calls) : what;
}

void tonePrepare(ToneSides& s, int i, std::mt19937& rng) {
    ToneDraws& d = s.shared.draws;
    d.tvaOver = rng() % 4 == 0;
    d.tvaStopped = rng() % 4 == 0;
    d.tvaHeld = drawFlag(rng);
    std::ranges::generate(d.installOsc, [&] { return (uint32_t)rng(); });
    std::ranges::generate(d.installCosm, [&] { return drawFlag(rng) ? rng() % 2 : (uint32_t)rng(); });
    std::ranges::generate(d.freeValue, [&] { return drawFloat(rng); });
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) {
        const uint32_t at = kToneFreeAt + kToneFreeStride * l;
        s.shared.free[l] = {};
        s.fw.cpu.write(at, s.shared.free[l].data(), sizeof s.shared.free[l]);
        s.fw.cpu.write(at + kFreeValue, &d.freeValue[l], sizeof(float));
    }
    s.parts[(size_t)i].words = drawWords(rng);
    toneWriteRom(s.fw, s.pool.addressOf(i), s.parts[(size_t)i].words);
    s.rom.clear();
    s.shared.calls.clear();
    ++s.n.events;
}

void tonePrepareNoteOn(ToneSides& s, int i, std::mt19937& rng) {
    tonePrepare(s, i, rng);
    drawToneInput(s, rng);
    drawPrevious(s, rng);
    s.fw.cpu.write(kToneInputAt, s.shared.input.words.data(), sizeof s.shared.input.words);
    buildLists(s, i, rng);
}
#endif
