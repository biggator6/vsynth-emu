#pragma once
// xt_vc2settings' shared parts (v286): a getter's call as either side logs it (the read object's block, part and kind,
// and the ID), the store both sides answer from, each side's reads (its store, its copy of the held flags, the read
// after which a flag is set, its log), the port's parameters on them, and the two sides; from v288 the tables'
// addresses, the events by what a routine is given and a case of them, the parts' (the third in
// Vc2PartSettingsOscCheck.cpp).  XT_VC2SETTINGS_MAIN only.
#include "Vc2CheckCalls.h"
#include "Vc2CheckMain.h"
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsTables.h"
#include <vector>

// [FW] the tables, rom.4's
inline constexpr uint32_t kCommonsAt = 0x89997090, kEnvelopesAt = 0x89e2765c, kLfosAt = 0x89e2f85c;
inline constexpr uint32_t kMixesAt = 0x89e32a60, kTvasAt = 0x89e4987c, kControlsAt = 0x89997fcc;
inline constexpr uint32_t kBytesAt = 0x89998a14, kCosmsAt = 0x89e2003c;   // v287's
inline constexpr uint32_t kOscsAt = 0x89e371e0;                            // v288's
inline constexpr int kSettingsParts = 16, kSettingsKinds = 16, kEventsACase = 400;
// The OSC block's reader 0x893a04e0 as the log has it, a block past the read objects' (v288)
inline constexpr int32_t kOscBlockCall = 8;

struct SettingsCall {
    int32_t block, part, kind, sub, id;   // sub: a COSM's read object's COSM input (v287), the OSC (v288), else 0
    bool operator==(const SettingsCall&) const = default;
};

// The parameters' values, one draw per event: by a hash of the seed and the read, mostly a byte's or a depth's, now
// and then at the laws' edges or any word; the OSC block's reader's answer mostly 1, now and then 0 or any word
struct SettingsStore {
    uint32_t seed = 0;
    int32_t value(const SettingsCall& c) const;
};

// A side's reads in an event: the getter answers from the store, logs, and after the `flipAt`-th read sets the flag
// `flipFlag` (rom.4's side in its memory too, by its hook)
struct SettingsReads {
    SettingsStore store {};
    std::array<uint8_t, 5> flags {};
    int flipAt = 0, flipFlag = 0, reads = 0;
    std::vector<SettingsCall> log {};
    int32_t read(const SettingsCall& c);
};

class SettingsPortParams final : public Vc2PartParams {
public:
    explicit SettingsPortParams(SettingsReads& reads) : reads_(reads) {}
    int32_t value(Vc2ParamBlock block, int part, int kind, int16_t id) override {
        return reads_.read({ (int32_t)block, part, kind, 0, id });
    }
    int32_t cosmValue(int part, int kind, int cosm, int16_t id) override {
        return reads_.read({ (int32_t)Vc2ParamBlock::kCosm, part, kind, cosm, id });
    }
    bool oscBlock(int part, int kind, int osc) override {
        return reads_.read({ kOscBlockCall, part, kind, osc, 0 }) != 0;
    }
    bool held(int flag) override { return reads_.flags[flag] != 0; }

private:
    SettingsReads& reads_;
};

struct SettingsCounts {
    long events = 0, calls = 0, gaveUp = 0, flipped = 0;
    long moreEvents = 0, moreCalls = 0;   // the second part's (v287)
    long oscEvents = 0, oscCalls = 0;     // the third's (v288)
    std::array<long, 24> byEvent {};
};

// The two sides: rom.4's interpreter with the read objects' set-ups, the getter and (v288) the OSC block's reader
// hooked, the tables, each side's reads, the port
struct SettingsSides {
    explicit SettingsSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2PartRows rows {};
    SettingsReads rom {}, mine {};
    SettingsPortParams params { mine };
    Vc2PartSettings settings { params };
    SettingsCounts n {};
    // rom.4's side's read: logged and answered from the store, the flag set in its memory at the flip
    uint32_t romRead(Sh4& c, const SettingsCall& call);
};

// rom.4's routine on `args` and the port's `mine()`, both from the same flags, store and flip; what differs in the
// tables, the calls or the answer
std::string settingsOnBoth(SettingsSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                           const std::function<uint32_t()>& mine);
// The draws before an event: the store's seed, now and then a held flag, now and then one of the first `flipped`
// flags set after a read
void settingsDraw(SettingsSides& s, std::mt19937& rng, uint32_t flipped);
// The third part (v288): the OSC table's readers
long settingsOscPart(SettingsSides& s, const Vc2Run& run);

using SettingsEvent = std::string (*)(SettingsSides&, std::mt19937&);

template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string partEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts);
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a part's reader", vc2R0 }, { TableAt, (uint32_t)part },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part); });
}

template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string kindEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts), kind = (int)(rng() % kSettingsKinds);
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a kind's row", vc2R0 }, { TableAt, (uint32_t)part, (uint32_t)kind },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part, kind); });
}

template <auto Member, uint32_t Routine, uint32_t TableAt, int Rows, size_t Counted>
std::string rowEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts), kind = (int)(rng() % kSettingsKinds), i = (int)(rng() % Rows);
    ++s.n.byEvent[Counted];
    const Vc2CheckRoutine r { Routine, "a row", vc2R0 };
    return settingsOnBoth(s, r, { TableAt, (uint32_t)part, (uint32_t)kind, (uint32_t)i },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part, kind, i); });
}

// A system's reader: rom.4's answers 1 (0x893a71d0 reads no table: the word it's given unread)
template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string systemEvent(SettingsSides& s, std::mt19937&) {
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a system's reader", vc2R0 }, { TableAt }, [&] {
        (s.settings.*Member)(s.rows);
        return 1u;
    });
}

// A case: noise in the tables up to `Noised` bytes in (each part's draws as it first drew them), the tables into
// rom.4's memory, then 400 of `Events` with `Flipped` flags to set, counted in `events` and `calls`
template <const auto& Events, size_t Noised, uint32_t Flipped>
std::string settingsCaseOf(SettingsSides& s, std::mt19937& rng, long& events, long& calls) {
    auto* rows = reinterpret_cast<uint8_t*>(&s.rows);
    for (uint8_t* b = rows; b != rows + Noised; ++b) *b = (uint8_t)rng();
    const Vc2PartRows& t = s.rows;
    s.fw.cpu.write(kCommonsAt, &t.commons, sizeof t.commons);
    s.fw.cpu.write(kEnvelopesAt, &t.envelopes, sizeof t.envelopes);
    s.fw.cpu.write(kLfosAt, &t.lfos, sizeof t.lfos);
    s.fw.cpu.write(kMixesAt, &t.mixes, sizeof t.mixes);
    s.fw.cpu.write(kTvasAt, &t.tvas, sizeof t.tvas);
    s.fw.cpu.wr8(kControlsAt, t.controls);
    s.fw.cpu.write(kBytesAt, &t.bytes, sizeof t.bytes);
    s.fw.cpu.write(kCosmsAt, &t.cosms, sizeof t.cosms);
    s.fw.cpu.write(kOscsAt, &t.oscs, sizeof t.oscs);
    std::string what;
    for (int k = 0; k < kEventsACase && what.empty(); ++k, ++events) {
        settingsDraw(s, rng, Flipped);
        what = Events[rng() % std::size(Events)](s, rng);
        calls += (long)s.mine.log.size();
    }
    return what;
}
