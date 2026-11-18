// xt_fx control: the driver through XtFxControl against the reference, its readers, and live edits (XtFxCheck.h; moved
// from XtFxChip.cpp as it stood, v204).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
// ------------------------------------------------------------------ the control side
// xt_fx <rom> control [events] [seed].  Each part says how many of what it tried were different.
// - Against the reference: the firmware's driver through XtFxControl with the bus straight on JE-8086's chip, whose waits run it, against the same driver through the queue into XtFxChip, whole
//   state each sample.  That's the queue, the copy of the words and the waits; the driver is the same code on both sides.
// - The readers: a patch through the firmware's readers, by ID from the store, against the routines the rest of this file calls one by one, with the same values, word for word.
// - Behaviour: what each kind of change costs on the bus, exactly; every ID a reader reads counts as a change; the Matrix reaches the types that assign it; clamping; no click under a tone.
// - Live edits: edits one at a time through update(), then the same store loaded as a patch into a new one.  The words must end up the same, but for what the firmware itself leaves behind.
// - Threads: the driver on one thread and the samples on another, as fast as they go, each stopping now and then: the stream arrives whole and in order, the waits are kept, and the audio thread
//   allocates nothing.  research/tools/fxthreads.sh runs this part under the thread sanitizer.
void apply(XtFxControl& c, const Edit& e) { if (e.matrix) { c.setMatrix(e.prm[0], e.prm[1], e.prm[2]); return; } if (e.clockMs) { c.outsideClock(e.clockMs); return; } for (const auto& s : e.sets) c.set(s.first, s.second); if (e.patch) c.loadPatch(); else c.update(); }

// The reference with the driver on its bus.  Its waits run the samples, and the samples run both chips.
struct DirectBus : XtFxControl::Bus {
    Reference& ref; std::function<void()> sample;
    explicit DirectBus(Reference& r) : ref(r) {}
    void write(uint8_t c, uint16_t i, uint32_t w) override { ref.hostWrite(0x2003, c); for (uint32_t k = 0; k < 4; k++) ref.hostWrite((uint32_t)i * 4 + k, (uint8_t)(w >> (8 * k))); }
    uint32_t read(uint16_t i) override { write(0x37, i, 0); uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)ref.hostRead(k) << (8 * k); return v & 0x0fffffff; }
    void wait(uint32_t n) override { for (uint32_t i = 0; i < n; i++) sample(); }
};

const FxIds& rom2Ids() {
    static constexpr int kLevels[] = { 0x178, 0x179, 0x17a, 0x1fd, 0x1fe, 0x231, 0x3e, 0x76, 0x51e, 0x11d, 0x120, 0x121,
                                       0x122, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70,
                                       0x78, 0x7c };   // and External Input Type and Source (v204)
    // HARDWARE_FIRMWARE.md, "XtFxControl"
    static constexpr int kAlone[] = { 0x57b, 0x6bb, 0x6bd, 0x5b7, 0x5dd, 0x634, 0x71e };
    static const FxIds ids = { xtVSynthFx(), "control", 0x177, 0x1fc, 0x230, 0x41, 0x11d, kLevels, kAlone };
    return ids;
}

void againstReference(const char* rom, int randomEvents, uint32_t seed, const FxIds& fx) {
    auto ref = std::make_unique<Reference>(); auto chip = std::make_unique<XtFxChip>();
    InputNoise noise(seed);
    long samples = 0, different = 0, firstAt = -1, words = 0;
    std::string what;
    XtFxControl queued(rom, nullptr, fx.image);
    const auto sample = [&] {
        noise.into(*chip, *ref, samples);
        queued.service(*chip); chip->runFrame(); ref->runProgram(768); ref->sync(); samples++;
        const std::string d = differs(*chip, *ref, samples % 2048 == 0);
        if (!d.empty()) { if (!different) { firstAt = samples; what = d; } different++; }
    };
    DirectBus bus(*ref); bus.sample = sample;
    XtFxControl direct(rom, &bus, fx.image);                                     // starts both: its start's waits run the samples
    Editor ed(rom, queued, seed, fx);
    const auto run = [&](const Edit& e, int after) {
        apply(queued, e);
        apply(direct, e);
        for (int i = 0; i < after; i++) sample();
        if (!queued.drained()) return;
        for (unsigned i = 0; i < 2048; i++) {
            if ((chip->word(i) & 0x0fffffff) == (queued.word(i) & 0x0fffffff)) continue;
            words++;
            break;
        }
    };
    const auto everyType = [&](Edit (Editor::*type)(int), uint32_t table, int count) {
        for (int t = 1; t <= count; t++) {
            run((ed.*type)(t % count), 128);
            Edit e;
            ed.params(e, table, t % count, 3);
            run(e, 128);
        }
    };
    everyType(&Editor::mfxType, fx.image.mfxIds, fx.image.mfxTypes);
    everyType(&Editor::choType, fx.image.choIds, fx.image.choTypes);
    everyType(&Editor::revType, fx.image.revIds, fx.image.revTypes);
    for (int i = 0; i < randomEvents; i++) {
        run(ed.any(), ed.r.below(4) ? ed.r.below(64) : ed.r.below(2048));
        if (i % 8 != 3) continue;
        Edit source;   // an outside clock, on and off
        source.sets.push_back({ fx.clockSource, ed.r.below(4) ? 1 : 0 });
        run(source, 16);
        Edit clock;
        clock.clockMs = 60 + ed.r.below(400);
        run(clock, 16);
    }
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  %s: first at sample %ld, %s\n", fx.label, firstAt, what.c_str());
    const bool ran = queued.ok() && direct.ok();
    if (!ran) std::printf("  %s: a fault: %s %s\n", fx.label, queued.error().c_str(), direct.error().c_str());
    const long wrong = different + words + (ran ? 0 : 1);
    std::printf("%s against the reference: %ld samples, %ld different\n", fx.label, samples, wrong);
}

// A patch through the firmware's readers, by ID, against Rig's routines one by one.  Where they should differ, the Rig is told what it leaves out: the tempo, and that a Chorus of type Off keeps its
// return shut however its switch stands.  And the system's EQ, which the firmware starts with on, goes off.
void readers(const char* rom, uint32_t seed) {
    int cases = 0, different = 0; std::string first;
    for (int t = 0; t < 62; t++) {
        const int mfx = t < 42 ? t : 0, rev = t < 42 ? t % 11 : t < 53 ? t - 42 : 0, cho = t < 42 ? t % 9 : t < 53 ? 0 : t - 53;
        XtFxControl c(rom); Editor ed(rom, c, seed + (uint32_t)t); std::map<int, int> forced;
        const auto draw = [&](uint32_t table, int type) { const int f = ed.s16(table + 4 * (uint32_t)type), l = ed.s16(table + 4 * (uint32_t)type + 2); for (int id = f; id <= l; id++) { forced[id] = ed.draw(id); c.set(id, forced[id]); } };
        draw(kMfxIds, mfx); draw(kRevIds, rev); draw(kChoIds, cho);
        c.set(0x177, mfx); c.set(0x230, rev); c.set(0x1fc, cho); c.set(0x120, 1); c.set(0x121, 1); c.set(0x122, 1); c.set(0x70, 0); c.update(); c.loadPatch();
        Rig g(rom, false); g.forced = &forced;
        g.boot(mfx, rev, cho, nullptr);
        g.call(0x894d34e0, { 1200 }); g.call(0x894d0b50, { 1200 }); g.call(0x894affe0, { cho ? 1u : 0u });
        int d = 0; unsigned at = 0; uint32_t x = 0, y = 0;
        for (unsigned i = 0; i < 2048; i++) { const uint32_t a = g.chip.word(i) & 0x0fffffff, b = c.word(i) & 0x0fffffff; if (a != b && !d++) { at = i; x = a; y = b; } }
        if (d && first.empty()) { char s[120]; std::snprintf(s, sizeof s, "MFX %d, Reverb %d, Chorus %d: word %03x, %07x against %07x", mfx, rev, cho, at, y, x); first = s; }
        if (!c.ok()) { first = c.error(); d++; }
        cases++; different += d ? 1 : 0;
    }
    if (different && xtSwitch("XT_FX_FIRST")) std::printf("  readers: first %s\n", first.c_str());
    std::printf("control's readers against the routines one by one: %d samples, %d different\n", cases, different);
}

// Edits one at a time through update(), then the same store as a patch into a new control.  Types have their defaults and the edits their own values, so anything set() lost shows.
void liveEdits(const char* rom, int events, uint32_t seed, const FxIds& fx) {
    int cases = 0, different = 0; std::string first;
    const auto isAlone = [&](const std::pair<int, int>& x) {
        return std::ranges::find(fx.alone, x.first) != fx.alone.end();
    };
    for (int run = 0; run < 12; run++) {
        XtFxControl live(rom, nullptr, fx.image);
        Editor ed(rom, live, seed + 1000 + (uint32_t)run, fx);
        for (int i = 0; i < events; i++) {
            Edit e = ed.any();
            e.patch = false;
            if (e.matrix) continue;                                              // the Matrix isn't in the store, so a new patch wouldn't have it
            // Nor do these parameters reach the words alone, in the firmware's own update: a new patch has them and a live edit doesn't (HARDWARE_FIRMWARE.md, "XtFxControl").
            std::erase_if(e.sets, isAlone);
            apply(live, e);
        }
        XtFxControl fresh(rom, nullptr, fx.image);
        for (int id = 0; id < fx.image.ids; id++) fresh.set(id, live.get(id));
        fresh.loadPatch();
        // The firmware leaves two things behind.  Reverb types Off and Delay don't write the pre-EQ (A 0x138…0x156), so it's what the last type left, and a new patch hasn't had one.  So through Reverb 1
        // on both, and back.  And Reverb Delay's note values use the tempo it last heard, which it only hears as the current type.  So the tempo away and back, on both.
        const int rev = live.get(fx.revType), tempo = live.get(fx.tempo);
        const std::pair<int, int> away[] = { { fx.revType, rev == 1 ? 2 : 1 }, { fx.revType, 1 }, { fx.revType, rev },
                                             { fx.tempo, tempo == 2500 ? tempo - 1 : tempo + 1 }, { fx.tempo, tempo } };
        for (XtFxControl* c : { &live, &fresh }) {
            for (const auto& step : away) {
                Edit e;
                e.sets.push_back(step);
                apply(*c, e);
            }
        }
        int d = 0; unsigned at = 0; uint32_t x = 0, y = 0;
        for (unsigned i = 0; i < 2048; i++) { const uint32_t a = live.word(i) & 0x0fffffff, b = fresh.word(i) & 0x0fffffff; if (a != b && !d++) { at = i; x = a; y = b; } }
        if (d && first.empty()) { char s[100]; std::snprintf(s, sizeof s, "run %d: %d words, first %03x, live %07x, patch %07x", run, d, at, x, y); first = s; }
        if (!live.ok() || !fresh.ok()) { first = live.error() + fresh.error(); d = 1; }
        if (d && xtSwitch("XT_FX_LIVE")) {
            std::printf("  run %d, MFX %d, Chorus %d, Reverb %d:", run, live.get(fx.mfxType), live.get(fx.choType),
                        live.get(fx.revType));
            for (unsigned i = 0; i < 2048; i++) {
                const uint32_t a = live.word(i) & 0x0fffffff, b = fresh.word(i) & 0x0fffffff;
                if (a != b) std::printf(" %c%03x %07x/%07x", "AB"[i >> 10], i & 1023, a, b);
            }
            std::printf("\n");
        }
        cases++; different += d ? 1 : 0;
    }
    if (different && xtSwitch("XT_FX_FIRST")) std::printf("  live edits: %s\n", first.c_str());
    std::printf("%s's live edits against a patch: %d samples, %d different\n", fx.label, cases, different);
}
} // namespace xtfxcheck
#endif
