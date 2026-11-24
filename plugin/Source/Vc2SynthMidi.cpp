// The part's MIDI parts: see Vc2SynthMidi.h (v346).  Every address is rom.4's.
#include "Vc2SynthMidi.h"

namespace {
constexpr uint32_t kHeldClass = 0x898cd3b8;   // [FW] a part object's held class (the plain one's 0x898cd184)
constexpr uint32_t kNextListenerAt = 0xc;     // [FW] a listener node's next

// A channel message's way on the part, by its handler
using Way = void (*)(Vc2Synth&, Vc2ChannelWorld&, int32_t source, int32_t channel, uint32_t data1, uint32_t data2);

void partNoteOff(Vc2Synth&, Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t key, uint32_t velocity) {
    vc2ChannelNoteOff(w, source, channel, key, velocity);
}

void partNoteOn(Vc2Synth&, Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t key, uint32_t velocity) {
    vc2ChannelNoteOn(w, source, channel, key, velocity);
}

// [FW] the polyphonic aftertouch, channel pressure and bend, one code: the channel's state to its listeners
void partHeard(Vc2Synth&, Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t, uint32_t) {
    vc2ChannelHeard(w, source, channel);
}

void partController(Vc2Synth& s, Vc2ChannelWorld&, int32_t source, int32_t channel, uint32_t number, uint32_t value) {
    s.controls->controller(source, channel, (int32_t)number, (int32_t)value);
}

void partProgram(Vc2Synth&, Vc2ChannelWorld&, int32_t, int32_t, uint32_t, uint32_t) {}   // [FW] 0x89393090: rts

// [FW] by Vc2MidiHandler: the note-off, note-on, poly pressure, controller, program change, pressure and bend
constexpr Way kWays[] = { partNoteOff, partNoteOn, partHeard, partController, partProgram, partHeard, partHeard };
} // namespace

void Vc2SynthMidi::message(const Vc2MidiMessage& m, uint32_t word) {
    s_.midi.handle(m, word, *this, *s_.settings);
    Vc2MidiWorld w { s_.rows, s_.noteGate, *s_.settings, *this };
    vc2MidiDispatch(w, m, word);
}

void Vc2SynthMidi::channelMessage(Vc2MidiHandler h, int32_t source, int32_t channel, uint32_t data1,
                                  uint32_t data2) {
    Vc2ChannelWorld w = channelWorld();
    kWays[(size_t)h](s_, w, source, channel, data1, data2);
}

Vc2ChannelWorld Vc2SynthMidi::channelWorld() { return { s_.keys, s_.nodes, s_.pitch, s_.autoNote.w.onSwitch, *this }; }

bool Vc2SynthMidi::held(uint32_t object) const {
    const uint32_t k = (object - Vc2PartNodes::kObjectsAt) / (uint32_t)sizeof(Vc2PartObject);
    return k < s_.nodes.objects.size() && s_.nodes.objects[k].vtable == kHeldClass;
}

void Vc2SynthMidi::objectNoteOn(uint32_t object, uint32_t table, uint32_t word, uint32_t key, uint32_t velocity) {
    if (!held(object)) return;
    s_.assigner.nodeNoteOn({ object, table, word, (int32_t)key, velocity }, s_.keys, s_.voices, *s_.voiceParts);
}

void Vc2SynthMidi::objectNoteOff(uint32_t object, uint32_t table, uint32_t word) {
    if (!held(object)) return;
    s_.assigner.nodeNoteOff({ object, table, word, 0, 0 }, s_.keys, s_.voices, *s_.voiceParts);
}

uint32_t Vc2SynthMidi::listenerOf(uint32_t node) { return s_.reads->wordAt(node); }

uint32_t Vc2SynthMidi::nextListener(uint32_t node) { return s_.reads->wordAt(node + kNextListenerAt); }
