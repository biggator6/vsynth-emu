// xt_vc2settings' fourteenth part (v300): the reload objects, Vc2PartReloads, against rom.4's own routines on its own 16
// objects (0x89996120) and the three floats after them, on the seed thirteen after the run's.  Each case every table,
// the objects and the floats noise, each object's part 0…15, its state 0, 1 or 2 mostly, now and then any byte, its
// posted word 0, 1 or any; before each event the object's words drawn so half the time, and the part's read's answer
// drawn.  Hooked to log: the queue's add 0x893004a0 and remove 0x89300640 (the held flags as they stand at the remove)
// and the part's read 0x8939b170 (v292's, answering as drawn); the eleventh part's hooks on the request and the run
// taken off.  Events: the static initializer one time in 32 (it makes
// every object again; its array builder 0x8950773a rom.4's own), else the constructor in place, the cancel, the run,
// the request, the timer's fire or the notes' message on a drawn object.  After each the tables, the objects, the
// floats, the held flags and the calls, and the run's and the fire's answer.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
using R = Vc2PartReloads;
constexpr uint32_t kObjectsBytes = sizeof(Vc2Reload) * R::kObjects;
constexpr size_t kHeldRemoves = 62;

SettingsCall postCalled(Sh4& c) { return { kOutPost, 0, 0, 0, (int32_t)c.r[5] }; }
SettingsCall unpostCalled(Sh4& c) {
    Vc2HeldFlags held {};
    for (size_t k = 0; k < held.size(); ++k) held[k] = c.rd8(kHeldAt + (uint32_t)k);
    return settingsUnposted(held.data(), c.r[5]);
}
SettingsCall readCalled(Sh4& c) { return { kOutReadPart, 0, 0, 0, (int32_t)c.r[4] }; }
uint32_t readAnswered(SettingsSides& s) { return s.outs.readAnswer ? 1u : 0u; }
constexpr SettingsCallee kReloadCallees[] = { { 0x893004a0, postCalled }, { 0x89300640, unpostCalled },
                                              { 0x8939b170, readCalled, readAnswered } };
constexpr uint32_t kReloadOwn[] = { 0x8939af40, 0x8939af10 };   // [FW] the request and the run, the eleventh's hooks

void hookReloads(SettingsSides& s) { settingsHookCallees(s, kReloadCallees, kReloadOwn); }

// [FW] a routine on an object: its address, the port's, whether its answer is compared, and its count
struct ReloadRoutine {
    uint32_t at;
    const char* name;
    uint32_t (*mine)(R&, uint32_t);
    bool answers;
    size_t counted;
};
constexpr ReloadRoutine kStaticInit { 0x8939b350, "the static initializer", [](R& p, uint32_t) {
    p.staticInit();
    return 0u;
}, false, 55 };
constexpr ReloadRoutine kRoutines[] = {
    { 0x8939b260, "the constructor", [](R& p, uint32_t o) {
        p.construct(o);
        return 0u;
    }, false, 56 },
    { 0x8939ae50, "the cancel", [](R& p, uint32_t o) {
        p.cancel(o);
        return 0u;
    }, false, 57 },
    { 0x8939af10, "the run", [](R& p, uint32_t o) { return p.run(o) ? 1u : 0u; }, true, 58 },
    { 0x8939af40, "the request", [](R& p, uint32_t o) {
        p.request(o);
        return 0u;
    }, false, 59 },
    { 0x8939b020, "the timer's fire", [](R& p, uint32_t o) { return (uint32_t)p.fire(o); }, true, 60 },
    { 0x8939b050, "the notes' message", [](R& p, uint32_t o) {
        p.message(o);
        return 0u;
    }, false, 61 },
};

// An object's part 0…15, its state 0, 1 or 2 mostly, now and then any byte, its posted word 0, 1 or any
void drawReload(Vc2Reload& r, std::mt19937& rng) {
    r.part = (uint8_t)(rng() % R::kObjects);
    const std::array<uint8_t, 4> states { 0, 1, 2, (uint8_t)rng() };
    r.state = states[rng() % states.size()];
    const std::array<uint32_t, 3> posts { 0, 1, (uint32_t)rng() };
    r.posted = posts[rng() % posts.size()];
}

// A routine on both sides, the objects and floats into rom.4's memory first; then the held flags, the objects and the
// floats
std::string reloadEvent(SettingsSides& s, std::mt19937& rng) {
    const ReloadRoutine& row = rng() % 32 == 0 ? kStaticInit : kRoutines[rng() % std::size(kRoutines)];
    const auto k = (uint32_t)(rng() % R::kObjects);
    const uint32_t object = R::kAt + (uint32_t)sizeof(Vc2Reload) * k;
    R& p = s.reloads;
    if (rng() % 2 == 0) drawReload(p.objects[k], rng);
    s.outs.readAnswer = rng() % 2 == 0;
    ++s.n.byEvent[row.counted];
    s.fw.cpu.write(R::kAt, p.objects.data(), kObjectsBytes);
    s.fw.cpu.write(R::kFloatsAt, p.floats.data(), sizeof p.floats);
    const std::string what = settingsOnBoth(s, { row.at, row.name, row.answers ? vc2R0 : nullptr }, { object },
                                            [&] { return row.mine(p, object); });
    for (const SettingsCall& c : s.mine.log) s.n.byEvent[kHeldRemoves] += c.block == kOutUnpost && c.kind != 0;
    if (!what.empty()) return what;
    for (size_t b = 0; b < s.mine.flags.size(); ++b)
        if (s.fw.cpu.rd8(kHeldAt + (uint32_t)b) != s.mine.flags[b]) return std::string(row.name) + ": the held flags";
    return vc2RegionsDifference(s.fw, { { R::kAt, p.objects.data(), kObjectsBytes, "the reload objects" },
                                        { R::kFloatsAt, p.floats.data(), sizeof p.floats, "their floats" } });
}

const SettingsEvent kReloadEvents[] = { reloadEvent };

// A case: the objects and floats noise, each object's words drawn; then the tables' and the events
std::string reloadCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    R& p = s.reloads;
    settingsNoise(p.objects.data(), kObjectsBytes, rng);
    for (Vc2Reload& r : p.objects) drawReload(r, rng);
    settingsNoise(p.floats.data(), sizeof p.floats, rng);
    return settingsCaseOf<kReloadEvents, sizeof(Vc2PartRows), 5>(s, rng, events, calls);
}

constexpr SettingsSaid kReloadSaid[] = { { 55, "static initializers" }, { 56, "constructors" }, { 57, "cancels" },
                                         { 58, "runs" }, { 59, "requests" }, { 60, "fires" }, { 61, "messages" },
                                         { 62, "removes held" } };
} // namespace

const SettingsPartRow kReloadPartRow { 13, &SettingsCounts::reloadEvents, &SettingsCounts::reloadCalls, reloadCase,
                                       nullptr, hookReloads, "reload objects", kReloadSaid };
#endif
