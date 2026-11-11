// xt_msm's cases and main (XT_MSM_MAIN): `xt_msm <rom> [cases] [seed]`.  The disk image's patches with the Multi Step
// Modulator on, each with its own settings at its Patch Tempo, and random settings over every ID's range; both played with
// notes of parts 0 and 1, the tempo timer's ticks a ms (in bursts in some), the tone task's clocks on time or late, and
// in the random ones the arpeggio's flag, edits and loads as they play.  XtMsmCheck.cpp compares the two sides.
// XT_MSM_FIRST=1 says what differs; XT_MSM_STOP=1 ends the run at the first case that differs (mutate_msm.py).
#ifdef XT_MSM_MAIN
#include "XtFiles.h"
#include "XtFirmware.h"
#include "XtMsmCheck.h"
#include "XtSwitches.h"
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int kTempoId = 0x11d;                 // Patch Tempo, in tenths of a BPM
constexpr int kTicksPerQuarter = 480;           // the OS's tempo timer [FW 0x894710e0]
constexpr long kTenthsPerMinute = 600000;       // ms a minute, in tenths of a BPM
constexpr int kPresetMs = 6000, kRandomMs = 2500;
constexpr int kCoarseDests[] = { 0, 3, 17 };    // the destinations whose steps go through the curve

// The tempo timer's ticks each ms at a tempo, the remainder kept
struct Ticks {
    long tempo10 = 1200, acc = 0;
    int next() {
        acc += tempo10 * kTicksPerQuarter;
        const long t = acc / kTenthsPerMinute;
        acc %= kTenthsPerMinute;
        return (int)t;
    }
};

int pick(std::mt19937& r, int lo, int hi) {
    return lo + (int)(r() % (unsigned)(hi - lo + 1));
}

// A value of an ID over its descriptor's range, as the getter gives it: less the centre
int draw(std::mt19937& r, int id) {
    const XtParamDesc* d = xtParamDesc(id);
    return d ? pick(r, d->min - d->centre, d->max - d->centre) : 0;
}

XtMsmSettings randomSettings(std::mt19937& r) {
    XtMsmSettings s;
    for (int id = kXtMsmFirstId; id <= kXtMsmLastId; ++id) *xtMsmField(s, id) = draw(r, id);
    s.master = r() % 8 != 0;
    for (XtMsmTrackSettings& t : s.track) {
        if (r() % 3 == 0) t.zone = 0;
        if (r() % 3 == 0) t.dest = kCoarseDests[r() % 3];
        if (r() % 4 == 0) t.endPoint = pick(r, 0, 2);
    }
    return s;
}

// A player's hands and the tone task's timing on one case
struct Player {
    XtMsmCheck& c;
    std::mt19937& r;
    XtMsmSettings s;
    double notes = 0.003, offs = 0.004;         // a ms
    double live = 0;                            // edits, loads and the arpeggio's flag, a ms
    int lateMost = 2;                           // ms the tone task may run behind a clock
    bool ragged = false;                        // the ticks in bursts, and a catching up
    int held = 0, tail = 0, late = 0;

    bool chance(double p) { return std::uniform_real_distribution<double>(0, 1)(r) < p; }
    void run(XtMsmEvent::Kind kind, bool flag = false, int part = 0, int32_t ticks = 0);
    void play(int ms, long tempo10);
    void liveEvent();
};

void Player::run(XtMsmEvent::Kind kind, bool flag, int part, int32_t ticks) {
    XtMsmEvent e;
    e.kind = kind;
    e.flag = flag;
    e.part = part;
    e.ticks = ticks;
    e.settings = s;
    c.run(e);
}

void Player::liveEvent() {
    const int what = pick(r, 0, 9);
    if (what < 6) {
        const int id = pick(r, kXtMsmFirstId, kXtMsmLastId);
        *xtMsmField(s, id) = draw(r, id);
        run(XtMsmEvent::kEdit, false, r() % 8 == 0);
    } else if (what < 8) {
        run(XtMsmEvent::kArpeggio, r() % 2 == 0);
    } else {
        if (r() % 2 == 0) s = randomSettings(r);
        run(XtMsmEvent::kLoad, false, r() % 8 == 0);
        c.sweep();
    }
}

void Player::play(int ms, long tempo10) {
    Ticks ticks{ tempo10 };
    for (int t = 0; t < ms; ++t) {
        run(XtMsmEvent::kTick, false, 0, ragged && r() % 50 == 0 ? pick(r, 0, 120) : ticks.next());
        if (ragged && r() % 20 == 0) run(XtMsmEvent::kTick);
        if (chance(notes)) {
            const int part = r() % 10 == 0 ? 1 : 0;
            run(XtMsmEvent::kNoteOn, held > 0, part);
            if (part == 0) ++held;
        }
        if (held > 0 && chance(offs) && --held == 0) tail = pick(r, 20, 1500);
        if (tail > 0) --tail;
        if (c.port.pending > 0 || r() % 500 == 0) {
            if (late > 0) {
                --late;
            } else {
                run(XtMsmEvent::kClocks, held > 0 || tail > 0);
                c.termsAlike(r);
                late = pick(r, 0, lateMost);
            }
        }
        if (chance(live)) liveEvent();
    }
    c.sweep();
}

void report(const char* what, const XtMsmCheck& c, long cases) {
    std::printf("%s: %ld cases, %ld events, %ld states, %ld terms: %ld different\n", what, cases, c.events, c.states,
                c.terms, c.different);
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || !xtFwOpen(argv[1]) || !xtMsmLoadTables(argv[1])) {
        std::fprintf(stderr, "usage: xt_msm <rom.2> [random cases] [seed]\n");
        return 2;
    }
    const long cases = argc > 2 ? std::strtol(argv[2], nullptr, 10) : 300;
    const unsigned seed = argc > 3 ? (unsigned)std::strtoul(argv[3], nullptr, 10) : 2024;
    const std::string rom = argv[1];
    const std::string image = rom.substr(0, rom.find_last_of('/') + 1) + "V-SynthXTDiskImage.bin.0.bin";
    XtMsmFw fw(rom);
    XtDiskImage disk;
    if (!fw.ok() || !disk.open(image)) {
        std::fprintf(stderr, "xt_msm: no firmware machine, or no disk image at %s\n", image.c_str());
        return 2;
    }
    std::mt19937 r(seed);
    XtMsmCheck presets(fw), random(fw);
    presets.first = random.first = xtSwitchIs("XT_MSM_FIRST", "1");
    const bool stop = xtSwitchIs("XT_MSM_STOP", "1");
    long nPresets = 0;
    for (const auto& entry : disk.list("Internal.prj/Patch")) {
        if (stop && presets.different) break;
        const auto file = disk.read(entry);
        XtPatchFile pf;
        if (!xtPatchRead(file.data(), file.size(), pf) || xtPatchValue(pf.store, kXtMsmFirstId) == 0) continue;
        Player h{ presets, r, xtMsmSettingsOf(pf.store) };
        presets.where = pf.name;
        presets.begin();
        fw.readStore(&pf.store);
        h.run(XtMsmEvent::kLoad);
        presets.sweep();
        h.play(kPresetMs, xtPatchValue(pf.store, kTempoId));
        ++nPresets;
    }
    report("presets", presets, nPresets);
    for (long i = 0; i < cases && !(stop && (presets.different || random.different)); ++i) {
        Player h{ random, r, randomSettings(r), 0.004, 0.005, 0.002, pick(r, 0, 8), r() % 3 == 0 };
        random.where = "case " + std::to_string(i);
        random.begin();
        h.run(XtMsmEvent::kLoad, false, r() % 16 == 0);
        h.play(kRandomMs, pick(r, 200, 3000));
    }
    report("random", random, cases);
    const long different = presets.different + random.different;
    std::printf("xt_msm: %ld different\n", different);
    return different == 0 ? 0 : 1;
}
#endif
