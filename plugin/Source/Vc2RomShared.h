#pragma once
// The shared routines the wiring gives its ports (Vc2ToneShared: the velocity curve, exp2) as rom.4's own, run in the
// interpreter between its calls of a check's routines, so both sides of a check share them (v222's, moved out of
// Vc2VocoderOscCheck.cpp in v230 for xt_vc2osc too).  Check builds only.
#include "Vc2CheckRig.h"
#include "Vc2VocoderOsc.h"
#include <bit>

class RomShared final : public Vc2ToneShared {
public:
    static constexpr uint32_t kVelocity = 0x893a7fb0, kExp2 = 0x893a8340;   // [FW]
    explicit RomShared(Vc2Fw& fw) : fw_(fw) {}
    int velocity(int velocity, int sens, int curve) const override {
        uint32_t r0 = 0;
        faulted = !fw_.call(kVelocity, { (uint32_t)velocity, (uint32_t)sens, (uint32_t)curve }, &r0) || faulted;
        return (int)r0;
    }
    float exp2(float x) const override {
        fw_.cpu.fb[0][4] = std::bit_cast<uint32_t>(x);
        faulted = !fw_.call(kExp2, {}) || faulted;
        return std::bit_cast<float>(fw_.cpu.fb[0][0]);
    }
    mutable bool faulted = false;

private:
    Vc2Fw& fw_;
};
