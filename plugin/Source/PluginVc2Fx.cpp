// The VC-2 mode's effects: see PluginVc2Fx.h (v458).  Checked by VSynthEmuVc2PlayTest's effects run (Vc2PlayFx.cpp)
// against Vc2PlayDirect with a Vc2Fx of its own.
#include "PluginVc2Fx.h"
#include "PluginVc2Patch.h"
#include "Vc2Params.h"
#include "Vc2PatchStore.h"
#include "XtEncoded.h"
#include "XtFiles.h"
#include "XtFxImage.h"

namespace {
constexpr int kCosmTypedFirst = 0x926;   // [FW] the COSMs' typed IDs from here: the effect types' own are below it
}

void vc2FxGive(XtFxControl& c, const std::vector<uint8_t>& store, const Vc2System& system) {
    const int ids = vc2Fx().ids;
    for (int id = 0; id < ids; ++id) {
        Vc2SystemArea area {};
        if (vc2FxSystemAreaOf(id, area)) c.set(id, vc2SystemValue(system, area, id));
        else if (id >= kVc2FirstTyped && id < kCosmTypedFirst) c.set(id, vc2StoreTyped(store, id));
        else if (id >= kVc2FirstStored && id < kVc2FirstTyped) c.set(id, xtStoreValue(vc2Store(), store, id));
    }
}

Vc2Fx::Vc2Fx(const std::string& rom4) : unit_(rom4.c_str(), vc2Fx()), defaults_(vc2DefaultStore()) {}

void Vc2Fx::start(const std::vector<uint8_t>* store, const Vc2System& system) {
    if (!ok()) return;
    vc2FxGive(unit_.control(), storeOf(store), system);
    unit_.control().loadPatch();
    unit_.settle();
}

void Vc2Fx::tick(const std::vector<uint8_t>* store, const Vc2System& system, bool moved, bool another,
                 std::span<const Vc2FxControl> controls) {
    XtFxControl& c = unit_.control();
    if (moved) vc2FxGive(c, storeOf(store), system);
    if (another) c.wantPatch();
    c.tick();
    for (const Vc2FxControl& k : controls) c.controller(k.number, k.value);
    c.pump();
}

void PluginVc2Fx::tick(bool wanted, const std::vector<uint8_t>* store, uint64_t storeSeq, long gen,
                       const Vc2System& system, uint64_t systemSeq) {
    if (!fx_) {
        head_.store(tail_.load(std::memory_order_acquire), std::memory_order_release);   // with no unit, none kept
        if (!wanted || !failed_.empty()) return;
        auto made = std::make_unique<Vc2Fx>(xtSynthVc2RomPath());
        made->start(store, system);
        if (!made->ok()) {
            failed_ = made->unit().control().error();
            return;
        }
        fx_ = std::move(made);
        storeSeq_ = storeSeq;
        systemSeq_ = systemSeq;
        gen_ = gen;
        head_.store(tail_.load(std::memory_order_acquire), std::memory_order_release);   // none heard before it counts
        sink_.store(&fx_->unit(), std::memory_order_release);
        return;   // started: its first tick the next
    }
    const bool moved = storeSeq != storeSeq_ || systemSeq != systemSeq_, another = gen != gen_;
    storeSeq_ = storeSeq;
    systemSeq_ = systemSeq;
    gen_ = gen;
    taken_.clear();
    for (size_t at = head_.load(std::memory_order_relaxed); at != tail_.load(std::memory_order_acquire); ++at)
        taken_.push_back(ring_[at % ring_.size()]);
    head_.store(head_.load(std::memory_order_relaxed) + taken_.size(), std::memory_order_release);
    fx_->tick(store, system, moved, another, taken_);
}

void PluginVc2Fx::controller(uint8_t number, uint8_t value) {
    if (number != kVc2FxReverbSend && number != kVc2FxChorusSend) return;
    const size_t tail = tail_.load(std::memory_order_relaxed);
    if (tail - head_.load(std::memory_order_acquire) >= ring_.size()) return;
    ring_[tail % ring_.size()] = { number, value };
    tail_.store(tail + 1, std::memory_order_release);
}
