// The VC-2 part's channel messages: see Vc2PartChannel.h (v307).  Each routine's reads, calls and writes in rom.4's
// order; a list's next node read after the call.
#include "Vc2PartChannel.h"
#include "Vc2KeysTables.h"

namespace {
constexpr int32_t kAutoNote = 4, kAutoNoteTables = 3;   // [FW] Auto Note's source, the source whose tables it plays on
constexpr int32_t kReleasing = 1;                       // [FW] the On Switch at which another key releases its note
constexpr uint32_t kReleaseVelocity = 64;               // [FW]
constexpr uint32_t kStateWord = 2;                      // [FW] a channel's state: its row's word + 2
constexpr uint8_t kNoteSeen = 0xff;                     // [FW] the table's +0x130 after a note-on
} // namespace

// [FW] v355: a node's channel of -1 (none, the notes' task's own messages on it) reads the word before its row's, which
// for row 0 is the word before the table
uint32_t vc2ChannelStateOf(int32_t source, int32_t channel) {
    const Vc2KeysTables& k = vc2KeysTables();
    const int32_t row = k.sources[static_cast<size_t>(source - 1)];
    const int32_t at = Vc2Keys::kChannels * row + channel;
    return (at < 0 ? k.stateBefore : k.states[static_cast<size_t>(at)]) + kStateWord;
}

uint32_t vc2ChannelTableOf(int32_t source, int32_t channel) {
    return Vc2Keys::tableAt(static_cast<uint32_t>(source - 1), static_cast<uint32_t>(channel));
}

void vc2EachNode(const Vc2PartNodes& nodes, int32_t source, int32_t channel,
                 const std::function<void(const Vc2PartNode&)>& each) {
    const auto list = static_cast<size_t>(Vc2PartNodes::kChannels * (source - 1) + channel);
    for (uint32_t at = nodes.lists[list]; at != 0; at = nodes.node(at).next) each(nodes.node(at));
}

void vc2ChannelNoteOn(Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t key, uint32_t velocity) {
    Vc2PitchSource& p = w.pitch;
    int32_t from = source;
    if (source == kAutoNote) {
        if (p.others > 0) return;
        p.note = (int32_t)key;
        from = kAutoNoteTables;
    } else {
        if (p.note >= 0 && w.autoNoteSwitch == kReleasing)
            vc2ChannelNoteOff(w, kAutoNote, channel, (uint32_t)p.note, kReleaseVelocity);
        p.others = (int32_t)((uint32_t)p.others + 1);
    }
    const uint32_t table = vc2ChannelTableOf(from, channel);
    Vc2KeyTable& k = w.keys.table(table);
    k.noteOn(key, velocity);
    vc2EachNode(w.nodes, from, channel,
                [&](const Vc2PartNode& n) { w.parts.objectNoteOn(n.object, table, n.word8, key, velocity); });
    k.wasReset = kNoteSeen;
}

void vc2ChannelNoteOff(Vc2ChannelWorld& w, int32_t source, int32_t channel, uint32_t key, uint32_t velocity) {
    const bool autoNote = source == kAutoNote;
    const int32_t from = autoNote ? kAutoNoteTables : source;
    const uint32_t table = vc2ChannelTableOf(from, channel);
    Vc2KeyTable& k = w.keys.table(table);
    k.noteOff(key, velocity);
    vc2EachNode(w.nodes, from, channel, [&](const Vc2PartNode& n) { w.parts.objectNoteOff(n.object, table, n.word8); });
    w.parts.releaseTable(table);
    if (k.ons[key] == k.offs[key]) {
        k.offs[key] = 0;
        k.ons[key] = 0;
    }
    Vc2PitchSource& p = w.pitch;
    if (autoNote) {
        p.note = -1;
        return;
    }
    const int32_t left = p.others - 1;
    p.others = left > 0 ? left : 0;
}

void vc2ChannelHeard(Vc2ChannelWorld& w, int32_t source, int32_t channel) {
    w.parts.notify(vc2ChannelStateOf(source, channel));
}
