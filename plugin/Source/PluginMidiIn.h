#pragma once
// The plugin's own MIDI ahead of the firmware's (v462): the editor's queue (moved as it stood from PluginProcessor.h,
// v201's), and a control change's edits of the patch as rom.2's OS part makes them (XtMidiPort; HARDWARE_FIRMWARE.md,
// "rom.2's MIDI paths"), each onto its parameter through PluginMidiEdits, so the control and the host follow.
#include "PluginMidiEdits.h"
#include "PluginMidiLearn.h"
#include "XtMidiEdits.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <cstdint>

class PluginMidiIn {
public:
    explicit PluginMidiIn(juce::AudioProcessorValueTreeState& state);
    PluginMidiEdits edits;
    PluginMidiLearn learn { edits };   // v463: MIDI learn's map

    // Audio thread, at a block's start: the host's block with the controllers the learn map takes (one being learnt, one
    // learnt) taken out, each learnt or onto its parameter, the plugin's alone (v463, d-3), and with `patchEdits` (the
    // V-Synth mode) every other controller's edits of the patch made (`controller`), whatever then plays the block, as
    // the XT's OS makes them before and apart from its tone generator.  The block itself when the map takes none.
    const juce::MidiBuffer& ahead(const juce::MidiBuffer& host, bool patchEdits);

    // The editor's notes and controllers (v201): the footer's keys and the Time Trip pad's, for the synth as if from the
    // host's MIDI, queued here (any thread but the audio thread), taken at the next block's start before the host's.
    // False if the queue is full or the message is longer than three bytes.
    bool send(const juce::MidiMessage& m);
    // Each message the editor has queued, in order and taken: the audio thread's, at a block's start, and a test's
    template <class Take>
    void take(Take&& take) {
        const auto scope = fifo_.read(fifo_.getNumReady());
        scope.forEach([&](int i) {
            const EditorEvent& e = events_[(size_t)i];
            take(juce::MidiMessage(e.bytes[0], e.bytes[1], e.bytes[2]));
        });
    }

private:
    // A control change's edits of the patch (the V-Synth's parameters), before its tone generator has it
    void controller(int cc, int value);
    struct EditorEvent { uint8_t bytes[3]; };
    static constexpr int kEditorEvents = 256;
    juce::AbstractFifo fifo_ { kEditorEvents };
    std::array<EditorEvent, kEditorEvents> events_ {};
    XtMidiPort port_;
    static constexpr int kRestBytes = 1 << 16;
    juce::MidiBuffer rest_;   // learned's block without the map's controllers, its room made at the start
    // The parameter each ID the port edits is: Patch Coarse and Fine Tune, Mono/Poly, Portamento Switch and Time, the
    // Bend Ranges; by ID less 0x110, -1 for one it doesn't edit
    std::array<int, xtedit::BendDown - xtedit::CoarseTune + 1> stored_ {};
};
