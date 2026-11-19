#pragma once
// xt_vc2settings' shared parts (v286): a getter's call as either side logs it (the read object's block, part and kind,
// and the ID), the store both sides answer from, each side's reads (its store, its copy of the held flags, the read
// after which a flag is set, its log), the port's parameters on them, and the two sides.  XT_VC2SETTINGS_MAIN only.
#include "Vc2CheckCalls.h"
#include "Vc2CheckMain.h"
#include "Vc2PartSettings.h"
#include "Vc2PartSettingsTables.h"
#include <vector>

struct SettingsCall {
    int32_t block, part, kind, sub, id;   // sub: a COSM's read object's COSM input (v287), else 0
    bool operator==(const SettingsCall&) const = default;
};

// The parameters' values, one draw per event: by a hash of the seed and the read, mostly a byte's or a depth's, now
// and then at the laws' edges or any word
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
    bool held(int flag) override { return reads_.flags[flag] != 0; }

private:
    SettingsReads& reads_;
};

struct SettingsCounts {
    long events = 0, calls = 0, gaveUp = 0, flipped = 0;
    long moreEvents = 0, moreCalls = 0;   // the second part's (v287)
    std::array<long, 21> byEvent {};
};

// The two sides: rom.4's interpreter with the read objects' set-ups and the getter hooked, the tables, each side's
// reads, the port
struct SettingsSides {
    explicit SettingsSides(Vc2Fw& rig);
    Vc2Fw& fw;
    Vc2PartRows rows {};
    SettingsReads rom {}, mine {};
    SettingsPortParams params { mine };
    Vc2PartSettings settings { params };
    SettingsCounts n {};
};

// rom.4's routine on `args` and the port's `mine()`, both from the same flags, store and flip; what differs in the
// tables, the calls or the answer
std::string settingsOnBoth(SettingsSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                           const std::function<uint32_t()>& mine);
