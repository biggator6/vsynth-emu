// xt_fx numbers and speed: the firmware's own numbers on XtFxChip, and what it costs (XtFxCheck.h; moved from
// XtFxChip.cpp as it stood, v204).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
// ------------------------------------------------------------------ the firmware's own numbers
struct Tone { double in = 0, peak[2] = {}; };
Tone sine(Rig& g, unsigned slot, double f, double amp, int n, unsigned outL = 0xa0) {
    Tone t;
    for (int i = 0; i < n; i++) {
        const int32_t v = (int32_t)std::lrint(amp * std::sin(2 * M_PI * f * i / 44100.0) * 8388608.0);
        for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, s == slot || s == slot + 2 ? v : 0);
        const uint32_t p = g.chip.position();
        g.chip.runFrame();
        if (i > n / 2) { t.in = std::max(t.in, std::fabs((double)v)); for (unsigned k = 0; k < 2; k++) t.peak[k] = std::max(t.peak[k], std::fabs((double)g.chip.gram(outL + 2 * k - (g.chip.position() - p)))); }
    }
    return t;
}
double gain(const char* rom, int mfx, unsigned slot, double f, double amp, const char* over = nullptr, unsigned out = 0xa0) {
    Rig g(rom, false); g.boot(mfx, 0, 0, nullptr, over);
    const Tone t = sine(g, slot, f, amp, 20000, out);
    return db(t.peak[0] / t.in);
}
// Where a click comes out: the first arrival, then the largest peaks after it, from the dry.
std::string echoes(const char* rom, int mfx, int samples, int ch) {
    Rig g(rom, false); g.boot(mfx, 0, 0, nullptr);
    std::vector<double> y;
    const int t0 = 100000;                                                       // past the delay's glide
    for (int i = 0; i < t0 + samples; i++) {
        const int32_t v = i == t0 ? 2097152 : 0;
        for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, s == 0x28 || s == 0x2a ? v : 0);
        g.chip.runFrame();
        if (i >= t0) y.push_back(std::fabs((double)g.chip.gram(0xa1 + 2 * (unsigned)ch)));
    }
    int dry = 0; while (dry < (int)y.size() && y[(size_t)dry] < 100000.0) dry++;
    std::string s = "dry +" + std::to_string(dry);
    double top = 0; for (size_t i = (size_t)dry + 1000; i < y.size(); i++) top = std::max(top, y[i]);
    int shown = 0; for (size_t i = (size_t)dry + 1000; i < y.size() && shown < 3; i++) if (y[i] > 0.1 * top) { s += ", echo +" + std::to_string((int)i - dry); shown++; i += 500; }
    return s;
}
void numbers(const char* rom) {
    std::printf("MAIN to the mix: %+.2f dB\n", gain(rom, 0, 0x20, 1000, 0.25));
    std::printf("MULTI through Thru: %+.2f dB\n", gain(rom, 0, 0x28, 1000, 0.25));
    std::printf("the sends: into the Reverb, slot 72, %+.2f dB; into the Chorus, slot 70, through its filter at Off's values, %+.2f dB\n", gain(rom, 0, 0x24, 1000, 0.25, nullptr, 0x72), gain(rom, 0, 0x24, 1000, 0.25, nullptr, 0x70));
    const double flat = gain(rom, 1, 0x28, 800, 0.06);
    std::printf("Parametric EQ at 800 Hz, against flat: Mid 1 +15 %+.2f, +7 %+.2f, -7 %+.2f, -15 %+.2f; Total +15 %+.2f dB\n", gain(rom, 1, 0x28, 800, 0.06, "4=30") - flat, gain(rom, 1, 0x28, 800, 0.06, "4=22") - flat,
                gain(rom, 1, 0x28, 800, 0.06, "4=8") - flat, gain(rom, 1, 0x28, 800, 0.06, "4=0") - flat, gain(rom, 1, 0x28, 800, 0.06, "10=30") - flat);
    std::printf("Stereo Delay: L %s; R %s\n", echoes(rom, 11, 60000, 0).c_str(), echoes(rom, 11, 60000, 1).c_str());
    std::printf("Multi Tap: L %s; R %s\n", echoes(rom, 12, 50000, 0).c_str(), echoes(rom, 12, 50000, 1).c_str());
    int silent = 0, away = 0;
    for (int t = 0; t < 42; t++) { const double v = gain(rom, t, 0x28, 1000, 0.03); if (v < -40) silent++; if (v > 30) away++; }
    std::printf("MFX types at 1 kHz: of 42, %d silent, %d running away\n", silent, away);
    for (int rv = 1; rv <= 10; rv++) {                                          // the signal in a Reverb's tail, DC aside: a click into its send, 1 s and 3 s on
        Rig g(rom, false); g.boot(0, rv, 0, nullptr);
        double e1 = 0, e3 = 0, last = 0;
        for (int i = 0; i < 44100 * 3 + 4410; i++) {
            for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, s == 0x24 && i == 100 ? 2097152 : 0);
            g.chip.runFrame();
            const double v = (double)g.chip.gram(0xa1), d = v - last; last = v;
            if (i >= 44100 && i < 44100 + 4410) e1 += d * d;
            if (i >= 44100 * 3) e3 += d * d;
        }
        std::printf("Reverb %-11s tail, DC aside: at 1 s %.1f dB, at 3 s %.1f dB\n", kRevName[rv], 10 * std::log10(e1 / 4410 + 1e-3) - 138.5, 10 * std::log10(e3 / 4410 + 1e-3) - 138.5);
    }
}
void speed(const char* rom) {
    for (const auto& c : { std::pair<int, int>(0, 0), std::pair<int, int>(1, 4), std::pair<int, int>(27, 4) }) {
        Rig g(rom, false); g.boot(c.first, c.second, 1, nullptr);
        Rng r(1); const std::clock_t t0 = std::clock(); const int n = 200000;                  // CPU time: the machine may be busy with something else
        for (int i = 0; i < n; i++) { for (unsigned s = 0x20; s <= 0x2e; s += 2) g.chip.setGram(s, (int32_t)(r.next() >> 12) - 524288); g.chip.runFrame(); }
        const double sec = (double)(std::clock() - t0) / CLOCKS_PER_SEC;
        std::printf("MFX %d, Reverb %d: %.2f x real time, %.0f%% of a core\n", c.first, c.second, (n / 44100.0) / sec, 100 * sec / (n / 44100.0));
    }
    // The control side.  The driver on its thread, and the most the audio thread ever gets between two samples: a Reverb's type, the longest program with the most read-backs.
    using Clock = std::chrono::steady_clock;
    const auto us = [](Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::micro>(b - a).count(); };
    const Clock::time_point t0 = Clock::now(); XtFxControl c(rom); const Clock::time_point t1 = Clock::now(); XtFxChip chip;
    while (!c.drained()) { c.service(chip); chip.runFrame(); }
    std::printf("the driver's start: %.0f us\n", us(t0, t1));
    const auto edit = [&](const char* what, int id, int v) {
        c.set(id, v); const Clock::time_point a = Clock::now(); c.update(); const Clock::time_point b = Clock::now(); double most = 0; long samples = 0;
        while (!c.drained()) { const Clock::time_point p = Clock::now(); c.service(chip); most = std::max(most, us(p, Clock::now())); chip.runFrame(); samples++; }
        std::printf("%s: %.0f us on the control thread; at most %.1f us between two samples; %.1f ms to reach the chip\n", what, us(a, b), most, samples / 44.1);
    };
    edit("an MFX level", 0x178, 100); edit("the MFX's type", 0x177, 27); edit("one of its parameters", 0x57d, 3); edit("the Reverb's type", 0x230, 4); edit("the Chorus's type", 0x1fc, 3);
}
} // namespace xtfxcheck
#endif
