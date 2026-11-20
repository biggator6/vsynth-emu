// xt_vc2settings' nineteenth part (v306): the part's Time Trip, Vc2TripRecords and the trip object's routines, against
// rom.4's own on its own records (0x89989408) and the steps' scratch words (0x899996a4), on the seed eighteen after the
// run's.  The fifteenth part's hooks on the set and the move taken off, and the sounding tones' +0x48 (0x893b4ca0)
// hooked to log the tone, the count, the part and the mask.  Each case every table noise, the records and the scratch
// noise; then 400 events, each a routine on a record drawn: the static initializer (one in 32), the constructor, the
// rate (an index 0…8, now and then 9…15), the set, the move, the position, the steps or the broadcast alone.  Before
// each, half the time the record's words drawn about the routines' edges (the angle a few steps from the last, now and
// then none or any; the carry about the most held; the way 0, 1, −1 or any; the rate's words the table's or any), and
// the sounding list: up to three tones in scratch memory, each with the tone's vtable at its +0x18.  After each the
// tables, the records, the scratch words, the calls and the answer.  Not drawn: X or Y past 127 where the position is
// reached (rom.4 reads past its table), steps below 1 where the steps are reached (its division by them), a rate's
// index below 0.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartControllersCheck.h"

namespace {
using R = Vc2TripRecords;
using T = Vc2PartSettingsTables;
using Args = std::array<uint32_t, 4>;
constexpr uint32_t kTonesAt = 0x8C044000, kToneBytes = 0x40;                       // in Vc2Fw's RAM
constexpr uint32_t kSoundingAt = 0x899996d0, kToneVtable = 0x898cd568;           // [FW]
constexpr uint32_t kVtableAt = 0x18, kStepEntry = 0x48;                          // [FW] a tone's vtable, its slot
constexpr int kTones = 3, kMostSteps = 400;
constexpr uint8_t kPadMost = 128;

uint32_t tripRecordAt(int k) { return R::kAt + (uint32_t)(sizeof(Vc2TripRecord) * (size_t)k); }
Vc2TripObject& tripOf(SettingsSides& s, int k) { return s.tripRecords.records[(size_t)k].front.trip; }

SettingsCall tripStepped(Sh4& c) {
    return { kOutTripStep, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], (int32_t)c.r[4] };
}
constexpr SettingsCallee kTripCallees[] = { { 0x893b4ca0, tripStepped } };   // [FW] the tone's +0x48
constexpr uint32_t kTripOwn[] = { kTripSetAt, kTripMoveAt };

void hookTrip(SettingsSides& s) { settingsHookCallees(s, kTripCallees, kTripOwn); }

// The port's parts: the list read from rom.4's memory where the draws wrote it, a tone's +0x48 logged as its hook
class TripPortParts final : public Vc2TripParts {
public:
    explicit TripPortParts(SettingsSides& s) : s_(s) {}
    uint32_t firstTone() override { return s_.fw.cpu.rd32(kSoundingAt); }
    uint32_t nextTone(uint32_t tone) override { return s_.fw.cpu.rd32(tone); }
    void stepTone(uint32_t tone, int32_t count, uint32_t part, uint32_t mask) override {
        const uint32_t self = tone + s_.fw.cpu.rd32(s_.fw.cpu.rd32(tone + kVtableAt) + kStepEntry);
        s_.mine.log.push_back({ kOutTripStep, count, (int32_t)part, (int32_t)mask, (int32_t)self });
    }

private:
    SettingsSides& s_;
};

std::array<Vc2Region, 2> tripRegions(SettingsSides& s) {
    R& r = s.tripRecords;
    return { { { R::kAt, r.records.data(), sizeof r.records, "the records" },
               { R::kScratchAt, &r.scratch, sizeof r.scratch, "the steps' scratch" } } };
}

// An angle as the position gives one, now and then none, half a turn, near or any
int16_t drawAngle(std::mt19937& rng) {
    const int16_t picks[5] = { (int16_t)(4 * (rng() % 129)), -1, 256, (int16_t)(rng() % 1200 - 600), (int16_t)rng() };
    return picks[rng() % 2 ? 0 : 1 + rng() % 4];
}

// The angle after the last: a few steps on, half a turn either way, 0, or any
int16_t drawNextAngle(int16_t last, std::mt19937& rng) {
    const int16_t picks[5] = { (int16_t)(last + 4 * (int32_t)(rng() % 41 - 20)), (int16_t)(last + 256),
                               (int16_t)(last - 256), 0, drawAngle(rng) };
    return picks[rng() % 2 ? 0 : 1 + rng() % 4];
}

// The pad's X or Y, its centre one time in four
int8_t drawPad(std::mt19937& rng) { return (int8_t)(rng() % 4 == 0 ? 64 : rng() % kPadMost); }

// A record's trip words about the routines' edges
void drawTrip(Vc2TripObject& t, std::mt19937& rng) {
    const T& tables = vc2PartSettingsTables();
    const uint32_t moving[3] = { 0, 1, (uint32_t)rng() };
    t.moving = moving[rng() % 3];
    const int8_t ways[4] = { 0, 1, -1, (int8_t)rng() };
    t.turning = ways[rng() % 4];
    const int16_t carries[3] = { (int16_t)(rng() % 141 - 70), 0, (int16_t)rng() };
    t.carry = carries[rng() % 3];
    const size_t rate = rng() % T::kTripRates;
    t.steps = rng() % 2 ? tables.tripSteps[rate] : (int16_t)rng();
    t.offset = rng() % 2 ? tables.tripOffsets[rate] : (int16_t)(rng() % 129 - 64);
    t.lastAngle = drawAngle(rng);
    t.angle = drawNextAngle(t.lastAngle, rng);
    t.x = drawPad(rng);
    t.y = drawPad(rng);
}

// A move aimed at a count of one or two broadcasts' 127 exactly, either way: the angle the move will reach (placed on
// a copy) 127 or 254 from the last, the steps a turn 512, not turning yet, the offset −1 (a turn back's angle from
// then the last itself)
void aimMove(Vc2TripObject& t, int8_t x, int8_t y, std::mt19937& rng) {
    Vc2TripObject to = t;
    vc2TripPlace(to, x, y);
    const auto apart = (int32_t)(127 * (1 + rng() % 2));
    t.lastAngle = (int16_t)(to.angle + (rng() % 2 ? apart : -apart));
    t.steps = 512;
    t.offset = -1;
    t.turning = 0;
}

// The sounding list: up to three tones, each with the tone's vtable
void drawTones(SettingsSides& s, std::mt19937& rng) {
    const auto n = (uint32_t)(rng() % (kTones + 1));
    s.fw.cpu.wr32(kSoundingAt, n != 0 ? kTonesAt : 0);
    for (uint32_t k = 0; k < n; ++k) {
        s.fw.cpu.wr32(kTonesAt + kToneBytes * k, k + 1 < n ? kTonesAt + kToneBytes * (k + 1) : 0);
        s.fw.cpu.wr32(kTonesAt + kToneBytes * k + kVtableAt, kToneVtable);
    }
}

// Steps the rate's division can take
void stepsDivisible(Vc2TripObject& t, std::mt19937& rng) {
    if (t.steps < 1) t.steps = (int16_t)(1 + rng() % kMostSteps);
}

// A routine's way: its arguments for record `k` (and the record made ready), then the port's call and answer
struct TripWay {
    Vc2CheckRoutine routine;
    void (*args)(SettingsSides&, std::mt19937&, int k, Args& a);
    uint32_t (*mine)(SettingsSides&, int k, const Args& a);
};

// [FW] by routine
const TripWay kTripWays[] = {
    { { 0x89394570, "the records' static initializer", nullptr }, [](SettingsSides&, std::mt19937&, int, Args&) {},
      [](SettingsSides& s, int, const Args&) {
          s.tripRecords.staticInit();
          return 0u;
      } },
    { { 0x893b0400, "the constructor", vc2R0 },
      [](SettingsSides&, std::mt19937& rng, int k, Args& a) {
          a = { tripRecordAt(k), rng() % 2 ? (uint32_t)k : (uint32_t)rng(), rng() % 2 ? 0xffffu : (uint32_t)rng(), 0 };
      },
      [](SettingsSides& s, int k, const Args& a) {
          vc2TripConstruct(tripOf(s, k), a[1], a[2]);
          return tripRecordAt(k);
      } },
    { { 0x893b05b0, "the rate", nullptr },
      [](SettingsSides&, std::mt19937& rng, int k, Args& a) {
          a = { tripRecordAt(k), rng() % 4 ? rng() % T::kTripRates : T::kTripRates + rng() % 7, 0, 0 };
      },
      [](SettingsSides& s, int k, const Args& a) {
          vc2TripRate(tripOf(s, k), (int32_t)a[1]);
          return 0u;
      } },
    { { 0x893b0610, "the set", nullptr },
      [](SettingsSides&, std::mt19937& rng, int k, Args& a) {
          const uint32_t ons[3] = { 0, 1, (uint32_t)rng() };
          a = { tripRecordAt(k), ons[rng() % 3], rng() % 256, rng() % 256 };
      },
      [](SettingsSides& s, int k, const Args& a) {
          vc2TripSet(tripOf(s, k), (int32_t)a[1], (int8_t)a[2], (int8_t)a[3]);
          return 0u;
      } },
    { { 0x893b0460, "the move", vc2R0 },
      [](SettingsSides& s, std::mt19937& rng, int k, Args& a) {
          Vc2TripObject& t = tripOf(s, k);
          const int8_t x = drawPad(rng), y = drawPad(rng);
          if (t.moving != 0) stepsDivisible(t, rng);
          if (t.moving != 0 && rng() % 4 == 0) aimMove(t, x, y, rng);
          a = { tripRecordAt(k), (uint8_t)x, (uint8_t)y, 0 };
      },
      [](SettingsSides& s, int k, const Args& a) {
          TripPortParts parts { s };
          const int8_t radius = vc2TripMove(tripOf(s, k), (int8_t)a[1], (int8_t)a[2], s.tripRecords.scratch, parts);
          return (uint32_t)(int32_t)radius;
      } },
    { { 0x893b0690, "the position", nullptr },
      [](SettingsSides& s, std::mt19937& rng, int k, Args& a) {
          tripOf(s, k).x = drawPad(rng);
          tripOf(s, k).y = drawPad(rng);
          a = { tripRecordAt(k), 0, 0, 0 };
      },
      [](SettingsSides& s, int k, const Args&) {
          vc2TripPosition(tripOf(s, k));
          return 0u;
      } },
    { { 0x893b0790, "the steps", vc2R0 },
      [](SettingsSides& s, std::mt19937& rng, int k, Args& a) {
          stepsDivisible(tripOf(s, k), rng);
          a = { tripRecordAt(k), 0, 0, 0 };
      },
      [](SettingsSides& s, int k, const Args&) {
          return (uint32_t)vc2TripSteps(tripOf(s, k), s.tripRecords.scratch);
      } },
    { { 0x893b1780, "the broadcast", nullptr },
      [](SettingsSides&, std::mt19937& rng, int, Args& a) {
          const auto count = rng() % 2 ? (uint32_t)(rng() % 601) - 300u : (uint32_t)rng();
          a = { count, rng() % 2 ? rng() % Vc2TripRecords::kParts : (uint32_t)rng(), 0xffff, 0 };
      },
      [](SettingsSides& s, int, const Args& a) {
          TripPortParts parts { s };
          vc2TripBroadcast((int32_t)a[0], a[1], a[2], parts);
          return 0u;
      } },
};
constexpr size_t kFirstCounted = 74;

// A routine on a record drawn: the static initializer one time in 32
std::string tripEvent(SettingsSides& s, std::mt19937& rng) {
    const size_t way = rng() % 32 == 0 ? 0 : 1 + rng() % (std::size(kTripWays) - 1);
    const int k = (int)(rng() % Vc2TripRecords::kParts);
    if (rng() % 2) drawTrip(tripOf(s, k), rng);
    if (rng() % 8 == 0) settingsNoise(&s.tripRecords.scratch, sizeof s.tripRecords.scratch, rng);
    drawTones(s, rng);
    Args a {};
    kTripWays[way].args(s, rng, k, a);
    settingsWriteRegions(s, tripRegions(s));
    ++s.n.byEvent[kFirstCounted + way];
    const std::string what = settingsOnBoth(s, kTripWays[way].routine, { a[0], a[1], a[2], a[3] },
                                            [&] { return kTripWays[way].mine(s, k, a); });
    return what.empty() ? settingsRegionsDifference(s, tripRegions(s)) : what;
}

const SettingsEvent kTripEvents[] = { tripEvent };

// A case: the records and the scratch noise, then the tables' and the events
std::string tripPartCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    settingsNoise(&s.tripRecords, sizeof s.tripRecords, rng);
    return settingsCaseOf<kTripEvents, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

constexpr SettingsSaid kTripSaid[] = { { 74, "static initializers" }, { 75, "constructors" }, { 76, "rates" },
                                       { 77, "sets" }, { 78, "moves" }, { 79, "positions" }, { 80, "steps" },
                                       { 81, "broadcasts" } };
} // namespace

const SettingsPartRow kTripPartRow { 18, &SettingsCounts::tripEvents, &SettingsCounts::tripCalls, tripPartCase,
                                     nullptr, hookTrip, "Time Trip", kTripSaid };
#endif
