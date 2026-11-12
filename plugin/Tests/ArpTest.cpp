// XtSynth's arpeggiator (v189), heard: the router, the tempo ticks and what the arpeggio sends, measured in the render.
// A chord held on OSC 1's analog saw with the arpeggio on, UP on a quarter-note grid, Duration 25 and no release: the
// first note 15 ticks after the keys (the first step's delay), then one each quarter counted from the keys, as the step
// puts the next at its start plus the grid (0x89302570), at 90 and 140 BPM, each onset where 60/BPM puts it.  The same render twice is the same.  The Switch off: one onset, the chord held.  The Hold pedal (CC 64) keeps the
// arpeggio stepping after the keys are let go, and its release stops it.
// Run: VSynthEmuArpTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "../Source/XtSynth.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

constexpr double kRate = 44100.0;
constexpr int kFrame = 220;                  // the meter's frame, 5 ms
constexpr double kFrameMs = 1000.0 * kFrame / kRate;
constexpr double kSlackMs = 2.0 * kFrameMs;  // an onset within two frames of where it falls
constexpr int kChord[] = { 60, 64, 67 };
constexpr float kVelocity = 100.0f / 127.0f;
constexpr int kHoldPedal = 64;
constexpr double kFirstTicks = 15.0, kTicksPerQuarter = 480.0;   // [FW 0x89302570] the first step's delay

struct Case {
    int tempo10 = 1200;          // the Patch Tempo, in tenths of a BPM
    bool arpOn = true;
    double seconds = 4.1;        // no step within 50 ms of the end at either tempo
    double letGoAt = -1.0;       // the keys let go here, with the pedal down from the start (−1: held)
    double pedalUpAt = -1.0;
};

XtSynth::Controls controlsOf(const Case& c) {
    XtSynth::Controls ctl;
    ctl.oscType = 0;             // the analog oscillator: it plays with no wave
    ctl.attack = 0;
    ctl.release = 0;
    ctl.patchTempo = c.tempo10;
    ctl.arp.on = c.arpOn;
    ctl.arp.motif = 0;           // UP
    ctl.arp.grid = 0;            // a quarter
    ctl.arp.duration = 25;
    ctl.arp.endPoint = XtArpPattern::kSteps - 1;
    for (int s = 0; s < XtArpPattern::kSteps; ++s) ctl.arpPattern.cell[s][0] = 101;   // velocity 100 on line 1, a NOTE line
    return ctl;
}

// Each 5 ms frame's RMS, the case played from its first frame
std::vector<float> levels(const Case& c) {
    XtSynth s;
    s.prepare(kRate);
    s.setControls(controlsOf(c));
    if (c.letGoAt >= 0.0) s.controller(kHoldPedal, 127);
    for (const int k : kChord) s.noteOn(k, kVelocity);
    const int frames = (int)(c.seconds * kRate / kFrame);
    std::vector<float> l(kFrame), r(kFrame), rms;
    for (int f = 0; f < frames; ++f) {
        const double t = f * kFrame / kRate;
        if (c.letGoAt >= 0.0 && std::fabs(t - c.letGoAt) < 0.5 * kFrame / kRate)
            for (const int k : kChord) s.noteOff(k);
        if (c.pedalUpAt >= 0.0 && std::fabs(t - c.pedalUpAt) < 0.5 * kFrame / kRate) s.controller(kHoldPedal, 0);
        s.render(l.data(), r.data(), kFrame);
        double sum = 0.0;
        for (int i = 0; i < kFrame; ++i) sum += (double)l[i] * l[i] + (double)r[i] * r[i];
        rms.push_back((float)std::sqrt(sum / (2.0 * kFrame)));
    }
    return rms;
}

// Where the level first rises past a third of the render's loudest frame from below a tenth of it, in ms
std::vector<double> onsetsMs(const std::vector<float>& rms) {
    const float top = *std::max_element(rms.begin(), rms.end());
    std::vector<double> at;
    bool quiet = true;
    for (size_t f = 0; f < rms.size(); ++f) {
        if (quiet && rms[f] > top / 3.0f) at.push_back((double)f * kFrameMs);
        if (rms[f] > top / 3.0f) quiet = false;
        if (rms[f] < top / 10.0f) quiet = true;
    }
    return at;
}

std::string said(const std::vector<double>& at) {
    std::string s;
    for (const double t : at) s += " " + std::to_string((int)std::lround(t));
    return s;
}

// A note each quarter at this tempo, from 15 ticks after the keys
void steps(TestChecks& check, int tempo10) {
    const Case c{ tempo10 };
    const std::vector<float> rms = levels(c);
    check(rms == levels(c), "the same render twice should be the same");
    const std::vector<double> at = onsetsMs(rms);
    const double quarterMs = 600000.0 / tempo10, firstMs = quarterMs * kFirstTicks / kTicksPerQuarter;
    const int want = (int)std::floor(c.seconds * 1000.0 / quarterMs) + 1;
    const std::string bpm = std::to_string(tempo10 / 10) + " BPM";
    check((int)at.size() == want, bpm + ": " + std::to_string(want) + " onsets wanted, heard at" + said(at));
    for (size_t i = 0; i < at.size(); ++i) {
        const double due = i == 0 ? firstMs : (double)i * quarterMs;
        check(std::fabs(at[i] - due) <= kSlackMs, bpm + ": onset " + std::to_string(i) + " at " +
              std::to_string(at[i]) + " ms, due at " + std::to_string(due));
    }
}

int arpTest(const juce::File& rom, const juce::File&) {
    TestChecks check{ "arp" };
    if (!xtSynthSetRom(rom.getFullPathName().toStdString())) {
        check(false, "the image should load");
        return check.summary();
    }
    steps(check, 900);
    steps(check, 1400);
    Case off;
    off.arpOn = false;
    check(onsetsMs(levels(off)).size() == 1, "the Switch off: the chord should start once and hold");
    Case held;
    held.letGoAt = 1.0;
    held.pedalUpAt = 2.6;
    const std::vector<double> at = onsetsMs(levels(held));
    const auto between = [&](double lo, double hi) {
        return std::count_if(at.begin(), at.end(), [&](double t) { return t >= lo && t < hi; });
    };
    check(between(1100.0, 2600.0) >= 2, "the pedal down: it should step on after the keys go, heard at" + said(at));
    check(between(2700.0, 4000.0) == 0, "the pedal up: it should stop, heard at" + said(at));
    return check.summary();
}

} // namespace

int main(int argc, char** argv) {
    return testMain(argc, argv, "arp", arpTest);
}
