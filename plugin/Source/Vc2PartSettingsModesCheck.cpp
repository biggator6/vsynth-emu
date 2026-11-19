// xt_vc2settings' sixth part (v291): the mode row 0x89396e20, its wrapper 0x89396f70 and Auto Note's switch
// 0x89396f90 against rom.4's, on the seed five after the run's, every table and the part's nodes (0x899888d0) noise;
// the message's two routines (0x89393c00 makes one, 0x89393ba0 hands it on) hooked to log its number, source, channel
// and last word among the reads, Auto Note's two setters by v290's hooks, the port's All Sound Off the same through
// SettingsPortOuts; Modulator Key Mode and Vocoder Sw drawn narrow, and the row's two bytes they're held to set to the
// store's answers half the time; where Auto Note's switch reads through a read object never set up (flag 0 held), the
// stack word it leaves given a handle of its own.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
constexpr uint32_t kModeRow = 0x89396e20, kModeWrapper = 0x89396f70, kAutoNoteSwitch = 0x89396f90;   // [FW]
// [FW] Modulator Key Mode and Vocoder Sw, and their places in a part's 32 bytes
constexpr int16_t kKeyModeId = 0x1a2, kVocoderSwId = 0x193;
constexpr uint32_t kModeBytes = 32, kKeyModeAt = 0x18, kVocoderAt = 0x16;
// [FW] Auto Note's switch's second read object's word, in its frame (r14 and pr pushed, then 24 bytes)
constexpr uint32_t kLeftoverWordAt = kVc2Sp - 20;
// The IDs drawn narrow: Modulator Key Mode 0…2, Vocoder Sw 0…1
constexpr SettingsNarrow kModeNarrow[] = { { kKeyModeId, 3, kHalfTheTime }, { kVocoderSwId, 2, kHalfTheTime } };

// [FW] the message's routines: 0x89393c00 makes one by its number, 0x89393ba0 hands it on to a source's channel
SettingsCall made(Sh4& c) { return { kOutMessage, 0, 0, 0, (int32_t)c.r[5] }; }
SettingsCall handedOn(Sh4& c) { return { kOutSoundOff, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], 0 }; }
constexpr SettingsCallee kMessageCallees[] = { { 0x89393c00, made }, { 0x89393ba0, handedOn } };

// The part's start: the IDs narrow, the message's routines hooked (Auto Note's two setters hooked by the fifth's)
void hookMessages(SettingsSides& s) {
    s.rom.store.narrow = kModeNarrow;
    settingsHookCallees(s, kMessageCallees);
}

// Half the time a row's byte as the store will answer its ID, so that it stays
void heldTo(SettingsSides& s, std::mt19937& rng, int part, int16_t id, uint32_t at) {
    if (rng() % 2 == 0) return;
    const auto v = (uint8_t)s.rom.store.value({ (int32_t)Vc2ParamBlock::kPart, part, 0, 0, id });
    s.rows.modes[kModeBytes * part + at] = v;
    s.fw.cpu.wr8(kModesAt + kModeBytes * part + at, v);
}

// The All Sound Offs and the reads through a leftover object in the port's log
std::string counted(SettingsSides& s, const std::string& what) {
    for (const SettingsCall& c : s.mine.log) {
        s.n.soundOffs += c.block == kOutSoundOff;
        s.n.leftovers += c.block == kLeftoverCall;
    }
    return what;
}

// The mode row or its wrapper on a drawn part
template <uint32_t Routine, size_t Counted>
std::string modeEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts);
    heldTo(s, rng, part, kKeyModeId, kKeyModeAt);
    heldTo(s, rng, part, kVocoderSwId, kVocoderAt);
    ++s.n.byEvent[Counted];
    return counted(s, settingsOnBoth(s, { Routine, "the mode row", vc2R0 }, { kModesAt, (uint32_t)part },
                                     [&] { return (uint32_t)s.settings.modes(s.rows, part, s.nodes, s.outs); }));
}

// Auto Note's switch: rom.4's given the mode rows' address, which it doesn't read, and no answer to compare
std::string autoNoteEvent(SettingsSides& s, std::mt19937&) {
    s.fw.cpu.wr32(kLeftoverWordAt, kLeftoverHandle);
    ++s.n.byEvent[29];
    return counted(s, settingsOnBoth(s, { kAutoNoteSwitch, "Auto Note's switch", nullptr }, { kModesAt }, [&] {
        s.settings.autoNoteSwitch(s.outs);
        return 0u;
    }));
}

const SettingsEvent kModeEvents[] = { modeEvent<kModeWrapper, 27>, modeEvent<kModeRow, 28>, autoNoteEvent };

// A case: the nodes noise, then the tables' and the events
std::string modeCase(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    settingsNoise(s.nodes.nodes.data(), sizeof s.nodes.nodes, rng);
    return settingsCaseOf<kModeEvents, kV293Rows, 5>(s, rng, events, calls);
}

void sayModePart(const SettingsCounts& n) {
    const auto& e = n.byEvent;
    std::printf("modes: %ld events: %ld wrappers, %ld rows, %ld Auto Note switches; %ld All Sound Offs, %ld reads "
                "through a leftover object (%ld calls)\n", n.modeEvents, e[27], e[28], e[29], n.soundOffs,
                n.leftovers, n.modeCalls);
}
} // namespace

const SettingsPartRow kModePartRow { 5, &SettingsCounts::modeEvents, &SettingsCounts::modeCalls, modeCase, sayModePart,
                                     hookMessages };
#endif
