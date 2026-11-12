// xt_arp's cases and main (XT_ARP_MAIN): `xt_arp <rom> [cases] [seed]`.  The disk image's patches with the arpeggio on,
// each with its own settings, pattern and Patch Tempo, and random ones over every setting, both played with keys, chords,
// the pedal and live edits, then let go; XtArpeggioCheck.cpp compares the two sides.  XT_ARP_FIRST=1 says what differs.
#ifdef XT_ARP_MAIN
#include "XtArpeggioCheck.h"
#include "XtArpeggioPatch.h"
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtSwitches.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int kTempo = 0x11d;
constexpr int kTicksPerQuarter = 480;          // the tempo timer's [FW 0x894710e0]
constexpr int kPresetMs = 4000, kRandomMs = 3000, kTailMs = 1500;
constexpr long kTickCases = 100;               // XtTempoTicksCheck.cpp's, 3 s each, half of them EXTERNAL

// The range a case draws each setting from (their IDs: XtArpeggioPatch.h), kept at `kept` that often
struct SettingId {
    int XtArpSettings::*field;
    int lo, hi;
    double keep;
    int kept;
};
constexpr SettingId kSettingIds[] = {
    { &XtArpSettings::hold, 0, 1, 0.75, 0 },
    { &XtArpSettings::motif, 0, 7, 0, 0 },
    { &XtArpSettings::duration, 0, 100, 0.3, 100 },
    { &XtArpSettings::shuffleRate, 0, 100, 0.4, 50 },
    { &XtArpSettings::shuffleRes, 0, 1, 0, 0 },
    { &XtArpSettings::keyVelocity, 1, 127, 0.5, 0 },
    { &XtArpSettings::octave, -3, 3, 0, 0 },
    { &XtArpSettings::grid, 0, 5, 0, 0 },
    { &XtArpSettings::endPoint, 0, 31, 0.3, 31 },
};
const char* const kMotifNames[8] = { "UP", "DOWN", "UP&DOWN", "RANDOM", "NOTE-ORDER", "RHYTHM", "PHRASE", "AUTO" };

// The tempo timer's ticks each ms, 480 to the quarter of whole ms (the clock object's +0x14 = 60000 / BPM)
struct TempoTicks {
    int period = 500, acc = 0;
    explicit TempoTicks(int tempo10) : period(60000 / std::max(1, tempo10 / 10)) {}
    int next() {
        acc += kTicksPerQuarter;
        const int t = acc / period;
        acc %= period;
        return t;
    }
};

// A player's hands on one case
struct Player {
    XtArpCheck& c;
    std::mt19937& r;
    XtArpSettings s;
    XtArpPattern p;
    bool live = false, ragged = false;          // live edits; ticks in no steady rate
    std::vector<int> down;
    double letGo = 0.003;                       // a key let go each ms
    bool chordFirst = false;                    // a chord down from the start
    int pick(int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(r); }
    bool chance(double x) { return std::uniform_real_distribution<double>(0, 1)(r) < x; }
    int pickOf(int n) { return pick(0, n - 1); }

    void draw(const SettingId& f) { s.*f.field = chance(f.keep) ? f.kept : pick(f.lo, f.hi); }
    // A key: mostly the keyboard's middle, sometimes anywhere, sometimes the ends (where the walks wrap)
    int key() {
        if (chance(0.05)) return chance(0.5) ? pick(0, 2) : pick(125, 127);
        return chance(0.1) ? pick(0, 127) : pick(36, 96);
    }
    void press() {
        const int k = key();
        c.noteOn(k, pick(1, 127));
        if (std::ranges::find(down, k) == down.end()) down.push_back(k);
    }
    void release() {
        const size_t i = (size_t)pickOf((int)down.size());
        c.noteOff(down[i]);
        down.erase(down.begin() + (long)i);
    }
    void switchOffOn() {
        s.on = false;
        c.settings(s);
        s.on = true;
        c.settings(s);
    }
    // A line's State and Note/CC: a controller often one that resets, most often the pedals (CC 64, 66), at most 95
    uint8_t lineByte() { return chance(0.2) ? ctrlByte() : (uint8_t)pick(0, 127); }
    uint8_t ctrlByte() {
        static constexpr int kCcs[] = { 0, 1, 7, 10, 11, 32, 64, 66, 74, 91, 93 };
        if (chance(0.3)) return (uint8_t)(0x80 | (chance(0.5) ? 64 : 66));
        return (uint8_t)(0x80 | (chance(0.6) ? kCcs[pickOf((int)std::size(kCcs))] : pick(0, kXtArpCtrlMost)));
    }
    // The Hold pedal, often either side of 64
    int pedalValue() { return chance(0.3) ? 64 : chance(0.3) ? 63 : pick(0, 127); }
    // A cell, a line's State and Note/CC, or the first CTRL line's controller, edited while it plays: a line that sent a
    // pedal and then another controller leaves the pedal for resetControllers' last two sends
    void editPattern() {
        auto* ctrl = std::ranges::find_if(p.line, [](uint8_t b) { return (b & 0x80) != 0; });
        if (chance(0.4))
            p.cell[pickOf(XtArpPattern::kSteps)][pickOf(XtArpPattern::kLines)] = (uint8_t)pick(0, 128);
        else if (chance(0.5) && ctrl != std::end(p.line))
            *ctrl = ctrlByte();
        else
            p.line[pickOf(XtArpPattern::kLines)] = lineByte();
        c.pattern(p);
    }
    void keys() {
        if (down.size() < 6 && chance(0.004))
            for (int n = pick(1, 4); n > 0; --n) press();
        if (!down.empty() && chance(letGo)) release();
        if (!down.empty() && chance(0.0005)) c.noteOn(down[(size_t)pickOf((int)down.size())], pick(1, 127));
        if (chance(0.0003)) c.noteOn(key(), 0);
    }
    void ms(TempoTicks& tt, bool playing) {
        if (playing) keys();
        if (chance(0.0008)) c.pedal(pedalValue());
        if (live && chance(0.0004)) {
            draw(kSettingIds[pickOf((int)std::size(kSettingIds))]);
            c.settings(s);
        }
        if (live && chance(0.001)) editPattern();
        if (live && chance(0.00015)) switchOffOn();
        c.tick(!ragged ? tt.next() : chance(0.001) ? pick(50, 3000) : pick(0, 4));
    }
    void play(int ms, int tempo10) {
        TempoTicks tt(tempo10);
        for (int n = chordFirst ? pick(1, 4) : 0; n > 0; --n) press();
        for (int t = 0; t < ms; ++t) this->ms(tt, true);
        while (!down.empty()) release();
        if (chance(0.5)) c.pedal(0);
        for (int t = 0; t < kTailMs; ++t) this->ms(tt, false);
    }
};

void randomCase(Player& h) {
    h.s.on = true;
    for (const SettingId& f : kSettingIds) h.draw(f);
    for (int l = 0; l < XtArpPattern::kLines; ++l) {
        h.p.line[l] = h.lineByte();
        // empty, sparse or dense, and a fifth of the lines legato, mostly ties: so more notes sound than keys are held
        const double dense = h.chance(0.3) ? 0 : h.chance(0.5) ? 0.2 : 0.7;
        const double ties = h.chance(0.2) ? 0.7 : 0.2;
        for (int s = 0; s < XtArpPattern::kSteps; ++s)
            h.p.cell[s][l] = (uint8_t)(!h.chance(dense) ? 0 : h.chance(ties) ? 1 : h.pick(2, 128));
    }
    h.live = h.chance(0.5);
    h.ragged = h.chance(0.15);
}

void report(const char* what, const XtArpCheck& c, long cases) {
    std::printf("%s: %ld cases, %ld operations, %ld messages, %ld states, %ld different\n", what, cases, c.ops,
                c.events, c.states, c.different);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || !xtFwOpen(argv[1])) {
        std::fprintf(stderr, "usage: xt_arp <rom.2> [random cases] [seed]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 1000;
    const unsigned seed = argc > 3 ? (unsigned)std::strtoul(argv[3], nullptr, 10) : 2024;
    XtArpFw fw(argv[1]);
    XtDiskImage disk;
    const std::string rom = argv[1];
    const std::string image = rom.substr(0, rom.find_last_of('/') + 1) + "V-SynthXTDiskImage.bin.0.bin";
    if (!fw.ok() || !disk.open(image)) {
        std::fprintf(stderr, "xt_arp: no firmware machine, or no disk image at %s\n", image.c_str());
        return 2;
    }
    std::mt19937 r(seed);
    XtArpCheck presets(fw), random(fw);
    presets.first = random.first = xtSwitchIs("XT_ARP_FIRST", "1");
    const bool stop = xtSwitchIs("XT_ARP_STOP", "1");   // the run ends at its first case that differs (mutate_arp.py)
    long nPresets = 0;
    for (const auto& entry : disk.list("Internal.prj/Patch")) {
        const auto file = disk.read(entry);
        XtPatchFile pf;
        if (stop && presets.different) break;
        if (!xtPatchRead(file.data(), file.size(), pf) || xtPatchValue(pf.store, kXtArpSwitchId) == 0) continue;
        Player h{ presets, r, xtArpSettingsOf(pf.store), xtArpPatternOf(pf.store), false, false, {}, 0.0006, true };
        presets.where = pf.name;
        presets.begin((int)(nPresets++ % 16), h.s, h.p);
        h.play(kPresetMs, xtPatchValue(pf.store, kTempo));
    }
    report("presets", presets, nPresets);
    for (long i = 0; i < cases && !(stop && (presets.different || random.different)); ++i) {
        Player h{ random, r, {}, {}, false, false, {}, 0.003, false };
        randomCase(h);
        random.where = "case " + std::to_string(i);
        random.begin(h.pick(0, 15), h.s, h.p);
        h.play(kRandomMs, h.pick(200, 2500));
    }
    report("random", random, cases);
    std::printf("messages alike by motif:");
    for (int m = 0; m < 8; ++m) std::printf(" %s %ld", kMotifNames[m], presets.byMotif[m] + random.byMotif[m]);
    std::printf("\n");
    const XtTicksTally ticks = xtTempoTicksCheck(rom, kTickCases, seed, presets.first);
    std::printf("ticks: %ld cases, %ld ms, %ld values, %ld different%s\n", ticks.cases, ticks.ms, ticks.values,
                ticks.different, ticks.failed ? ", a routine failed" : "");
    return presets.different + random.different + ticks.different == 0 && !ticks.failed ? 0 : 1;
}
#endif
