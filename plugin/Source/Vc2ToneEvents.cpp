// xt_vc2tone's events: see Vc2ToneSides.h (v228, v229; moved out of Vc2ToneCheck.cpp, the note-on's added).  Each
// draws on both sides, runs rom.4's routine and the port's, then compares.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2CheckCalls.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {
using In = Vc2ToneInput;
// [FW] rom.4's routines, the tone's
constexpr uint32_t kMakeAt = 0x893b2db0;      // the constructor
constexpr uint32_t kUnmakeAt = 0x893b2fc0;    // the destructor
constexpr uint32_t kNoteOnAt = 0x893b3390;    // the note-on
constexpr uint32_t kUpdateAt = 0x893b4940;    // +0x0c
constexpr uint32_t kWholeAt = 0x893b4b10;     // +0x18
constexpr uint32_t kRenderAt = 0x893b4bb0;    // +0x24
constexpr uint32_t kClockAt = 0x893b4c20;     // +0x30
constexpr uint32_t kCountAt = 0x893b4c70;     // +0x3c
constexpr uint32_t kMatchedAt = 0x893b4ca0;   // +0x48
constexpr int32_t kAnyValue = -1;   // [FW] the matched count's value that matches every tone
constexpr uint32_t kFixed = 2, kFixMix = 4;   // [FW] what the note-on's fix-up leaves in OSC 2's +0xb0, and its mix

// [FW] the questions (+0x54 the level test, +0x60 held) and the plain calls (+0x6c start, the install pass)
struct ToneAsk {
    uint32_t at;
    bool (*ask)(Vc2Tone&, Vc2ToneParts&);
};
constexpr ToneAsk kAsks[] = {
    { 0x893b4c00, [](Vc2Tone& t, Vc2ToneParts& p) { return t.sounds(p); } },
    { 0x893b4040, [](Vc2Tone& t, Vc2ToneParts& p) { return t.held(p); } },
};
struct TonePlain {
    uint32_t at;
    void (*act)(Vc2Tone&, Vc2ToneParts&);
};
constexpr TonePlain kPlains[] = {
    { 0x893b4050, [](Vc2Tone& t, Vc2ToneParts&) { t.start(); } },
    { 0x893b4060, [](Vc2Tone& t, Vc2ToneParts& p) { t.install(p); } },
};

// An event on tone i: rom.4's routine on `args` and the port's `mine()`, then what differs on it
template <class Mine>
std::string toneOn(ToneSides& s, int i, uint32_t at, std::initializer_list<uint32_t> args, Mine mine) {
    return vc2BothOn(s.fw, at, args, "a tone's routine", mine, [&] { return toneDifference(s, i); });
}

// A live tone drawn and prepared, its number
int toneDrawLive(ToneSides& s, std::mt19937& rng) {
    const int i = s.live[rng() % s.live.size()];
    tonePrepare(s, i, rng);
    return i;
}

uint32_t toneDrawId(std::mt19937& rng) { return rng() % 2 == 0 ? rng() % 16 : (uint32_t)rng(); }
Vc2ToneParts& partsOf(ToneSides& s, int i) { return s.parts[(size_t)i]; }

std::string toneInPlaceOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    const uint32_t id = toneDrawId(rng), at = s.pool.addressOf(i);
    ++s.n.inPlace;
    std::string what =
        toneOn(s, i, kMakeAt, { at, id }, [&] { s.pool.tones[(size_t)i].construct(at, id, partsOf(s, i)); });
    return what.empty() && s.fw.cpu.r[0] != at ? "the constructor's tone where it stands" : what;
}

std::string toneUnmakeOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    if (rng() % 3 == 0) toneBothWord(s, i, kToneStateAt, tone.head.state, Vc2Tone::kFromOne);
    const uint32_t flags = (rng() % 8 == 0 ? 0u : 1u) | (rng() % 2 == 0 ? 2u : 0u);   // back but now and then
    std::erase(s.live, i);
    ++s.n.unmakes;
    s.n.back += flags & 1;
    return vc2BothOn(
        s.fw, kUnmakeAt, { s.pool.addressOf(i), flags }, "the destructor",
        [&] { s.pool.unmake(tone, (flags & 1) != 0, partsOf(s, i)); }, [&] { return toneDifference(s, -1); });
}

std::string toneUpdateOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    if (rng() % 8 != 0) toneBothWord(s, i, kToneStateAt, tone.head.state, Vc2Tone::kSounding);
    // [FW] 0x893b16c8: the update's caller gives at least 1 tick, so its low half, which the update extends, stays positive
    constexpr uint32_t kPositiveLowHalf = 0xffff7fff;
    const uint32_t n = rng() % 2 == 0 ? rng() % 64 : (uint32_t)rng() & kPositiveLowHalf;
    s.n.updates += tone.head.state == Vc2Tone::kSounding;
    std::string what =
        toneOn(s, i, kUpdateAt, { s.pool.addressOf(i), n }, [&] { tone.update((int32_t)n, partsOf(s, i)); });
    s.n.ends += tone.head.state == Vc2Tone::kEnded;
    return what;
}

std::string toneRenderOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    toneBothWord(s, i, kToneRendersAt, tone.tail.renders, rng() % 4 == 0 ? 0 : 1);
    s.n.renders += tone.tail.renders != 0;
    std::string what = toneMixPrepareRender(s, i, rng);
    if (!what.empty()) return what;
    float* out = s.shared.out.data();
    const std::initializer_list<uint32_t> args = { s.pool.addressOf(i), kToneOutAt };
    what = rng() % 2 == 0
               ? toneOn(s, i, kWholeAt, args, [&] { tone.renderWhole(out, s.shared.buffers, partsOf(s, i)); })
               : toneOn(s, i, kRenderAt, args, [&] { tone.render(out, s.shared.buffers, partsOf(s, i)); });
    return what.empty() ? toneMixRendered(s, i) : what;
}

std::string toneClockOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    const float x = 4.0f * vc2Unit(rng) - 2.0f;
    s.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(x);
    return toneOn(s, i, kClockAt, { s.pool.addressOf(i) }, [&] { s.pool.tones[(size_t)i].clock(x, partsOf(s, i)); });
}

std::string toneCountOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    const uint32_t x = rng() % 4;
    return toneOn(s, i, kCountAt, { s.pool.addressOf(i), x }, [&] { s.pool.tones[(size_t)i].count(x, partsOf(s, i)); });
}

// The matched count: every tone, the tone's own id under the mask, or any value
std::string toneMatchedOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    const uint32_t x = rng() % 4, mask = rng() % 2 == 0 ? 0xf : (uint32_t)rng(), pick = rng() % 3;
    const int32_t value = pick == 0 ? kAnyValue : (int32_t)(pick == 1 ? tone.head.id & mask : (uint32_t)rng());
    return toneOn(s, i, kMatchedAt, { s.pool.addressOf(i), x, (uint32_t)value, mask },
                  [&] { tone.countMatched(x, value, mask, partsOf(s, i)); });
}

// A question drawn from the table, rom.4's answer in r0 against the port's
std::string toneAskOn(ToneSides& s, std::mt19937& rng) {
    const ToneAsk& q = kAsks[rng() % std::size(kAsks)];
    const int i = toneDrawLive(s, rng);
    bool said = false;
    std::string what = toneOn(s, i, q.at, { s.pool.addressOf(i) },
                              [&] { said = q.ask(s.pool.tones[(size_t)i], partsOf(s, i)); });
    return what.empty() && s.fw.cpu.r[0] != (said ? 1u : 0u) ? "a tone's answer" : what;
}

std::string tonePlainOn(ToneSides& s, std::mt19937& rng) {
    const TonePlain& c = kPlains[rng() % std::size(kPlains)];
    const int i = toneDrawLive(s, rng);
    return toneOn(s, i, c.at, { s.pool.addressOf(i) }, [&] { c.act(s.pool.tones[(size_t)i], partsOf(s, i)); });
}

// The tone's own words as a note-on or another use might leave them: its head and tail drawn on both sides, but the end
// routine and the mix's member pointer and gain, which rom.4 calls and keeps, and the indexes, a delay line's number (the
// link stays: the free list follows it)
std::string toneScribbleOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    std::array<uint32_t, (sizeof tone.head + sizeof tone.tail) / sizeof(uint32_t)> words {};
    std::ranges::generate(words, [&] { return rng() % 4 == 0 ? rng() % 8 : (uint32_t)rng(); });
    const auto mixWords = std::to_array(tone.tail.unread);
    std::memcpy(&tone.head, words.data(), sizeof tone.head);
    std::memcpy(&tone.tail, words.data() + sizeof tone.head / sizeof(uint32_t), sizeof tone.tail);
    tone.tail.ended = kToneEndedAt;
    std::ranges::copy(mixWords, tone.tail.unread);
    tone.head.index = (int32_t)(rng() % Vc2ToneMixRings::kTones);
    tone.tail.index = (int32_t)(rng() % Vc2ToneMixRings::kTones);
    s.fw.cpu.write(s.pool.addressOf(i), &tone.head, sizeof tone.head);
    s.fw.cpu.write(s.pool.addressOf(i) + kToneEndedWordAt - offsetof(Vc2ToneTail, ended), &tone.tail, sizeof tone.tail);
    return toneDifference(s, i);
}

// The note-on: an input, the legato's tone and the lists drawn; rom.4's 0x893b3390(tone, input) against the port's
std::string toneNoteOnEvent(ToneSides& s, std::mt19937& rng) {
    if (!toneFreeLfosHaveRoom(s)) return "";
    const int i = s.live[rng() % s.live.size()];
    tonePrepareNoteOn(s, i, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    ++s.n.noteOns;
    std::string what = vc2BothOn(
        s.fw, kNoteOnAt, { s.pool.addressOf(i), kToneInputAt }, "the note-on",
        [&] { tone.noteOn(s.shared.input, s.pool, s.owner, partsOf(s, i)); }, [&] { return toneDifference(s, -1); });
    if (what.empty()) what = toneMixRingsOf(s, i);
    s.n.takes += vc2CountCalls(s.rom, kTakePlayer);
    s.n.fixes += s.shared.input.at(In::kOscs[1] + In::kWave) == kFixed && s.shared.input.at(In::kMix) == kFixMix;
    return what;
}

// What the OSC voice and the TVA read of a tone's envelopes and LFOs (v268), by rom.4's addresses, against rom.4's words
std::string toneBoundOn(ToneSides& s, std::mt19937& rng) {
    constexpr uint32_t kBytes[] = { Vc2ToneModulators::kEnvelopeBytes, Vc2ToneModulators::kLfoBytes };
    using W = Vc2EnvelopeWords;
    const int i = toneDrawLive(s, rng);
    const Vc2ToneModulators& m = s.parts[(size_t)i].mods;
    const uint32_t self = s.pool.addressOf(i);
    const bool envelope = rng() % 2 == 0;
    const uint32_t part = envelope ? toneEnvelopeAt(self, (int)(rng() % Vc2Tone::kEnvelopes))
                                   : toneLfoAt(self, (int)(rng() % Vc2Tone::kLfos));
    // Any word but its number, which the tone keeps (Vc2Tone::envelopeIndex, lfoIndex) and no part reads by address
    const uint32_t drawn = (rng() % (kBytes[envelope ? 0 : 1] / sizeof(uint32_t))) * sizeof(uint32_t);
    const uint32_t number = envelope ? offsetof(W, index) : offsetof(Vc2LfoWords, index);
    const uint32_t offset = drawn == number ? drawn + sizeof(uint32_t) : drawn;
    const auto same = [&](float mine, uint32_t at) { return std::bit_cast<uint32_t>(mine) == s.fw.cpu.rd32(at); };
    ++s.n.bounds;
    if (!same(m.boundFloat(self, part, offset), part + offset)) return "a bound part's word";
    if (m.boundFloat(self, self, 0) != 0.0f) return "a word of no bound part";
    const uint32_t lfo = toneLfoAt(self, (int)(rng() % Vc2Tone::kLfos));
    if (!same(m.lfoOut(self, lfo), lfo + offsetof(Vc2LfoWords, out))) return "an LFO's output";
    const uint32_t e = toneEnvelopeAt(self, (int)(rng() % Vc2Tone::kEnvelopes));
    const Vc2TvaEnvelope tva = m.tvaEnvelope(self, e);
    const bool agrees = tva.state == (int32_t)s.fw.cpu.rd32(e + offsetof(W, stage))
                        && same(tva.attack, e + offsetof(W, time)) && same(tva.next, e + offsetof(W, time) + 4)
                        && same(tva.out, e + offsetof(W, out)) && same(tva.onset, e + offsetof(W, onset))
                        && same(tva.stageTime, e + offsetof(W, stageTime));
    return agrees ? "" : "the TVA's envelope";
}

constexpr ToneEvent kEvents[] = { toneUpdateOn,    toneUpdateOn,    toneRenderOn,  toneRenderOn,   toneUnmakeOn,
                                  toneInPlaceOn,   toneClockOn,     toneCountOn,   toneMatchedOn,  toneAskOn,
                                  toneAskOn,       tonePlainOn,     tonePlainOn,   toneScribbleOn, toneNoteOnEvent,
                                  toneNoteOnEvent, toneNoteOnEvent, toneBoundOn,   toneMixPlayOn,  toneMixPlayOn,
                                  toneFreeTickOn,  toneFreeClockOn };
} // namespace

std::string toneMakeOn(ToneSides& s, std::mt19937& rng) {
    const uint32_t id = toneDrawId(rng), head = s.pool.head == 0 ? s.pool.addressOf(0) : s.pool.head;
    const int i = std::min((int)((head - Vc2TonePool::kBase) / Vc2Tone::kSize), Vc2TonePool::kTones - 1);
    tonePrepare(s, i, rng);
    Vc2Tone* tone = nullptr;
    std::string what = vc2BothOn(
        s.fw, kMakeAt, { 0, id }, "the constructor", [&] { tone = s.pool.make(id, partsOf(s, i)); },
        [&] { return toneDifference(s, -1); });
    if (what.empty() && s.fw.cpu.r[0] != (tone ? s.pool.addressOf(i) : 0)) what = "the constructor's tone";
    ++(tone ? s.n.makes : s.n.full);
    if (!what.empty() || !tone) return what;
    s.live.push_back(i);
    toneBothWord(s, i, kToneEndedWordAt, tone->tail.ended, kToneEndedAt);   // the note-on's end routine
    return "";
}

std::span<const ToneEvent> toneEvents() { return kEvents; }
#endif
