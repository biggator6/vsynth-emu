// xt_vc2tone <rom.4> [cases] [seed] (v228): the VC-2's tone object's frame, Vc2Tone and Vc2TonePool, against rom.4's own
// routines on its pool (0x8999f524), every part routine hooked to log and answer as drawn (Vc2ToneFakes.cpp), the
// port's parts the same.  Each case starts both pools fresh, then 400 events on up to 8 tones at once: a tone made
// (0x893b2db0(0, id): the first take links the pool, a full pool gives none) or built where it stands, unmade
// (0x893b2fc0(tone, flags): back to the pool by flags bit 0, now and then from state 1), updated (0x893b4940, its state
// set to 2 most times, the count with high bits), rendered whole (0x893b4b10) or not (0x893b4bb0) with the render flag
// drawn, clocked (0x893b4c20), counted (0x893b4c70) or by a mask (0x893b4ca0), asked (0x893b4c00, 0x893b4040), started
// (0x893b4050) or installed (0x893b4060).  Before each, the parts' answers drawn and written to the tone's parts
// (Vc2ToneSides.cpp).  After each, the pool's head and serial, the tones' own words (every tone's after a make or an
// unmake), the calls and the answers.  XT_VC2TONE_FIRST=1 says where the first case to differ does, XT_VC2TONE_STOP=1
// stops there.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneCheck.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {
// [FW] rom.4's routines, the tone's
constexpr uint32_t kMakeAt = 0x893b2db0;      // the constructor
constexpr uint32_t kUnmakeAt = 0x893b2fc0;    // the destructor
constexpr uint32_t kUpdateAt = 0x893b4940;    // +0x0c
constexpr uint32_t kWholeAt = 0x893b4b10;     // +0x18
constexpr uint32_t kRenderAt = 0x893b4bb0;    // +0x24
constexpr uint32_t kClockAt = 0x893b4c20;     // +0x30
constexpr uint32_t kCountAt = 0x893b4c70;     // +0x3c
constexpr uint32_t kMatchedAt = 0x893b4ca0;   // +0x48
constexpr uint32_t kHeadAt = 0x8993fb48;      // [FW] the pool's head and serial, cleared at a case's start
constexpr int kSteps = 400, kMaxLive = 8;
constexpr int32_t kAnyValue = -1;   // [FW] the matched count's value that matches every tone

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

std::string toneMakeOn(ToneSides& s, std::mt19937& rng) {
    const uint32_t id = toneDrawId(rng), head = s.pool.head == 0 ? s.pool.addressOf(0) : s.pool.head;
    const int i = std::min((int)((head - Vc2TonePool::kBase) / Vc2Tone::kSize), Vc2TonePool::kTones - 1);
    tonePrepare(s, i, rng);
    Vc2Tone* tone = nullptr;
    std::string what = vc2BothOn(
        s.fw, kMakeAt, { 0, id }, "the constructor", [&] { tone = s.pool.make(id, s.parts); },
        [&] { return toneDifference(s, -1); });
    if (what.empty() && s.fw.cpu.r[0] != (tone ? s.pool.addressOf(i) : 0)) what = "the constructor's tone";
    ++(tone ? s.n.makes : s.n.full);
    if (!what.empty() || !tone) return what;
    s.live.push_back(i);
    toneBothWord(s, i, kToneEndedWordAt, tone->tail.ended, kToneEndedAt);   // the note-on's end routine
    return "";
}

std::string toneInPlaceOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    const uint32_t id = toneDrawId(rng), at = s.pool.addressOf(i);
    ++s.n.inPlace;
    std::string what = toneOn(s, i, kMakeAt, { at, id }, [&] { s.pool.tones[(size_t)i].construct(at, id, s.parts); });
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
        [&] { s.pool.unmake(tone, (flags & 1) != 0, s.parts); }, [&] { return toneDifference(s, -1); });
}

std::string toneUpdateOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    if (rng() % 8 != 0) toneBothWord(s, i, kToneStateAt, tone.head.state, Vc2Tone::kSounding);
    const uint32_t n = rng() % 2 == 0 ? rng() % 64 : (uint32_t)rng();
    s.n.updates += tone.head.state == Vc2Tone::kSounding;
    std::string what =
        toneOn(s, i, kUpdateAt, { s.pool.addressOf(i), n }, [&] { tone.update((int32_t)n, s.parts); });
    s.n.ends += tone.head.state == Vc2Tone::kEnded;
    return what;
}

std::string toneRenderOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    toneBothWord(s, i, kToneRendersAt, tone.tail.renders, rng() % 4 == 0 ? 0 : 1);
    s.n.renders += tone.tail.renders != 0;
    const std::initializer_list<uint32_t> args = { s.pool.addressOf(i), kToneOutAt };
    if (rng() % 2 == 0) return toneOn(s, i, kWholeAt, args, [&] { tone.renderWhole(s.out, s.buffers, s.parts); });
    return toneOn(s, i, kRenderAt, args, [&] { tone.render(s.out, s.buffers, s.parts); });
}

std::string toneClockOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    const float x = 4.0f * vc2Unit(rng) - 2.0f;
    s.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(x);
    return toneOn(s, i, kClockAt, { s.pool.addressOf(i) }, [&] { s.pool.tones[(size_t)i].clock(x, s.parts); });
}

std::string toneCountOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    const uint32_t x = rng() % 4;
    return toneOn(s, i, kCountAt, { s.pool.addressOf(i), x }, [&] { s.pool.tones[(size_t)i].count(x, s.parts); });
}

// The matched count: every tone, the tone's own id under the mask, or any value
std::string toneMatchedOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    const uint32_t x = rng() % 4, mask = rng() % 2 == 0 ? 0xf : (uint32_t)rng(), pick = rng() % 3;
    const int32_t value = pick == 0 ? kAnyValue : (int32_t)(pick == 1 ? tone.head.id & mask : (uint32_t)rng());
    return toneOn(s, i, kMatchedAt, { s.pool.addressOf(i), x, (uint32_t)value, mask },
                  [&] { tone.countMatched(x, value, mask, s.parts); });
}

// A question drawn from the table, rom.4's answer in r0 against the port's
std::string toneAskOn(ToneSides& s, std::mt19937& rng) {
    const ToneAsk& q = kAsks[rng() % std::size(kAsks)];
    const int i = toneDrawLive(s, rng);
    bool said = false;
    std::string what = toneOn(s, i, q.at, { s.pool.addressOf(i) },
                              [&] { said = q.ask(s.pool.tones[(size_t)i], s.parts); });
    return what.empty() && s.fw.cpu.r[0] != (said ? 1u : 0u) ? "a tone's answer" : what;
}

std::string tonePlainOn(ToneSides& s, std::mt19937& rng) {
    const TonePlain& c = kPlains[rng() % std::size(kPlains)];
    const int i = toneDrawLive(s, rng);
    return toneOn(s, i, c.at, { s.pool.addressOf(i) }, [&] { c.act(s.pool.tones[(size_t)i], s.parts); });
}

// The tone's own words as a note-on or another use might leave them: its head and tail drawn on both sides, but the end
// routine, which rom.4 calls (the link stays: the free list follows it)
std::string toneScribbleOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    std::array<uint32_t, (sizeof tone.head + sizeof tone.tail) / sizeof(uint32_t)> words {};
    std::ranges::generate(words, [&] { return rng() % 4 == 0 ? rng() % 8 : (uint32_t)rng(); });
    std::memcpy(&tone.head, words.data(), sizeof tone.head);
    std::memcpy(&tone.tail, words.data() + sizeof tone.head / sizeof(uint32_t), sizeof tone.tail);
    tone.tail.ended = kToneEndedAt;
    s.fw.cpu.write(s.pool.addressOf(i), &tone.head, sizeof tone.head);
    s.fw.cpu.write(s.pool.addressOf(i) + kToneEndedWordAt - offsetof(Vc2ToneTail, ended), &tone.tail, sizeof tone.tail);
    return toneDifference(s, i);
}

using ToneEvent = std::string (*)(ToneSides&, std::mt19937&);
constexpr ToneEvent kEvents[] = { toneUpdateOn, toneUpdateOn, toneUpdateOn, toneRenderOn, toneRenderOn, toneUnmakeOn,
                                  toneInPlaceOn, toneClockOn, toneCountOn, toneMatchedOn, toneAskOn, toneAskOn,
                                  tonePlainOn, tonePlainOn, toneScribbleOn };

// Both pools fresh: rom.4's tones' memory, head and serial cleared, the port's pool new
void toneFreshPools(ToneSides& s) {
    const std::vector<uint8_t> zeros(Vc2TonePool::kTones * Vc2Tone::kSize);
    s.fw.cpu.write(Vc2TonePool::kBase, zeros.data(), zeros.size());
    s.fw.cpu.write(kHeadAt, zeros.data(), 2 * sizeof(uint32_t));
    s.pool = Vc2TonePool {};
    s.live.clear();
}

// A case: both pools fresh, then its events; one case in four lets more tones live than the pool holds
std::string vc2ToneCase(ToneSides& s, std::mt19937& rng) {
    toneFreshPools(s);
    const bool fills = rng() % 4 == 0;
    const int most = fills ? Vc2TonePool::kTones + 2 : kMaxLive;
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k) {
        const bool make = s.live.empty() || ((int)s.live.size() < most && rng() % (fills ? 2 : 6) == 0);
        what = make ? toneMakeOn(s, rng) : kEvents[rng() % std::size(kEvents)](s, rng);
    }
    return what;
}

bool toneNoTables(const char*) { return true; }
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2tone", toneNoTables);
    if (!run) return 1;
    Vc2TonePool pool;
    ToneDraws draws;
    Vc2ToneBuffers buffers {};
    std::array<float, Vc2ToneBuffers::kFloats> out {};
    TonePortParts parts(draws, buffers, out.data());
    std::vector<ToneCall> rom;
    ToneSides s { fw, pool, parts, draws, buffers, rom, out.data() };
    parts.pool = &pool;
    toneInstallRom(fw, draws, rom);
    Vc2Tally tally { "XT_VC2TONE_FIRST", "XT_VC2TONE_STOP" };
    vc2RunCases(*run, tally, toneDifference(s, -1), s.n.events,
                [&](std::mt19937& rng, bool) { return vc2ToneCase(s, rng); });
    const ToneCounts& n = s.n;
    std::printf("tone: %d cases, %ld events, %ld made, %ld none, %ld built in place, %ld unmade (%ld back), "
                "%ld updates, %ld ended, %ld renders, %ld different\n", run->cases, n.events, n.makes, n.full,
                n.inPlace, n.unmakes, n.back, n.updates, n.ends, n.renders, tally.different);
    std::printf("xt_vc2tone: %ld different\n", tally.different);
    return 0;
}
#endif
