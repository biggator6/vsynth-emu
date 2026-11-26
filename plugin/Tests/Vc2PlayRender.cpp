// VSynthEmuVc2PlayTest's render mode: `VSynthEmuVc2PlayTest <rom> render <place> <out.wav> [rate] [fx] [in=<file>]
// [state=<file>]`.  With `state`, a session's saved state (getStateInformation's bytes, rppstate.py takes one from a
// REAPER project) restored in place of the patch.  Else the disk image's .p02 at `place` (0 the first) in a processor at
// `rate` (44.1 kHz if none is said) in the VC-2 mode,
// with `fx` the Effects switch on, the control thread ticked before every block as the plugin's timer ticks it; on the
// input the rig's saw (vc2PlayInput: 220 Hz at 44.1 kHz, its frames the host's) or `file`'s frames as they are, looped,
// a mono file on both sides; C4, E4 and G4 held two seconds and let go a third.  It writes the output to `out.wav` and
// prints what the output is made of: its likeness to the input (the best normalised correlation over the lags a part's
// latency takes), its level at the saw's pitch and at each held note's, the part mixer's MAIN and MULTI levels, and the
// part's view (the mic's level, the voices sounding, the vocoder's bands).  A measure, not in verify.sh: the user's
// report that a vocoder patch gave the dry input (2026-11-26).
#include "TestWave.h"
#include "Vc2PlayRig.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginVc2Params.h"
#include "../Source/UiControls.h"
#include "../Source/Vc2PartPlayer.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numbers>

namespace {
constexpr double kRigRate = 44100.0;             // the rate the rig's saw is 220 Hz at
constexpr double kSeconds = 3.0, kReleaseSeconds = 1.0;   // the run's length unless `seconds=` says, its last let go
constexpr int kChord[] = { 60, 64, 67 };         // C4, E4, G4
constexpr juce::uint8 kVelocity = 100;
constexpr double kInputPitch = 220.0;            // the rig's saw at kRigRate
constexpr int kMostLag = 4096;                   // frames: the part's latency and more
constexpr double kFromSeconds = 0.5;             // the window measured: from here to the notes' release
constexpr int kDirect = 0x195, kSwitch = 0x193, kType = 0x194;         // [FW] Direct Sound Level, Vocoder Sw, Type

// The run's options and the frames it takes at its rate
struct Run {
    double rate = kRigRate;
    bool fx = false;
    juce::File input;   // none: the saw
    juce::File state;   // a session's saved state (getStateInformation's bytes) in place of the image's patch
    double seconds = kSeconds;
    int heldBlocks = 0, blocks = 0, from = 0, to = 0;
};

// An option's file, `key=<path>`, from the working folder; none if `o` isn't that key's
juce::File optionFile(const juce::String& o, const juce::String& key) {
    return o.startsWith(key) ? juce::File::getCurrentWorkingDirectory().getChildFile(o.substring(key.length()))
                             : juce::File();
}

// The options after `render <place> <out.wav>`: a number the rate, `fx`, `in=` the input's file, `state=` a session's
Run renderOptions(const juce::StringArray& rest) {
    Run run;
    constexpr int kOptions = 3;   // render, place, out.wav
    for (int i = kOptions; i < rest.size(); ++i) {
        const juce::String& o = rest[i];
        run.fx = run.fx || o == "fx";
        if (const juce::File f = optionFile(o, "in="); f != juce::File()) run.input = f;
        if (const juce::File f = optionFile(o, "state="); f != juce::File()) run.state = f;
        if (o.startsWith("seconds=")) run.seconds = std::max(kSeconds, o.fromFirstOccurrenceOf("=", false, false)
                                                                           .getDoubleValue());
        if (o.containsOnly("0123456789.")) run.rate = o.getDoubleValue();
    }
    const double held = run.seconds - kReleaseSeconds;
    run.heldBlocks = (int)(held * run.rate) / kVc2PlayBlock;
    run.blocks = (int)(run.seconds * run.rate) / kVc2PlayBlock;
    run.from = (int)(kFromSeconds * run.rate);
    run.to = (int)(held * run.rate);
    return run;
}

// The input for the whole run: the saw, or the file's frames looped; false if the file can't be read
bool inputOf(const Run& run, juce::AudioBuffer<float>& in) {
    const int frames = in.getNumSamples();
    if (run.input == juce::File()) {
        for (int side = 0; side < 2; ++side)
            for (int i = 0; i < frames; ++i) in.setSample(side, i, vc2PlayInput(i, side));
        return true;
    }
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(run.input));
    if (reader == nullptr || reader->lengthInSamples <= 0) return false;
    const int length = (int)std::min<juce::int64>(reader->lengthInSamples, frames);
    juce::AudioBuffer<float> file(2, length);
    reader->read(&file, 0, length, 0, true, true);   // a mono file read on both sides
    for (int at = 0; at < frames; at += length)
        for (int side = 0; side < 2; ++side) in.copyFrom(side, at, file, side, 0, std::min(length, frames - at));
    return true;
}

double noteHz(int note) { return 440.0 * std::pow(2.0, (note - 69) / 12.0); }

// The level at `hz` in the run's window at its rate: a Goertzel's magnitude, × 2 / n, a sine's amplitude
double levelAt(const float* x, const Run& run, double hz) {
    const double w = 2.0 * std::numbers::pi * hz / run.rate, c = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (int i = run.from; i < run.to; ++i) {
        const double s0 = x[i] + c * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return 2.0 * std::sqrt(std::max(0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (run.to - run.from);
}

double db(double v) { return 20.0 * std::log10(std::max(v, 1e-12)); }

// The input's best likeness in the output's left side: the most of the normalised correlation over lags 0…kMostLag,
// over the window's first kLikeSeconds
double likeness(const float* in, const float* out, const Run& run, int& bestLag) {
    constexpr double kLikeSeconds = 2.0;
    const int to = std::min(run.to, run.from + (int)(kLikeSeconds * run.rate));
    double best = 0;
    for (int lag = 0; lag <= kMostLag; ++lag) {
        double xy = 0, xx = 0, yy = 0;
        for (int i = run.from; i < to; ++i) {
            const double x = in[i - lag], y = out[i];
            xy += x * y;
            xx += x * x;
            yy += y * y;
        }
        const double r = xx > 0 && yy > 0 ? xy / std::sqrt(xx * yy) : 0.0;
        if (r > best) {
            best = r;
            bestLag = lag;
        }
    }
    return best;
}

// What the part held at the hold's end: the mixer's levels and the view's
void printPart(VSynthEmuProcessor& p) {
    const Vc2MixerBuffers& m = p.vc2Play().player()->part().mixer.b;
    std::printf("mixer: MAIN %.4g (target %.4g, Direct Sound %.4g), MULTI %.4g (target %.4g, MFX Send %.4g), Reverb "
                "%.4g\n", m.main, m.mainTarget, m.directSound, m.multi, m.multiTarget, m.mfxSend, m.reverb);
    const Vc2PartView& v = p.vc2Play().view();
    constexpr auto relaxed = std::memory_order_relaxed;
    double mic = 0, out = 0;
    const int bands = v.bands.load(relaxed);
    for (int b = 0; b < bands; ++b) {
        mic = std::max(mic, (double)v.micBands[(size_t)b].load(relaxed));
        out = std::max(out, (double)v.outBands[(size_t)b].load(relaxed));
    }
    std::printf("view: mic %.1f dBFS, %d voices sounding, output %.1f dBFS, %s; %d vocoder bands, their mic envelopes "
                "at most %.4g, their outputs at most %.4g\n", db(v.micPeak.load(relaxed)), v.sounding.load(relaxed),
                db(v.outputPeak.load(relaxed)), v.voiced.load(relaxed) ? "voiced" : "unvoiced", bands, mic, out);
}

// The chord's messages: note-ons, or note-offs
juce::MidiBuffer chord(bool on) {
    juce::MidiBuffer midi;
    for (const int note : kChord)
        midi.addEvent(on ? juce::MidiMessage::noteOn(1, note, kVelocity) : juce::MidiMessage::noteOff(1, note), 0);
    return midi;
}

// Host blocks [from, to) with `in` on the input, `first` at the first block's start; the output kept whole in `out`
void play(VSynthEmuProcessor& p, int from, int to, const juce::MidiBuffer& first, const juce::AudioBuffer<float>& in,
          juce::AudioBuffer<float>& out) {
    juce::AudioBuffer<float> buf(2, kVc2PlayBlock);
    for (int b = from; b < to; ++b) {
        juce::MidiBuffer midi = b == from ? first : juce::MidiBuffer();
        p.vc2Play().tick();   // the control thread's, as the plugin's timer ticks it between blocks
        for (int side = 0; side < 2; ++side) buf.copyFrom(side, 0, in, side, b * kVc2PlayBlock, kVc2PlayBlock);
        p.processBlock(buf, midi);
        for (int side = 0; side < 2; ++side) out.copyFrom(side, b * kVc2PlayBlock, buf, side, 0, kVc2PlayBlock);
    }
}

// The output's make-up over the hold's window: its level and the input's, its likeness to the input, its level at
// the saw's pitch (the saw's run) and each note's
void printOutput(const juce::AudioBuffer<float>& in, const juce::AudioBuffer<float>& out, const Run& run) {
    int lag = 0;
    const double like = likeness(in.getReadPointer(0), out.getReadPointer(0), run, lag);
    const float* o = out.getReadPointer(0);
    const double rms = out.getRMSLevel(0, run.from, run.to - run.from);
    const double inRms = in.getRMSLevel(0, run.from, run.to - run.from);
    std::printf("output (left, %.1f-%.1f s at %.0f Hz): rms %.1f dBFS (the input's %.1f); likeness to the input %.3f "
                "at a lag of %d frames\n ", kFromSeconds, run.to / run.rate, run.rate, db(rms), db(inRms), like, lag);
    if (run.input == juce::File()) {
        const double pitch = kInputPitch * run.rate / kRigRate;
        std::printf(" at the saw's %.1f Hz: %.1f dBFS;", pitch, db(levelAt(o, run, pitch)));
    }
    for (const int note : kChord)
        std::printf(" at %.1f Hz: %.1f dBFS;", noteHz(note), db(levelAt(o, run, noteHz(note))));
    std::printf("\n");
}

// The run's session state restored, its part made; the state's patch name in `name`
std::unique_ptr<VSynthEmuProcessor> fromState(TestChecks& check, const Run& run, std::string& name) {
    juce::MemoryBlock bytes;
    check(run.state.loadFileAsData(bytes), "the state should read: " + run.state.getFullPathName().toStdString());
    auto p = vc2ProcessorFromState(bytes, run.rate);
    check(p->vc2Play().player() != nullptr, "the state's VC-2 mode should make its part");
    name = p->vc2PatchName().toStdString() + " (the state's)";
    return p;
}

void printPatch(VSynthEmuProcessor& p, const std::string& name, const Run& run) {
    const auto value = [&p](int id) { return Controls::paramValue(p.apvts, vc2HostParamId(id, 0, 0)); };
    std::printf("%s: Vocoder Sw %d, Vocoder Type %d, Direct Sound Level %d; Effects %s; input %s\n", name.c_str(),
                value(kSwitch), value(kType), value(kDirect), run.fx ? "on" : "off",
                run.input == juce::File() ? "the saw" : run.input.getFileName().toRawUTF8());
}
} // namespace

int vc2PlayRender(const juce::File& rom, const juce::File& work, const juce::StringArray& rest) {
    TestChecks check { "vc2play render" };
    vc2PlayImages(check, rom);
    const Run run = renderOptions(rest);
    std::string name;
    auto p = run.state != juce::File() ? fromState(check, run, name)
                                       : vc2PlayPatchAt(check, rom, work, rest[1].getIntValue(), run.rate, name);
    check(rest[2].isNotEmpty(), "render <place> <out.wav> [options]: an output file should be named");
    juce::AudioBuffer<float> in(2, run.blocks * kVc2PlayBlock), out(2, run.blocks * kVc2PlayBlock);
    check(inputOf(run, in), "the input's file should read: " + run.input.getFullPathName().toStdString());
    if (p == nullptr || rest[2].isEmpty()) return check.summary();
    if (run.state == juce::File()) {   // a state keeps its own Effects switch
        p->apvts.getParameter(Params::kFxOn)->setValueNotifyingHost(run.fx ? 1.0f : 0.0f);
        p->vc2Play().tick();
    }
    printPatch(*p, name, run);
    play(*p, 0, run.heldBlocks, chord(true), in, out);
    printPart(*p);
    play(*p, run.heldBlocks, run.blocks, chord(false), in, out);
    printOutput(in, out, run);
    const juce::File wav(rest[2]);
    const bool written = writeWav(wav, out, (int)run.rate, 24);
    check(written, "the output should be written to " + wav.getFullPathName().toStdString());
    return check.summary();
}
