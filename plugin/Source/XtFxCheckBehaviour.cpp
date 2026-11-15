// xt_fx control's behaviour: what each kind of change costs on the bus (XtFxCheck.h; moved from XtFxChip.cpp as it
// stood, v204).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
// What each kind of change costs on the bus: a live edit stays small and doesn't wait, a value that hasn't changed costs nothing, a type change mutes before it swaps, a patch reloads.
void behaviour(const char* rom) {
    int tried = 0, different = 0;
    const bool first = xtSwitch("XT_FX_FIRST") != nullptr, show = xtSwitch("XT_FX_COUNTS") != nullptr;
    const auto check = [&](bool good, const char* what) { tried++; if (!good) { different++; if (first) std::printf("  behaviour: %s\n", what); } };
    Recorder rec; XtFxControl c(rom, &rec);
    c.set(0x120, 1); c.set(0x121, 1); c.set(0x122, 1); c.set(0x177, 1); c.set(0x1fc, 1); c.set(0x230, 1); c.loadPatch();
    // A live edit: this many words on the bus, no fewer and no more (a reload or a ramp would add to them), and no wait.
    const auto live = [&](int id, int v, uint64_t words, const char* what) {
        const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(id, v); c.update();
        if (show) std::printf("  %-20s %3llu words, %zu waits\n", what, (unsigned long long)(rec.n - n), rec.waits.size() - w);
        check(rec.n - n == words && rec.waits.size() == w, what);
    };
    live(0x55e + 4, 20, 16, "an MFX parameter"); live(0x7c7, 40, 1, "a Chorus parameter"); live(0x80e, 20, 3, "a Reverb parameter");
    live(0x178, 90, 4, "MFX Master"); live(0x179, 90, 1, "MFX Chorus Send"); live(0x17a, 90, 1, "MFX Reverb Send"); live(0x1fd, 90, 2, "Chorus Master"); live(0x1fe, 90, 1, "Chorus to Reverb"); live(0x231, 90, 2, "Reverb Master");
    live(0x3e, 90, 2, "Master Level"); live(0x51e, 1, 4, "Attenuation"); live(0x70, 0, 47, "EQ Switch"); live(0x70, 1, 7, "EQ Switch back"); live(0x66, 20, 7, "an EQ gain");
    // The tempo's debounce: a new tempo reaches the chip after holding for two reads, 80 ms apart, and a switch's edit goes at once and then waits one.
    const auto waits = [&](int id, int v, size_t count, const char* what) {
        const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(id, v); c.update();
        if (show) std::printf("  %-20s %3llu words, %zu waits\n", what, (unsigned long long)(rec.n - n), rec.waits.size() - w);
        bool good = rec.waits.size() - w == count && rec.n > n; for (size_t i = w; i < rec.waits.size(); i++) good = good && rec.waits[i].second == 3528; check(good, what);
    };
    c.set(0x1fc, 6); c.update(); waits(0x11d, 1400, 2, "a new tempo"); waits(0x120, 0, 1, "MFX Switch"); waits(0x120, 1, 1, "MFX Switch back"); waits(0x121, 0, 1, "Chorus Switch"); waits(0x121, 1, 1, "Chorus Switch back"); waits(0x122, 0, 1, "Reverb Switch"); waits(0x122, 1, 1, "Reverb Switch back");
    { const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(0x76, 3); c.update(); check(rec.n > n && rec.waits.size() > w, "Output Gain"); }
    // A value that hasn't changed: nothing at all.
    const auto same = [&](int id, const char* what) { const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.set(id, c.get(id)); c.update(); check(rec.n == n && rec.waits.size() == w && !c.pending(), what); };
    same(0x178, "a level unchanged"); same(0x120, "a switch unchanged"); same(0x177, "a type unchanged"); same(0x3e, "Master unchanged"); same(0x1fc, "the Chorus's type unchanged");
    // A type change ramps down and waits before it loads; a patch mutes, and loads the same type again.
    { const uint64_t n = rec.n; c.set(0x177, 2); const size_t w = rec.waits.size(); c.update(); check(rec.waits.size() > w && rec.waits[w].first - n < 64 && rec.waits[w].second > 128, "an MFX type change doesn't ramp down and wait first"); }
    { const uint64_t n = rec.n; const size_t w = rec.waits.size(); c.loadPatch(); check(rec.waits.size() > w && rec.waits[w].first - n < 64 && rec.waits[w].second > 128, "a patch doesn't mute first"); if (show) std::printf("  a patch: %llu words\n", (unsigned long long)(rec.n - n)); check(rec.n - n > 2000, "a patch doesn't reload"); }
    // set() keeps to the descriptor's range, and everything the readers read counts as a change.
    c.set(0x177, 1000); check(c.get(0x177) == 41, "a type above its range"); c.set(0x177, -5); check(c.get(0x177) == 0, "a type below its range"); c.set(0x76, 99); check(c.get(0x76) == 12, "Output Gain above its range"); c.set(0x76, -99); check(c.get(0x76) == -12, "Output Gain below its range");
    c.set(0x8000, 1); check(c.get(0x8000) == 0, "an ID that isn't there");
    c.update();
    static const int fixed[] = { 0x11d, 0x120, 0x121, 0x122, 0x177, 0x178, 0x179, 0x17a, 0x1fc, 0x1fd, 0x1fe, 0x230, 0x231, 0x3e, 0x76, 0x51e, 0x41, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70 };
    int unseen = 0; const auto seen = [&](int id) { if (c.highest(id) == c.lowest(id)) return; const int v = c.get(id); c.set(id, v == c.highest(id) ? c.lowest(id) : v + 1); if (!c.pending()) unseen++; c.set(id, v); c.update(); };
    for (const int id : fixed) seen(id);
    for (int id = 0x55d; id <= 0x8c6; id++) seen(id);
    if (first && unseen) std::printf("  behaviour: %d IDs set() doesn't count\n", unseen);
    check(unseen == 0, "an ID the readers read that set() doesn't count");
    // The Matrix: each of the three reaches the types that assign it, and moving a parameter puts words on the bus.  An unchanged value, or a type that doesn't assign it, costs nothing.
    {
        int responds[3] = {}, thru = 0;
        for (int t = 0; t < 42; t++) for (int k = 0; k < 3; k++) {
            Recorder m; XtFxControl a(rom, &m); a.set(0x177, t); a.loadPatch();
            const uint64_t n = m.n; int v[3] = {}; v[k] = 20000; a.setMatrix(v[0], v[1], v[2]);
            if (m.n != n) responds[k]++;
            if (t == 0 && m.n != n) thru++;
            const uint64_t after = m.n; a.setMatrix(v[0], v[1], v[2]); check(m.n == after, "the Matrix, unchanged");
            v[k] = 0; a.setMatrix(v[0], v[1], v[2]); if (m.n != after) check(a.ok(), "the Matrix back to 0");
        }
        if (show) std::printf("  the Matrix: types that respond to PRM1 %d, PRM2 %d, PRM3 %d\n", responds[0], responds[1], responds[2]);
        check(responds[0] == 32 && responds[1] == 22 && responds[2] == 6, "the Matrix doesn't reach the types it should"); check(thru == 0, "Thru answers the Matrix");
    }
    // The catalog: the types and what each lists.  A step of each parameter, with its type loaded, goes on the bus, but for the eight that HARDWARE_FIRMWARE.md names.
    {
        const XtFxCatalog cat = xtFxCatalog(rom);
        check(cat.ok && cat.mfx.size() == 42 && cat.chorus.size() == 9 && cat.reverb.size() == 11 && cat.mfxTypes.size() == 42 && cat.chorusTypes.size() == 9 && cat.reverbTypes.size() == 11 && cat.eq.size() == 12, "the catalog's size");
        check(cat.ok && cat.mfxTypes[0] == "Thru" && cat.mfxTypes[3] == "Resonant Filter" && cat.mfxTypes[41] == "Pseudo Stereo", "the MFX types' names");
        int listed = 0, silent = 0, badName = 0, badRange = 0;
        const auto step = [&](Recorder& m, XtFxControl& a, const XtFxParam& p) {
            listed++;
            if (p.name.empty() || p.name[0] == ' ' || p.name.back() == ' ' || p.name.find('*') != std::string::npos) badName++;
            if (p.low != a.lowest(p.id) || p.high != a.highest(p.id) || p.low >= p.high) badRange++;
            const uint64_t n = m.n; const int v = a.get(p.id); a.set(p.id, v == p.high ? v - 1 : v + 1); a.update();
            if (m.n == n) silent++;
        };
        const auto types = [&](int typeId, int switchId, const std::vector<std::vector<XtFxParam>>& list) {
            for (size_t t = 0; t < list.size(); t++) { Recorder m; XtFxControl a(rom, &m); a.set(switchId, 1); a.set(typeId, (int)t); a.loadPatch(); for (const XtFxParam& p : list[t]) step(m, a, p); }
        };
        types(0x177, 0x120, cat.mfx); types(0x1fc, 0x121, cat.chorus); types(0x230, 0x122, cat.reverb);
        { Recorder m; XtFxControl a(rom, &m); for (const XtFxParam& p : cat.eq) step(m, a, p); }
        if (show) std::printf("  the catalog: %d parameters, %d silent\n", listed, silent);
        check(listed == 872, "the catalog lists the wrong parameters"); check(silent == 8, "the catalog's parameters on the bus"); check(badName == 0, "a name in the catalog"); check(badRange == 0, "a range in the catalog");
    }
    // The outside clock: the firmware's poll goes by Clock Source, and moves only past its margin, here 2.5 BPM plus 1% of 120.
    {
        Recorder m; XtFxControl a(rom, &m); a.set(0x120, 1); a.set(0x177, 11); a.loadPatch();
        const auto poll = [&](int ms, bool moves, const char* what) { const uint64_t n = m.n; a.outsideClock(ms); check((m.n != n) == moves && a.ok(), what); };
        poll(100, false, "the outside clock with Clock Source at 0");
        a.set(0x41, 1); a.update();
        poll(125, true, "the outside clock's first"); poll(125, false, "the outside clock, the same");
        poll(0, false, "the outside clock at no time");
        poll(122, false, "the outside clock within its margin, up"); poll(128, false, "the outside clock within its margin, down");
        poll(129, true, "the outside clock past its margin, down");
        poll(125, true, "the outside clock past its margin, up");
        a.set(0x41, 0); a.update();
        poll(90, false, "the outside clock with Clock Source back at 0");
    }
    // tick(): a message at a time, so a stream of edits doesn't run ahead of the chip by what the driver's waits add up to, and the last still lands.  And what it sends is what update() or loadPatch() would.
    {
        auto chip = std::make_unique<XtFxChip>(); XtFxControl a(rom); a.set(0x120, 1); a.set(0x177, 11); a.loadPatch();
        while (!a.drained()) { a.pump(); a.service(*chip); chip->runFrame(); }
        int tempo = 0;
        for (int i = 0; i < 200; i++) { tempo = 600 + i * 7; a.set(0x11d, tempo); a.tick(); a.pump(); for (int s = 0; s < 441; s++) { a.service(*chip); chip->runFrame(); } }
        long behind = 0;
        for (; !a.drained() && behind < 88200; behind++) { a.pump(); a.service(*chip); chip->runFrame(); }
        a.tick();
        for (; !a.drained() && behind < 88200; behind++) { a.pump(); a.service(*chip); chip->runFrame(); }
        if (show) std::printf("  a stream of edits: %.2f s behind at its end\n", (double)behind / 44100.0);
        check(behind < 17640, "a stream of edits runs ahead of the chip");
        XtFxControl b(rom); b.set(0x120, 1); b.set(0x177, 11); b.set(0x11d, tempo); b.loadPatch();
        int d = 0; for (unsigned i = 0; i < 2048; i++) if (a.word(i) != b.word(i)) d++;
        check(d == 0, "the last of a stream of edits");
        Recorder x, y; XtFxControl e(rom, &x), f(rom, &y);
        e.set(0x177, 11); f.set(0x177, 11); e.wantPatch(); e.tick(); f.loadPatch(); check(x.crc == y.crc, "a patch by tick()");
        const uint64_t n = x.n; e.tick(); check(x.n == n, "tick() sends a patch twice");
        e.set(0x178, 90); f.set(0x178, 90); e.tick(); f.update(); check(x.crc == y.crc && x.n != n, "an edit by tick()");
    }
    // What's the system's survives a patch, and the start reads it.
    { XtFxControl a(rom), b(rom); a.set(0x3e, 64); a.update(); a.loadPatch(); b.set(0x3e, 64); b.loadPatch(); int d = 0; for (unsigned i = 0; i < 2048; i++) if (a.word(i) != b.word(i)) d++; check(d == 0, "a system edit lost to a patch"); }
    { XtFxControl a(rom), b(rom); b.set(0x70, 0); b.update(); b.set(0x70, 1); b.update(); int d = 0; for (unsigned i = 0; i < 2048; i++) if (a.word(i) != b.word(i)) d++; check(d == 0, "the start doesn't read the system"); }
    // Each thing the check counts on, a type at the end of its range: types 41, 8 and 10 load.
    { XtFxControl a(rom); a.set(0x177, 41); a.set(0x1fc, 8); a.set(0x230, 10); a.update(); check(a.ok(), "the last types fault"); }
    // The waits and the mutes do what they're for.  (0 dB would be no bigger than the tone's own step; without them it's over 30.)
    check(stepUnderTone(rom, true, false) < 6.0, "a type change under a tone clicks"); check(stepUnderTone(rom, true, true) < 6.0, "a patch under a tone clicks");
    if (!c.ok() && first) std::printf("  behaviour: a fault: %s\n", c.error().c_str());
    std::printf("control's behaviour on the bus: %d samples, %d different\n", tried, different + (c.ok() ? 0 : 1));
}
} // namespace xtfxcheck
#endif
