// The plugin's MIDI ahead of the firmware's (v462), through the processor: rom.2's edits of the patch from a control
// change (XtMidiPort, which xt_midi holds to the firmware's own) onto the parameters, as research/NEXT_ISSUES.md item 2
// and the user's d-5 have them:
// - CC 5, 65, 126 and 127 and RPN 0, 1 and 2's Data Entry through processBlock: the synth's controls take each at once,
//   the parameter once the message thread hands it on, the host told by a gesture;
// - a controller the OS part doesn't edit by (CC 1, 74) changes no parameter;
// - an edit whose parameter something else moved before it was handed on is dropped, and a patch's controls or a
//   session coming back drop one (the image's first patch, through applyPatch); a saved session carries one not yet
//   handed on.
// Run: VSynthEmuMidiTest <rom.2.bin>.  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestControls.h"
#include "TestMain.h"
#include "TestPatches.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginProcessor.h"
#include <initializer_list>

namespace {
constexpr double kRate = 48000.0;
constexpr int kBlock = 480;

juce::MidiMessage cc(int number, int value) { return juce::MidiMessage::controllerEvent(1, number, value); }

// One block with the messages at its first frame
void play(VSynthEmuProcessor& p, std::initializer_list<juce::MidiMessage> messages) {
    juce::AudioBuffer<float> buffer(2, kBlock);
    buffer.clear();
    juce::MidiBuffer midi;
    for (const auto& m : messages) midi.addEvent(m, 0);
    p.processBlock(buffer, midi);
}

float stored(VSynthEmuProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue(id)->load(); }

// What a parameter tells its listeners, a host's among them
struct Told : juce::AudioProcessorParameter::Listener {
    int values = 0, gestures = 0;
    void parameterValueChanged(int, float) override { ++values; }
    void parameterGestureChanged(int, bool) override { ++gestures; }
};

// A processor on the image, its OSC 1 ANALOG so the XT's path plays with no wave
struct Playing {
    VSynthEmuProcessor p;
    explicit Playing(const juce::File& rom) {
        p.setRomFile(rom);
        setParam(p, Params::kOscType, 0);
        p.prepareToPlay(kRate, kBlock);
    }
};

// CC 5's edit: the synth's at once, the parameter's when handed on, a gesture to the host
void portamentoTime(TestChecks& check, const juce::File& rom) {
    Playing s(rom);
    VSynthEmuProcessor& p = s.p;
    play(p, { cc(5, 100) });
    check(p.currentControls().portaTime == 100, "CC 5 should be the synth's Portamento Time at once");
    check(stored(p, Params::kPortaTime) == 20, "and its parameter's only once the message thread hands it on");
    Told told;
    auto* param = p.apvts.getParameter(Params::kPortaTime);
    param->addListener(&told);
    check(p.midiIn().edits.flush() == 1, "the message thread should hand CC 5's edit on");
    check(stored(p, Params::kPortaTime) == 100, "Portamento Time's parameter should be CC 5's value then");
    check(told.values == 1 && told.gestures == 2, "and its listeners told once, within a gesture");
    check(p.midiIn().edits.flush() == 0, "an edit handed on should be handed on once");
    param->removeListener(&told);
    play(p, { cc(1, 90), cc(74, 10), cc(7, 40) });
    check(p.midiIn().edits.flush() == 0, "CC 1, 74 and 7 should edit no parameter");
}

// Each of the six, as the OS part writes the patch
void theSix(TestChecks& check, const juce::File& rom) {
    Playing s(rom);
    VSynthEmuProcessor& p = s.p;
    const auto after = [&](std::initializer_list<juce::MidiMessage> m, const juce::String& id, float want,
                           const char* what) {
        play(p, m);
        p.midiIn().edits.flush();
        check(stored(p, id) == want, what);
    };
    after({ cc(65, 64) }, Params::kPorta, 1, "CC 65 at 64 should turn Portamento on");
    after({ cc(65, 63) }, Params::kPorta, 0, "and at 63 off");
    after({ cc(126, 0) }, Params::kMonoPoly, 0, "CC 126 should make it MONO");
    after({ cc(127, 0) }, Params::kMonoPoly, 1, "CC 127 POLY");
    after({ cc(101, 0), cc(100, 0), cc(6, 12) }, Params::kBendUp, 12, "RPN 0 should set Bend Range Up");
    check(stored(p, Params::kBendDown) == 12, "and Bend Range Down with it");
    after({ cc(6, 25) }, Params::kBendUp, 12, "past 24 RPN 0 should leave the Bend Ranges");
    after({ cc(100, 1), cc(6, 96), cc(38, 0) }, Params::kPatchFine, 50, "RPN 1 at 96 should be Fine Tune +50");
    after({ cc(100, 2), cc(6, 40) }, Params::kPatchCoarse, -24, "RPN 2 at 40 should be Coarse Tune -24");
    after({ cc(99, 0), cc(98, 2), cc(6, 70) }, Params::kPatchCoarse, -24, "an NRPN chosen last should edit nothing");
}

// Who wins: something else's later setting, a patch, a session
void laterWins(TestChecks& check, const juce::File& rom) {
    Playing s(rom);
    VSynthEmuProcessor& p = s.p;
    play(p, { cc(5, 90) });
    setParam(p, Params::kPortaTime, 33);   // a host or a control, before the edit is handed on
    p.midiIn().edits.flush();
    check(stored(p, Params::kPortaTime) == 33, "a parameter moved after the edit should keep its own value");
    check(p.currentControls().portaTime == 33, "and the synth take it");
    play(p, { cc(5, 91) });
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    Playing back(rom);
    back.p.setStateInformation(saved.getData(), (int)saved.getSize());
    check(stored(back.p, Params::kPortaTime) == 91, "a saved session should carry an edit not yet handed on");
    play(back.p, { cc(5, 10) });
    back.p.setStateInformation(saved.getData(), (int)saved.getSize());
    check(back.p.currentControls().portaTime == 91, "a session coming back should drop a pending edit");
    back.p.midiIn().edits.flush();
    check(stored(back.p, Params::kPortaTime) == 91, "and the message thread not hand it on after");
}

// A patch's controls drop a pending edit: the image's first patch, onto zone 1
void patchDrops(TestChecks& check, const juce::File& rom) {
    const auto patches = imagePatches(rom);
    check(!patches.empty(), "the image's patches should be beside the ROM");
    if (patches.empty()) return;
    Playing s(rom);
    VSynthEmuProcessor& p = s.p;
    play(p, { cc(5, 77) });
    p.applyPatch(patches[0].name, patches[0].store, 0, juce::File());
    const float patchTime = stored(p, Params::kPortaTime);
    check(patchTime != 77, "the patch's own Portamento Time should differ from the edit's");
    check(p.currentControls().portaTime == (int)patchTime, "a patch's controls should drop a pending edit");
    p.midiIn().edits.flush();
    check(stored(p, Params::kPortaTime) == patchTime, "and the message thread not hand it on after");
}

int midiTest(const juce::File& rom, const juce::File&) {
    TestChecks check { "midi" };
    portamentoTime(check, rom);
    theSix(check, rom);
    laterWins(check, rom);
    patchDrops(check, rom);
    return check.summary();
}
}  // namespace

int main(int argc, char** argv) { return testMain(argc, argv, "midi", midiTest); }
