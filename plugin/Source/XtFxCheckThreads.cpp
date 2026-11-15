// xt_fx control's threads, and what the waits and mutes are for (XtFxCheck.h; moved from XtFxChip.cpp as it stood,
// v204).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
thread_local bool tAudioThread = false;
std::atomic<uint64_t> gAudioAllocations{0};

// What the audio thread sees.  No allocation here: what it records has its room already.
struct Tap {
    std::vector<uint32_t>& at; uint32_t crc = 0; uint64_t n = 0; uint32_t sample = 0; bool overflow = false;
    static void observe(void* context, uint8_t c, uint16_t i, uint32_t w) {
        Tap& t = *static_cast<Tap*>(context); t.crc = Recorder::mix(Recorder::mix(Recorder::mix(t.crc, c), i), w);
        if (t.n < t.at.size()) t.at[t.n] = t.sample; else t.overflow = true;
        t.n++;
    }
};

void threads(const char* rom, int events, uint32_t seed) {
    // A burst too big for the ring while the audio thread's stopped, then the rest with both going.
    std::vector<Edit> edits;
    { XtFxControl c(rom); Editor ed(rom, c, seed + 7); for (int i = 0; i < events; i++) edits.push_back(ed.any()); for (int i = 0; i < 3000; i++) { Edit e; const int id = 0x178 + (i & 1); e.sets.push_back({ id, i % 128 }); edits.push_back(e); } }
    // The order it'll go in, the burst in the middle, recorded with one thread and no queue.
    const size_t random = (size_t)events;
    Recorder expect; { XtFxControl e(rom, &expect);
        for (size_t i = 0; i < edits.size(); i++) { if (i == random / 2) for (size_t k = 0; k < 3000; k++) apply(e, edits[random + k]); if (i >= random && i < random + 3000) continue; apply(e, edits[i]); } }
    auto chip = std::make_unique<XtFxChip>(); std::vector<uint32_t> at((size_t)expect.n + 1024);
    Tap tap{ at };
    XtFxControl c(rom); c.observe(Tap::observe, &tap);
    std::atomic<bool> finished{false}; std::atomic<int> hold{0};                 // hold: 1, the audio thread waits; 2, it's waiting
    std::thread control([&] {
        Rng r(seed + 11);
        for (size_t i = 0; i < edits.size(); i++) {
            if (i == random / 2) { hold = 1; while (hold.load() != 2) std::this_thread::yield(); for (size_t k = 0; k < 3000; k++) apply(c, edits[random + k]); hold = 0; }
            if (i >= random && i < random + 3000) continue;
            apply(c, edits[i]);
            if (r.below(8) == 0) std::this_thread::sleep_for(std::chrono::microseconds(r.below(2000)));
        }
        while (!c.drained()) { c.pump(); std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        finished = true;
    });
    tAudioThread = true;
    Rng r(seed + 13); uint64_t samples = 0;
    while (!finished.load()) {
        if (hold.load() == 1) { hold = 2; while (hold.load() == 2) std::this_thread::yield(); }
        c.service(*chip); chip->runFrame(); tap.sample++; samples++;
        if (r.below(4096) == 0) std::this_thread::sleep_for(std::chrono::microseconds(r.below(2000)));
    }
    tAudioThread = false;
    control.join();
    long short_ = 0; for (const auto& w : expect.waits) if (w.first > 0 && w.first < tap.n && w.first < at.size() && at[(size_t)w.first] - at[(size_t)w.first - 1] < w.second) short_++;
    long words = 0; for (unsigned i = 0; i < 2048; i++) if ((chip->word(i) & 0x0fffffff) != (c.word(i) & 0x0fffffff)) words++;
    const long lost = (long)expect.n - (long)tap.n, wrong = tap.crc != expect.crc ? 1 : 0, allocations = (long)gAudioAllocations.load();
    if (xtSwitch("XT_FX_FIRST") && (lost || wrong || short_ || words || allocations))
        std::printf("  threads: %ld lost, %ld out of order, %ld waits cut short, %ld words, %ld allocations\n", lost,
                    wrong, short_, words, allocations);
    if (!c.ok()) std::printf("  threads: a fault: %s\n", c.error().c_str());
    std::printf("control under threads: %zu edits, %ld different\n", edits.size(), (lost != 0) + wrong + short_ + words + allocations + (tap.overflow ? 1 : 0) + (c.ok() ? 0 : 1));
    (void)samples;
}

// What the waits and the mutes are for: a type change and a patch under a tone, the largest step between two samples against the tone's own, with the driver's waits and with the words all at once.
struct Immediate : XtFxControl::Bus {
    XtFxChip& chip; explicit Immediate(XtFxChip& x) : chip(x) {}
    void write(uint8_t c, uint16_t i, uint32_t w) override { chip.hostWord(c, i, w); }
    uint32_t read(uint16_t i) override { write(0x37, i, 0); uint32_t v = 0; for (uint32_t k = 0; k < 4; k++) v |= (uint32_t)chip.hostRead(k) << (8 * k); return v & 0x0fffffff; }
    void wait(uint32_t) override {}
};
double stepUnderTone(const char* rom, bool honoured, bool patch) {
    auto chip = std::make_unique<XtFxChip>(); Immediate imm(*chip);
    XtFxControl c(rom, honoured ? nullptr : &imm);
    c.set(0x120, 1); c.set(0x177, 11); c.loadPatch();                                                     // from a Stereo Delay, full of the tone, to an overdrive
    double last = 0, most = 0, tone = 0;
    for (int i = 0; i < 60000; i++) {
        if (i == 40000) { c.set(0x177, 5); if (patch) c.loadPatch(); else c.update(); }
        const int32_t v = (int32_t)std::lrint(0.25 * std::sin(2 * M_PI * 100 * i / 44100.0) * 8388608.0);
        c.service(*chip);
        for (unsigned s = 0x20; s <= 0x2e; s += 2) chip->setGram(s, s == 0x28 || s == 0x2a ? v : 0);
        chip->runFrame();
        const double y = (double)chip->gram(0xa1), d = std::fabs(y - last); last = y;
        if (i > 20000 && i < 40000) tone = std::max(tone, d);
        if (i >= 40000) most = std::max(most, d);
    }
    return db(most / tone);
}
void changeUnderTone(const char* rom) {
    std::printf("under a tone, the largest step against the tone's own: an MFX type change %+.1f dB, a patch %+.1f dB; with the words all at once, %+.1f dB, %+.1f dB\n",
                stepUnderTone(rom, true, false), stepUnderTone(rom, true, true), stepUnderTone(rom, false, false), stepUnderTone(rom, false, true));
}
} // namespace xtfxcheck
#endif
