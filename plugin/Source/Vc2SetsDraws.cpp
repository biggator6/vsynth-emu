// xt_vc2sets' draws (v280): before each event, what rom.4 reads besides the port's words (the voices' 16 bytes a call
// points at, the key tables' channel words, the channels' bends and controllers, the parts' numbers, the part's bytes
// by kind) in its memory, and the port's own (C, M, W, G, the part's M).  Not drawn: a part's number past 15 (G holds
// 16 records), a voice's number past 23 (M's pool holds 24), C's M outside the pool.  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetsChildCheck.h"

namespace {
constexpr uint32_t kParts = 16, kFewParts = 4, kKinds = 4, kSums = 64;
constexpr uint16_t kBendCentre = 8192;   // [FW] a channel's bend at rest

uint32_t anyOf(uint32_t at, uint32_t bytes, std::mt19937& rng) { return at + bytes * (rng() % kSetsObjects); }

uint32_t anyChannel(std::mt19937& rng) { return anyOf(kSetsChannelsAt, kSetsChannelBytes, rng); }

// A note's kind as C reads it: 0 half the time, a few others, now and then any
uint8_t setKindDrawn(std::mt19937& rng) {
    if (rng() % 2) return 0;
    return rng() % 8 ? (uint8_t)(1 + rng() % (kKinds - 1)) : (uint8_t)rng();
}

void setVoiceDrawn(SetSides& s, uint32_t j, std::mt19937& rng) {
    Vc2SetVoice v;
    v.keys = anyOf(kSetsKeysAt, kSetsKeyBytes, rng);
    v.source = (uint32_t)rng();
    v.part = anyOf(kSetsPartsAt, kSetsPartBytes, rng);
    v.number = (uint8_t)(rng() % Vc2Sets::kCount);
    v.key = (uint8_t)rng();
    v.byte42 = (uint8_t)rng();
    v.byte43 = (uint8_t)rng();
    s.fw.cpu.write(kSetsVoicesAt + kSetsVoiceBytes * j + kSetsVoiceWords, &v, sizeof v);
}

// A channel's bend (the centre, its ends, near the centre, any) and the bytes past it
void setsChannelDrawn(SetSides& s, uint32_t at, std::mt19937& rng) {
    const uint16_t near = (uint16_t)(kBendCentre + rng() % 17 - 8);
    const uint16_t any = (uint16_t)rng();
    const uint16_t bends[7] = { kBendCentre, 0, 0x3fff, 0x7fff, 0x8000, near, any };
    s.fw.cpu.wr16(at, bends[rng() % 7]);
    for (uint32_t b = 2; b < kSetsChannelBytes; ++b) s.fw.cpu.wr8(at + b, (uint8_t)rng());
}

// A part's number: one of a few (where the part's M's matches) or any of the sixteen
uint8_t setsPartDrawn(std::mt19937& rng) { return (uint8_t)(rng() % (rng() % 2 ? kFewParts : kParts)); }

void setsSetDrawn(Vc2SetWords& c, std::mt19937& rng) {
    c.kind = setKindDrawn(rng);
    c.mode = (uint8_t)rng();
    for (uint32_t& child : c.children)
        child = rng() % 4 ? kSetsChildrenAt + kSetsChildBytes * (uint32_t)(rng() % kSetsChildren) : 0;
    c.matrix = Vc2NoteMatrices::kAt + Vc2NoteMatrices::kBytes * (uint32_t)(rng() % Vc2NoteMatrices::kCount);
}

void setsNoteDrawn(Vc2NoteWords& w, std::mt19937& rng) {
    w.kind = setKindDrawn(rng);
    w.input.at(Vc2ToneInput::kStructure) = (uint32_t)rng();
    w.input.at(Vc2ToneInput::kOscSet) = (uint32_t)rng();
}

// rom.4's memory the routines read past the port's words
void setsAround(SetSides& s, std::mt19937& rng, bool all) {
    for (uint32_t j = 0; j < (uint32_t)kSetsObjects; ++j) {
        if (all || rng() % 4 == 0) setVoiceDrawn(s, j, rng);
        if (all || rng() % 4 == 0) s.fw.cpu.wr32(kSetsKeysAt + kSetsKeyBytes * j, anyChannel(rng));
        if (all || rng() % 4 == 0) setsChannelDrawn(s, kSetsChannelsAt + kSetsChannelBytes * j, rng);
        if (all || rng() % 4 == 0) s.fw.cpu.wr8(kSetsPartsAt + kSetsPartBytes * j, setsPartDrawn(rng));
    }
    if (all || rng() % 8 == 0)
        for (uint32_t b = 0; b < kParts * kParts; ++b) s.fw.cpu.wr8(kSetsModesAt + b, (uint8_t)rng());
}
} // namespace

void setsDraw(SetSides& s, std::mt19937& rng, bool all) {
    s.seed = (uint32_t)rng();
    setsAround(s, rng, all);
    for (Vc2SetWords& c : s.port.items)
        if (all || rng() % 4 == 0) setsSetDrawn(c, rng);
    for (Vc2NoteMatrixWords& m : s.port.matrices.items) {
        if (!all && rng() % 8) continue;
        if (rng() % 2) setsNoise(m, rng);
        m.links.channel = anyChannel(rng);
    }
    for (Vc2NoteWords& w : s.notes.notes)
        if (all || rng() % 4 == 0) setsNoteDrawn(w, rng);
    if (all) setsNoise(s.g, rng);
    if (!all && rng() % 8 == 0) setsNoise(s.g.parts[rng() % kParts], rng);
    if (!all && rng() % 8) return;
    setsNoise(s.partM, rng);
    s.partLinks.part = (int16_t)(rng() % 2 ? rng() % kFewParts : rng());
    s.partLinks.channel = (uint32_t)rng();
}

uint32_t setsSet(std::mt19937& rng) { return Vc2Sets::kAt + Vc2Sets::kBytes * (uint32_t)(rng() % Vc2Sets::kCount); }

uint32_t setsNote(std::mt19937& rng) {
    return Vc2Notes::kNotesAt + (uint32_t)sizeof(Vc2NoteWords) * (uint32_t)(rng() % Vc2Notes::kNotes);
}

uint32_t setsCall(SetSides& s, std::mt19937& rng) {
    const uint32_t voice = kSetsVoicesAt + kSetsVoiceBytes * (uint32_t)(rng() % kSetsObjects);
    const uint32_t words[3] = { voice + kSetsVoiceWords, voice + kSetsByteAt, (uint32_t)rng() };
    s.fw.cpu.write(kVc2Args, words, sizeof words);
    return kVc2Args;
}

void setsPartWordsDrawn(Vc2Fw& fw, std::mt19937& rng, bool all, const SetPartTable& t, SetWordsDraw draw) {
    for (uint32_t p = 0; p < kParts; ++p)
        for (uint32_t k = 0; k < kKinds; ++k) {
            if (!all && rng() % 16) continue;
            for (uint32_t i = 0; i < t.perKind; ++i) {
                const std::array<uint16_t, 5> words = draw(rng);
                fw.cpu.write(t.at + t.part * p + t.kind * k + kSetsPartWordsBytes * i, words.data(), sizeof words);
            }
        }
}

// A sum: 0, small, the ends of a byte's range, wider, any
void setsSumsDrawn(Vc2Fw& fw, std::mt19937& rng, bool all) {
    for (uint32_t k = 0; k < kSums; ++k) {
        if (!all && rng() % 4) continue;
        const auto sums = std::to_array<int16_t>({ 0, (int16_t)(rng() % 21 - 10), 127, -127,
                                                  (int16_t)(rng() % 601 - 300), (int16_t)rng() });
        fw.cpu.wr16(kSetsSumsAt + 2 * k, (uint16_t)setsOneOf(sums, rng));
    }
}

void setsHookReaders(SetSides& s, std::span<const uint32_t> readers) {
    for (const uint32_t reader : readers)
        s.fw.cpu.hook(reader, [&s, reader](Sh4& c) {
            s.rom.push_back({ kSetMatrixSum, reader, { c.r[4] } });
            c.ret(kSetsSumsAt);
        });
}
#endif
