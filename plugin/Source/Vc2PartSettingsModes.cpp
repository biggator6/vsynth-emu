// The VC-2 part's settings by ID, the mode row 0x89396e20 (its wrapper 0x89396f70) and Auto Note's switch 0x89396f90
// (v291): see Vc2PartSettings.h.  Their flag tests, reads, stores and calls out in rom.4's order.
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsWords.h"
#include <algorithm>

using namespace Vc2PartWords;

namespace {
using Block = Vc2ParamBlock;
// [FW] the part's IDs: Octave Shift, Mono/Poly, Legato Switch, Patch Tempo, Zone 1 Range Upper (the next 14 after
// it), Split Point, Modulator Key Mode, Vocoder Sw, Auto Note On Switch; a kind's first OSC's Type
constexpr int16_t kOctave = 0x12f, kMonoPoly = 0x130, kLegato = 0x131;
constexpr int16_t kTempo = 0x13a, kZoneUpper = 0x141, kSplit = 0x1a1, kKeyMode = 0x1a2, kVocoderSw = 0x193;
constexpr int16_t kAutoNote = 0x19c, kOscType = 0x2e5;
// [FW] the row: 32 bytes a part, its places
constexpr uint32_t kModeBytes = 32, kKeyAt = 0, kMonoAt = 1, kLegatoAt = 2, kTempoAt = 4, kZonesAt = 6, kSplitAt = 0x17;
constexpr uint32_t kVocoderAt = 0x16, kKeyModeAt = 0x18;
// [FW] the zones read (the 16th's upper key the top), the sources told; a semitone's octave, OSC Type EXTIN; Auto
// Note's switch reads part 0 alone, its kinds 0 and 1
constexpr int kZones = 15, kSources = 3, kPartZero = 0, kFirstKind = 0, kSecondKind = 1;
constexpr int32_t kTopKey = 127, kOctaveKeys = 12, kExtin = 2;
} // namespace

void Vc2PartSettings::zoneUppers(uint8_t* at, int part) {
    int32_t most = 0;
    for (int z = 0; z < kZones; ++z) {
        most = std::max(most, p_.value(Block::kPart, part, 0, static_cast<int16_t>(kZoneUpper + z)));
        put8(at + z, most);
    }
    put8(at + kZones, kTopKey);
}

bool Vc2PartSettings::modes(Vc2PartRows& t, int part, const Vc2PartNodes& nodes, Vc2SettingsParts& out) {
    if (p_.held(kHeldCommon)) return false;
    const auto get = [&](int16_t id) { return p_.value(Block::kPart, part, 0, id); };
    uint8_t* row = t.modes.data() + kModeBytes * part;
    put8(row + kKeyAt, times(get(kOctave), kOctaveKeys));
    put8(row + kMonoAt, get(kMonoPoly) == 0 ? 1 : 0);
    put8(row + kLegatoAt, get(kLegato));
    put16(row + kTempoAt, get(kTempo));
    zoneUppers(row + kZonesAt, part);
    put8(row + kSplitAt, get(kSplit));
    const int32_t mode = get(kKeyMode);
    const int32_t vocoder = get(kVocoderSw);
    // The Mode moved, or Vocoder Sw gone off: each source's node of the part with a channel told (rom.4 tests the
    // node's address for 0 as well, which none is)
    const bool moved = static_cast<int8_t>(row[kKeyModeAt]) != mode;
    if (moved || (vocoder == 0 && row[kVocoderAt] != 0))
        for (int source = 1; source <= kSources; ++source) {
            const int8_t channel = nodes.of(source, part).channel;
            if (channel >= 0) out.allSoundOff(source, channel);
        }
    put8(row + kKeyModeAt, mode);
    put8(row + kVocoderAt, vocoder);
    return !p_.held(kHeldCommon);
}

void Vc2PartSettings::autoNoteSwitch(Vc2SettingsParts& out) {
    if (p_.held(kHeldKind)) return;
    const int32_t first = p_.value(Block::kKindA, kPartZero, kFirstKind, kOscType);
    if (p_.held(kHeldKind)) return;
    const int32_t second = p_.value(Block::kKindA, kPartZero, kSecondKind, kOscType);
    int32_t on = 0;
    if (first == kExtin || second == kExtin) {
        // flag 0 held: no set-up, and the reads go through the stack's leftover word
        const Block block = p_.held(kHeldCommon) ? Block::kLeftover : Block::kPart;
        on = p_.value(block, kPartZero, 0, kAutoNote);
        p_.value(block, kPartZero, 0, kLegato);   // read, its answer tested and dropped
    }
    out.setAutoNote(on);
    out.setAutoNoteLegato();
}
