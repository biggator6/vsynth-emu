#pragma once
// The VC-2 mode's effects (v458; HARDWARE_FIRMWARE.md, "The VC-2 mode's effects"): rom.4's effect driver and chip
// (XtFxUnit on vc2Fx(), step 4.4's), behind the plugin's one Effects switch as the V-Synth's are (PluginEffects.cpp).
// - Vc2Fx: the unit and what its driver is given, the processor's and VSynthEmuVc2PlayTest's reference's alike: each ID
//   it reads as rom.4 answers it (vc2FxGive), the patch loaded at the start and again when another comes, the control
//   changes it keeps (CC 91 and 93, the MFX's Reverb and Chorus Sends: v402), then the driver's tick and pump.  Not
//   given: the Matrix's MFX-PRMs (rom.4 sends them to the part's mixer, v401) and an outside clock (the part's clock is
//   INT until the real-time bytes: VC2_MODE_PLAN.md, step 4.5's [deferred]).
// - PluginVc2Fx: its threads.  The unit made on the control thread the first time the Effects switch is on in the VC-2
//   mode and started there, then handed to the audio thread; the audio thread's control changes reach the control
//   thread through a ring.
#include "Vc2SystemStore.h"
#include "XtFxControl.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

// A control change for the driver, as heard
struct Vc2FxControl {
    uint8_t number, value;
};
// [FW] the control changes rom.4's driver keeps (its receiver 0x89360b90): the MFX's Reverb Send, its Chorus Send
inline constexpr uint8_t kVc2FxReverbSend = 91, kVc2FxChorusSend = 93;

// The driver's IDs as rom.4 answers its reads: the patch's from `store` (rom.4's patch store, the store the part plays:
// first-bank IDs where the part reads them, zone 1's and COSM 1's; the effect types' own from their blocks' slots,
// vc2StoreTyped), the effects' system ones from `system` on the areas the driver reads them on (vc2FxSystemAreaOf); IDs
// 0…2 and 0x34 left at the driver's own defaults.  Control thread
void vc2FxGive(XtFxControl& c, const std::vector<uint8_t>& store, const Vc2System& system);

class Vc2Fx {
public:
    explicit Vc2Fx(const std::string& rom4);   // control thread: rom.4's driver started as the XT starts it
    Vc2Fx(const Vc2Fx&) = delete;
    Vc2Fx& operator=(const Vc2Fx&) = delete;
    bool ok() const { return unit_.ok(); }
    // Control thread, before the audio thread has it: the IDs (no store: every ID's default), the patch loaded, the
    // chip played until it has all of it
    void start(const std::vector<uint8_t>* store, const Vc2System& system);
    // Control thread, a tick: the IDs again when they `moved`, the patch loaded again when `another` came, the driver's
    // tick, the control changes, its pump
    void tick(const std::vector<uint8_t>* store, const Vc2System& system, bool moved, bool another,
              std::span<const Vc2FxControl> controls);
    XtFxUnit& unit() { return unit_; }

private:
    const std::vector<uint8_t>& storeOf(const std::vector<uint8_t>* store) const { return store ? *store : defaults_; }
    XtFxUnit unit_;
    std::vector<uint8_t> defaults_;   // every ID's default, for no store (vc2DefaultStore)
};

class PluginVc2Fx {
public:
    // Control thread: `wanted` (the VC-2 mode with the Effects switch on) makes and starts the unit the first time;
    // once made, each tick gives it what moved: the store by its number `storeSeq` and the patch's generation `gen`,
    // the system by `systemSeq`, and the control changes heard
    void tick(bool wanted, const std::vector<uint8_t>* store, uint64_t storeSeq, long gen, const Vc2System& system,
              uint64_t systemSeq);
    // Audio thread: the unit once started, none before or when it didn't start
    XtBusSink* sink() const { return sink_.load(std::memory_order_acquire); }
    // Audio thread: a control change; the driver's (CC 91 and 93) kept for the next tick, a full ring's dropped
    void controller(uint8_t number, uint8_t value);
    const std::string& failed() const { return failed_; }   // control thread: why it didn't start, empty if it did
    // For the checks, control thread: an ID as the driver's store holds it, 0 with no unit
    int driverValue(int id) const { return fx_ ? fx_->unit().control().get(id) : 0; }

private:
    std::unique_ptr<Vc2Fx> fx_;
    std::atomic<XtBusSink*> sink_ { nullptr };
    std::array<Vc2FxControl, 256> ring_ {};   // the control changes waiting, at most 256
    std::atomic<size_t> head_ { 0 }, tail_ { 0 };   // the control thread's next to take, the audio thread's next to put
    std::vector<Vc2FxControl> taken_;
    uint64_t storeSeq_ = 0, systemSeq_ = 0;
    long gen_ = -1;
    std::string failed_;
};
