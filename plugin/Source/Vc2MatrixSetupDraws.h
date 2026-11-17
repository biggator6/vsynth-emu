#pragma once
// xt_vc2cosm's fourth part's draws (v247, Vc2MatrixSetupCheck.cpp): the parameter store both sides read, its values
// by each parameter's kind (a store's range, now and then past it), G drawn with rom.4's source routines in its
// records, and the destination routines' arguments called alone.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2MatrixCheck.h"
#include "Vc2MatrixSetup.h"
#include <map>
#include <utility>

inline constexpr int kSetupSystem = -1, kSetupLevels = -2;   // a call's block: the system's, or 0x893b1ee0's
// In Vc2Fw's RAM: a second copy of the channel's state, so G's channel word and M's can differ and both be read
inline constexpr uint32_t kSetupChannelCopyAt = kVc2ChannelAt + 0x400;
inline constexpr std::array<uint32_t, 2> kSetupChannels { kVc2ChannelAt, kSetupChannelCopyAt };

// A call, rom.4's hooks' or the port's: a read's block (a part, kSetupSystem) and ID, or 0x893b1ee0 (kSetupLevels)
struct SetupCall {
    int32_t kind;
    int32_t id;
    bool operator==(const SetupCall&) const = default;
};

// The parameters both sides read: a value by block and ID, drawn at its first reading and kept until the store is
// drawn again
struct SetupStore {
    std::map<std::pair<int, int>, int32_t> values;
    std::mt19937 rng;
    void redraw(std::mt19937& from) {
        values.clear();
        rng.seed(from());
    }
    int32_t value(int block, int16_t id);
};

// A destination routine's arguments when it is called alone
struct SetupArgs {
    int32_t source, dest, sens;
};
SetupArgs vc2DrawSetupArgs(std::mt19937& rng);

// G drawn: noise, its records' routines rom.4's sources (the levels' refresh calls them), its channel word one of the
// world's two copies
void vc2DrawSettings(Vc2MatrixSettings& g, std::mt19937& rng);
#endif
