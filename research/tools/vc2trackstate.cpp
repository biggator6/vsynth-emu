// The pitch tracker's settings and state, read after the track run's feeds (vc2track.cpp; research/VC2_VOCAL_DESIGNER.md,
// "The pitch tracker's analysis"): what the setters and the reset leave in mode 0, then, after each feed, the period, the
// band-pass's targets and coefficients (as Hz), the recent peak, the noise count, the lock and the smoothing.  Each word's
// role is the reading of the listing, named here so a run can hold it to the firmware.
#include "vc2track.h"
#include <cstdio>
#include <numbers>

namespace vc2 {

namespace {

enum class Unit { hz, period, level, count };
struct UnitShow {
    double scale;   // a word's value times this is what is said
    const char* suffix;
    bool integer;
};
constexpr UnitShow kShows[] = {
    {kTrackRate / (2 * std::numbers::pi), " Hz", false}, {1, " samples", false}, {1, "", false}, {1, "", true}};

struct Word {
    uint32_t at;
    const char* name;
    Unit unit;
};
// [FW] read in 0x893ee280 and the routines under it
constexpr Word kSettings[] = {
    {0x899d9ba0, "the peak frame", Unit::count},
    {0x899d9bac, "the level a raw crossing and the recent peak must pass", Unit::level},
    {0x899d9bb0, "level bb0", Unit::level},
    {0x899d9bb4, "level bb4", Unit::level},
    {0x899d9bb8, "level bb8", Unit::level},
    {0x899d9bbc, "level bbc", Unit::level},
    {0x899d9bc0, "level bc0", Unit::level},
    {0x899d9bc4, "level bc4, an onset's peak", Unit::level},
    {0x899d9ba4, "period ba4", Unit::period},
    {0x899d9ba8, "period ba8", Unit::period},
    {0x899d9b84, "the shortest period the band follows", Unit::period},
    {0x899d9b80, "the longest period the band follows", Unit::period},
    {0x899d9cd8, "the shortest candidate", Unit::period},
    {0x899d9cdc, "the longest candidate", Unit::period},
    {0x899d9bc8, "the mode's high-pass", Unit::hz},
    {0x899d9bcc, "the mode's low-pass", Unit::hz},
};
constexpr Word kState[] = {
    {kTrackPeriod, "period", Unit::period},
    {0x899d9bd8, "last estimate", Unit::period},
    {0x899d9cb8, "high-pass target", Unit::hz},
    {0x899d9ce4, "now", Unit::hz},
    {0x899d9cb4, "low-pass target", Unit::hz},
    {0x899d9ce0, "now", Unit::hz},
    {0x899d9c7c, "recent peak", Unit::level},
    {0x899d9d14, "noise count", Unit::count},
    {0x899d9c68, "lock", Unit::count},
    {0x899d9c6c, "fails", Unit::count},
    {0x899d9cd4, "smoothing", Unit::level},
};

constexpr double kLoud = 0.5;
struct Feed {
    const char* name;
    std::vector<TrackSegment> segments;
};

std::vector<Feed> feeds() {
    const auto tone = [](double hz, double level) { return TrackInput{TrackKind::tone, hz, level}; };
    return {
        {"the reset, nothing fed", {}},
        {"110 Hz at 0.5", {{tone(110, kLoud), kTrackSecond}}},
        {"440 Hz at 0.5", {{tone(440, kLoud), kTrackSecond}}},
        {"1480 Hz at 0.5", {{tone(1480, kLoud), kTrackSecond}}},
        {"880 Hz at 0.05 (not tracked)", {{tone(880, 0.05), kTrackSecond}}},
        {"220 Hz at 0.002 (−54 dB)", {{tone(220, 0.002), kTrackSecond}}},
        {"220 Hz at 0.001 (−60 dB, not tracked)", {{tone(220, 0.001), kTrackSecond}}},
        {"noise at 0.5", {{{TrackKind::noise, 0, kLoud}, kTrackSecond}}},
        {"110 Hz then 880 Hz at 0.5", {{tone(110, kLoud), kTrackSecond}, {tone(880, kLoud), kTrackSecond}}},
    };
}

void sayWord(Machine& m, const Word& w) {
    const UnitShow& show = kShows[(int)w.unit];
    if (show.integer)
        std::printf(" %s %d;", w.name, (int32_t)m.cpu.rd32(w.at));
    else
        std::printf(" %s %.6g%s;", w.name, m.f32(w.at) * show.scale, show.suffix);
}

} // namespace

bool trackState(Machine& m) {
    std::printf("\nmode %u, the settings after the reset\n", kTrackFirmwareMode);
    if (!trackReadings(m, kTrackFirmwareMode, {}).ok) return false;
    for (const Word& w : kSettings) {
        std::printf(" ");
        sayWord(m, w);
        std::printf("\n");
    }
    std::printf("\nmode %u, the state after each feed\n", kTrackFirmwareMode);
    for (const Feed& f : feeds()) {
        if (!trackReadings(m, kTrackFirmwareMode, f.segments).ok) return false;
        std::printf("  %s:\n   ", f.name);
        for (const Word& w : kState) sayWord(m, w);
        std::printf("\n");
    }
    return true;
}

} // namespace vc2
