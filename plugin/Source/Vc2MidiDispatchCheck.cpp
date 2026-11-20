// xt_vc2settings' twenty-first part (v308): the part's MIDI dispatcher, Vc2MidiDispatch, against rom.4's own 0x8938fed0
// (with its class 0x8938fe50, the 0x40's builder 0x89390230 and the reset 0x89390340), on the seed twenty after the
// run's.  Hooked to log their arguments and routine: the handlers it calls (the note-off and note-on, the polyphonic
// aftertouch, the controller, the program change, the channel pressure, the bend), v297's parameters and
// partParameters (their word), the DC cut's setter 0x893955e0 and all notes off; the system's read of Out DC cut for
// TG the read objects' hooks as the first part's.  Each case the tables noise; then 400 events, each a message: a
// status of any class and channel, a parameters' message (0x40, 0x41), the reset (0x70) or any byte, the data bytes
// below 128 or any (a note-on's velocity 0 one time in four), a source 1…4 or any, the word's kind 0…4 or any; before
// each the note-on gate (0x89981904) 0, 1 or any and the held note's word 0, 1 or any.  After each the tables (the
// held note's word among them), the calls and the answer.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
constexpr uint32_t kMidiDispatchAt = 0x8938fed0, kGateAt = 0x89981904, kPartAt = 0x899888ac;   // [FW]
constexpr uint32_t kMidiMessageAt = 0x8C046000, kWordAt = 0x8C046010;                         // in Vc2Fw's RAM
// [FW] the handlers by Vc2MidiHandler, and whether each is given the second data byte
constexpr const auto& kHandlers = kVc2MidiHandlers;
constexpr bool kTwoBytes[] = { true, true, true, true, false, false, true };
constexpr uint32_t kParametersAt = 0x89392890, kPartParametersAt = 0x89392830, kDcCutAt = 0x893955e0;   // [FW]

SettingsCall threeCalled(Sh4& c) { return { kOutMidi, (int32_t)c.r[5], (int32_t)c.r[6], 0, (int32_t)c.r[4] }; }
SettingsCall wordCalled(Sh4& c) { return { kOutMidi, (int32_t)c.rd32(c.r[5]), 0, 0, (int32_t)c.r[4] }; }
SettingsCall dcCutCalled(Sh4& c) { return { kOutMidi, (int32_t)c.r[4], 0, 0, 0 }; }

constexpr auto kMidiCallees = [] {
    constexpr auto routine = settingsCalledRoutine;
    std::array<SettingsCallee, std::size(kHandlers) + 4> a {};
    for (size_t k = 0; k < std::size(kHandlers); ++k)
        a[k] = { kHandlers[k], kTwoBytes[k] ? settingsCalledFour : threeCalled, nullptr, routine };
    a[std::size(kHandlers)] = { kParametersAt, wordCalled, nullptr, routine };
    a[std::size(kHandlers) + 1] = { kPartParametersAt, wordCalled, nullptr, routine };
    a[std::size(kHandlers) + 2] = { kDcCutAt, dcCutCalled, nullptr, routine };
    a[std::size(kHandlers) + 3] = kSettingsNotesOffCallee;
    return a;
}();

void hookMidi(SettingsSides& s) { settingsHookCallees(s, kMidiCallees); }

// The port's parts: each call logged as its hook logs it
class MidiPortParts final : public Vc2MidiParts {
public:
    explicit MidiPortParts(SettingsSides& s) : s_(s) {}
    void channelMessage(Vc2MidiHandler h, int32_t source, int32_t channel, uint32_t d1, uint32_t d2) override {
        const auto k = static_cast<size_t>(h);
        called(kHandlers[k], { channel, (int32_t)d1, kTwoBytes[k] ? (int32_t)d2 : 0, source });
    }
    void parameters(uint32_t word) override { called(kParametersAt, { (int32_t)word, 0, 0, (int32_t)kPartAt }); }
    void partParameters(uint32_t word) override {
        called(kPartParametersAt, { (int32_t)word, 0, 0, (int32_t)kPartAt });
    }
    void dcCut(int32_t value) override { called(kDcCutAt, { value, 0, 0, 0 }); }
    void allNotesOff() override { s_.mine.log.push_back({ kOutNotesOff, 0, 0, 0, 0 }); }

private:
    void called(uint32_t routine, const std::array<int32_t, 4>& a) { settingsLogRoutine(s_.mine.log, routine, a); }
    SettingsSides& s_;
};

// A message: a status of a class drawn, its data, its source
Vc2MidiMessage drawMidiMessage(std::mt19937& rng) {
    const uint8_t statuses[5] = { (uint8_t)(0x80 + rng() % 0x70), 0x40, 0x41, 0x70, (uint8_t)rng() };
    Vc2MidiMessage m;
    m.status = statuses[rng() % 2 ? 0 : 1 + rng() % 4];
    m.data1 = (uint8_t)(rng() % 4 ? rng() % 128 : rng());
    m.data2 = (uint8_t)(rng() % 4 ? rng() % 128 : rng());
    if (vc2MidiClass(m.status) == 0x90 && rng() % 4 == 0) m.data2 = 0;
    m.source = (uint8_t)(rng() % 4 ? 1 + rng() % 4 : rng());
    return m;
}

// [FW] the count each class goes to: the channel messages, the parameters', the reset, the rest
size_t countOf(uint8_t cls) {
    if (cls >= 0x80) return 87;
    const std::pair<uint8_t, size_t> rows[] = { { 0x40, 88 }, { 0x41, 88 }, { 0x70, 89 } };
    for (const auto& [c, at] : rows)
        if (c == cls) return at;
    return 90;
}

std::string midiEvent(SettingsSides& s, std::mt19937& rng) {
    const Vc2MidiMessage m = drawMidiMessage(rng);
    const uint32_t kinds[6] = { 0, 1, 2, 3, 4, (uint32_t)rng() };
    const uint32_t word = (kinds[rng() % 6] & 0xff) | (uint32_t)rng() << 8;
    const uint32_t gates[3] = { 0, 1, (uint32_t)rng() };
    s.noteGate = gates[rng() % 3];
    const int32_t helds[3] = { 0, 1, (int32_t)rng() };
    s.rows.heldNote = helds[rng() % 3];
    s.fw.cpu.write(kMidiMessageAt, &m, sizeof m);
    s.fw.cpu.wr32(kWordAt, word);
    s.fw.cpu.wr32(kGateAt, s.noteGate);
    s.fw.cpu.wr32(kHeldNoteAt, (uint32_t)s.rows.heldNote);
    ++s.n.byEvent[countOf(vc2MidiClass(m.status))];
    return settingsOnBoth(s, { kMidiDispatchAt, "the dispatcher", nullptr }, { 0, kMidiMessageAt, kWordAt }, [&] {
        MidiPortParts parts { s };
        Vc2MidiWorld w { s.rows, s.noteGate, s.params, parts };
        vc2MidiDispatch(w, m, word);
        return 0u;
    });
}

const SettingsEvent kMidiEvents[] = { midiEvent };
constexpr SettingsSaid kMidiSaid[] = { { 87, "channel messages" }, { 88, "parameters' messages" }, { 89, "resets" },
                                       { 90, "others" } };
} // namespace

const SettingsPartRow kMidiPartRow { 20, &SettingsCounts::midiEvents, &SettingsCounts::midiCalls,
                                     settingsCaseOf<kMidiEvents, sizeof(Vc2PartRows), 5>, nullptr, hookMidi,
                                     "MIDI dispatcher", kMidiSaid };
#endif
