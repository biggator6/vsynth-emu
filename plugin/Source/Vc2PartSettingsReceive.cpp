// The VC-2 part's receive channel (v298): see Vc2PartSettings.h.  0x89393fd0 in rom.4's order: the part's Receive
// Switch and Channel on system B's block, the channel −1 with the switch off; the store's record, IDs 0, 1, 2 and 52
// on the system's fourth block (0x893206f0), read by 0x89320700 and parsed by 0x89318a20 and 0x89318a90 to no effect
// (0x89318a20 leaves its record's first byte 0 on every way, so 0x89318a90 answers 0 and the held class's install
// after it, 0x89393310, is never reached; the bank's count 0x893273c0 it reads is only compared), so only the reads
// are kept; each source's node moved and its two glides aimed; for part 0, source 3's channel's change heard.  Both
// routines answer 1 on every way.
#include "Vc2PartSettings.h"
#include <utility>

namespace {
using Block = Vc2ParamBlock;
constexpr int16_t kSwitchId = 71, kChannelId = 72, kIdsAPart = 2;   // [FW] Part 1 Receive Switch and Channel
constexpr int16_t kRecordIds[] = { 0, 1, 2, 52 };                   // [FW] 0x89320700's, given 0
constexpr int kGlidesAPart = 2, kGlidesASource = kGlidesAPart * Vc2Keys::kChannels;
constexpr int32_t kNoChannel = -1;
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

// A part's two glides of `source` (less 1), aimed at `table`
void aim(Vc2Keys& keys, int source, int part, uint32_t table) {
    for (int n = 0; n < kGlidesAPart; ++n) keys.glides[kGlidesASource * source + kGlidesAPart * part + n].table = table;
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
