// The VC-2 part's receive channel (v298): see Vc2PartSettings.h.  0x89393fd0 in rom.4's order: the part's Receive
// Switch and Channel on system B's block, the channel −1 with the switch off; the store's record, IDs 0, 1, 2 and 52
// on the system's fourth block (0x893206f0), read by 0x89320700 and parsed by 0x89318a20 and 0x89318a90 to no effect
// (0x89318a20 leaves its record's first byte 0 on every way, so 0x89318a90 answers 0 and the held class's install
// after it, 0x89393310, is never reached; the bank's count 0x893273c0 it reads is only compared), so only the reads
// are kept; each source's node moved and its two glides aimed; for part 0, source 3's channel's change heard.  Both
// routines answer 1 on every way.  And (v299) the routing's reset 0x89393e30, which calls the receive channel for part
// 0: ChordMemory Switch's change; with its flag every node to no channel, their glides on the spare table and reset
// (each node moved once a glide, as rom.4 does, the second move finding nothing to do), then by channel its Hold-press
// byte and the four sources' key tables reset, the receive channel (whose 1 lets it go on), and part 0's node of source 1
// given the held class (0x89392820, a bare answer of 0, chooses it over the plain).  It answers 1 on every way.
#include "Vc2PartSettings.h"
#include <utility>

namespace {
using Block = Vc2ParamBlock;
constexpr int16_t kSwitchId = 71, kChannelId = 72, kIdsAPart = 2;   // [FW] Part 1 Receive Switch and Channel
constexpr int16_t kRecordIds[] = { 0, 1, 2, 52 };                   // [FW] 0x89320700's, given 0
constexpr int16_t kChordMemoryId = 55;                              // [FW] ChordMemory Switch, the system's fourth
constexpr int kGlidesAPart = 2, kGlidesASource = kGlidesAPart * Vc2Keys::kChannels;
constexpr int32_t kNoChannel = -1, kNoSource = -1;
// [FW] source 4's glides are aimed at its channel 0's table whatever the channel; the others' at their table for it,
// the spare one off 0…15
constexpr int kLastSource = Vc2PartNodes::kSources - 1;
// [FW] part 0's change heard by source 3's node's channel; the controllers then sent from source 3 on the old
// channel: Reset All Controllers, All Notes Off with no value, All Sound Off
constexpr int kPartZero = 0, kHeardSource = 3, kControlSource = 3;
constexpr std::pair<int32_t, int32_t> kOldChannelControls[] = { { 121, 0 }, { 123, -1 }, { 120, 0 } };

uint32_t tableFor(int source, int32_t channel) {
    if (source == kLastSource) return Vc2Keys::tableAt(static_cast<uint32_t>(source), 0);
    if (channel < 0 || channel >= Vc2Keys::kChannels) return Vc2Keys::kSpareAt;
    return Vc2Keys::tableAt(static_cast<uint32_t>(source), static_cast<uint32_t>(channel));
}

// A part's glide `n` of `source` (less 1)
Vc2KeyGlide& glideOf(Vc2Keys& keys, int source, int part, int n) {
    return keys.glides[kGlidesASource * source + kGlidesAPart * part + n];
}

// A part's two glides of `source`, aimed at `table`
void aim(Vc2Keys& keys, int source, int part, uint32_t table) {
    for (int n = 0; n < kGlidesAPart; ++n) glideOf(keys, source, part, n).table = table;
}

// The routing's reset of a part's node of `source`: for each of its glides the node moved to no channel, the glide on
// the spare table and reset
void release(Vc2PartWorld& w, int source, int part) {
    for (int n = 0; n < kGlidesAPart; ++n) {
        w.nodes.move(Vc2PartNodes::addressOf(source + 1, part), kNoSource, kNoChannel);
        Vc2KeyGlide& g = glideOf(w.keys, source, part, n);
        g.table = Vc2Keys::kSpareAt;
        g.reset();
    }
}
} // namespace

bool Vc2PartSettings::receiveChannel(Vc2PartWorld& w, int part) {
    const auto id = [part](int16_t first) { return static_cast<int16_t>(first + kIdsAPart * part); };
    int32_t channel = kNoChannel;
    if (p_.value(Block::kSystemB, 0, 0, id(kSwitchId)) != 0) channel = p_.value(Block::kSystemB, 0, 0, id(kChannelId));
    for (const int16_t record : kRecordIds) p_.value(Block::kSystemD, 0, 0, record);
    const int8_t before = w.nodes.of(kHeardSource, kPartZero).channel;
    const int32_t onto = channel >= Vc2Keys::kChannels ? kNoChannel : channel;
    for (int s = 0; s < Vc2PartNodes::kSources; ++s) {
        w.nodes.move(Vc2PartNodes::addressOf(s + 1, part), s, onto);
        aim(w.keys, s, part, tableFor(s, channel));
    }
    if (part != kPartZero) return true;
    const int8_t after = w.nodes.of(kHeardSource, kPartZero).channel;
    if (after == before) return true;
    if (before != kNoChannel)
        for (const auto& [number, value] : kOldChannelControls) w.out.controller(kControlSource, before, number, value);
    if (after == kNoChannel) return true;
    if (w.t.heldNote == 0) {
        w.out.allNotesOff();
        return true;
    }
    w.out.heldNoteOn();
    return true;
}

bool Vc2PartSettings::receive(Vc2PartWorld& w) { return receiveChannel(w, kPartZero); }

bool Vc2PartSettings::routing(Vc2PartWorld& w, bool on) {
    const int32_t chordMemory = p_.value(Block::kSystemD, 0, 0, kChordMemoryId);
    if (chordMemory != w.t.chordMemory) {
        w.t.chordMemory = chordMemory;
        w.out.allNotesOff();
    }
    if (!on) return true;
    for (int s = 0; s < Vc2PartNodes::kSources; ++s)
        for (int p = 0; p < Vc2PartNodes::kParts; ++p) release(w, s, p);
    for (int c = 0; c < Vc2Keys::kChannels; ++c) {
        w.t.holdPresses[static_cast<size_t>(c)] = 1;
        for (int s = 0; s < Vc2Keys::kSources; ++s)
            w.keys.table(Vc2Keys::tableAt(static_cast<uint32_t>(s), static_cast<uint32_t>(c))).reset();
    }
    receiveChannel(w, kPartZero);
    w.nodes.install(Vc2PartNodes::addressOf(1, kPartZero), Vc2NodeClass::kHeld, w.keys);
    return true;
}
