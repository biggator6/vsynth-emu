// The plugin's own MIDI ahead of the firmware's (PluginMidiIn.h, v462)
#include "PluginMidiIn.h"
#include "PluginParamIds.h"
#include <algorithm>
#include <utility>

PluginMidiIn::PluginMidiIn(juce::AudioProcessorValueTreeState& state) : edits(state) {
    stored_.fill(-1);
    // Each the patch's value as the parameter holds it: MONO 0 and POLY 1 as Mono/Poly's choices, the switch 0 and 1
    const std::pair<int, const juce::String*> kStored[] = {
        { xtedit::CoarseTune, &Params::kPatchCoarse }, { xtedit::FineTune, &Params::kPatchFine },
        { xtedit::MonoPoly, &Params::kMonoPoly },      { xtedit::PortaSwitch, &Params::kPorta },
        { xtedit::PortaTime, &Params::kPortaTime },    { xtedit::BendUp, &Params::kBendUp },
        { xtedit::BendDown, &Params::kBendDown },
    };
    for (const auto& [id, param] : kStored) stored_[(size_t)(id - xtedit::CoarseTune)] = edits.indexOf(*param);
    rest_.ensureSize(kRestBytes);
}

const juce::MidiBuffer& PluginMidiIn::ahead(const juce::MidiBuffer& host, bool patchEdits) {
    const auto isCc = [](const juce::MidiMessageMetadata& m) { return m.numBytes == 3 && (m.data[0] & 0xf0) == 0xb0; };
    const bool learning = std::any_of(host.begin(), host.end(), [&](const juce::MidiMessageMetadata& m) {
        return isCc(m) && learn.takes(m.data[1]);
    });
    if (learning) rest_.clear();
    for (const auto meta : host) {   // in the block's order: one being learnt decides the next ones'
        const bool cc = isCc(meta), taken = cc && learning && learn.take(meta.data[1], meta.data[2]);
        if (cc && !taken && patchEdits) controller(meta.data[1], meta.data[2]);
        if (learning && !taken)
            rest_.addEvent(meta.data, meta.numBytes, meta.samplePosition);   // rt-ok: within the room made at the start
    }
    return learning ? rest_ : host;
}

// The editor's message for the next block (any thread but the audio thread's): three bytes at most, the keys' notes and
// the pad's controllers.  False when the queue is full or the message is longer.
bool PluginMidiIn::send(const juce::MidiMessage& m) {
    if (m.getRawDataSize() > (int)sizeof(EditorEvent::bytes)) return false;
    EditorEvent e {};
    std::copy_n(m.getRawData(), m.getRawDataSize(), e.bytes);
    const auto scope = fifo_.write(1);
    if (scope.blockSize1 + scope.blockSize2 == 0) return false;
    scope.forEach([&](int i) { events_[(size_t)i] = e; });
    return true;
}

void PluginMidiIn::controller(int cc, int value) {
    const XtStoredEdits e = port_.controlChange(cc, value);
    for (int i = 0; i < e.count; ++i)
        edits.edit(stored_[(size_t)(e.at[i].id - xtedit::CoarseTune)], (float)e.at[i].value);
}
