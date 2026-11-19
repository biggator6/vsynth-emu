// xt_vc2tone's part for the real free LFOs (v271): see Vc2ToneSides.h.  rom.4's own manager runs beside Vc2FreeLfos, built
// afresh at each case's start, its 0x8448 bytes compared word for word after every event that can reach it.  The LFOs'
// note-ons, destructors and follows reach it through the tone's own routines; its two passes are events here.
// XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include <bit>
#include <cstddef>

namespace {
constexpr uint32_t kConstructAt = 0x893ad4b0, kTickAt = 0x893ad5f0, kClockAt = 0x893ad870;   // [FW] the manager's
constexpr uint32_t kSpareNext = Vc2FreeLfos::kAt + offsetof(Vc2FreeLfoBank, spare) + offsetof(Vc2FreeLfoNode, next);
constexpr uint32_t kMostTicks = 5;           // a pass's ticks, as the scheduler gives them: the phase stays near its span
constexpr float kLongestClock = 0.25f;       // seconds between clocks, past both ends of what an object keeps
} // namespace

// The manager's words, and no word written for an LFO that follows nothing: the check never lets the pool run out
std::string toneFreeLfosDifference(ToneSides& s) {
    const Vc2FreeLfoBank& w = s.shared.frees.w;
    const Vc2FreeLfoWords& none = s.shared.frees.noObject();
    if (none.refs != 0 || none.lfo != 0 || none.id != 0) return "a word of no free object";
    return vc2FirstDifference(s.fw, Vc2FreeLfos::kAt, &w, sizeof w, "the free LFOs");
}

std::string toneFreeLfosFresh(ToneSides& s) {
    return vc2BothOn(s.fw, kConstructAt, { Vc2FreeLfos::kAt }, "the free LFOs' constructor",
                     [&] { s.shared.frees.construct(); }, [&] { return toneFreeLfosDifference(s); });
}

// Read from rom.4's list: an object released while spare counts twice in the words that count them
bool toneFreeLfosHaveRoom(ToneSides& s) {
    uint32_t at = s.fw.cpu.rd32(kSpareNext);
    for (int k = 0; k < Vc2Tone::kLfos && at != 0; ++k) at = s.fw.cpu.rd32(at + offsetof(Vc2FreeLfoNode, next));
    return at != 0;
}

uint32_t toneDrawFollow(std::mt19937& rng) {
    if (rng() % 2 == 0) return 0;
    return Vc2FreeLfos::kObjectsAt + (rng() % Vc2FreeLfoBank::kObjects) * (uint32_t)sizeof(Vc2FreeLfoWords);
}

// The pass takes its count as a halfword, so the upper half is drawn as noise
std::string toneFreeTickOn(ToneSides& s, std::mt19937& rng) {
    tonePrepare(s, s.live[rng() % s.live.size()], rng);
    const uint32_t n = rng() % kMostTicks | (uint32_t)rng() << 16;
    ++s.n.freeTicks;
    return vc2BothOn(s.fw, kTickAt, { Vc2FreeLfos::kAt, n }, "the free LFOs' tick",
                     [&] { s.shared.frees.tick((int32_t)n); }, [&] { return toneDifference(s, -1); });
}

std::string toneFreeClockOn(ToneSides& s, std::mt19937& rng) {
    tonePrepare(s, s.live[rng() % s.live.size()], rng);
    const float seconds = kLongestClock * vc2Unit(rng);
    s.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(seconds);
    ++s.n.freeClocks;
    return vc2BothOn(s.fw, kClockAt, { Vc2FreeLfos::kAt }, "the free LFOs' clock",
                     [&] { s.shared.frees.clock(seconds); }, [&] { return toneDifference(s, -1); });
}
#endif
