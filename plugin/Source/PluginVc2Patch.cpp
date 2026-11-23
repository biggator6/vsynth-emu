// A VC-2 patch on the processor: see PluginVc2Patch.h (v452).  Its check is VSynthEmuArpPluginTest's VC-2 patch part
// (Vc2PatchPluginTest.cpp).
#include "PluginVc2Patch.h"
#include "PluginArp.h"
#include "PluginParamIds.h"
#include "XtArpeggioPatch.h"
#include "PluginProcessor.h"
#include "Vc2Params.h"
#include "Vc2PatchFile.h"
#include "Vc2PatchStore.h"

namespace {

constexpr int kStoreFirst = 0x120;   // [FW] the store's first ID, Patch Name 1; the system's are below it
const juce::Identifier kStoreKey { "vc2Store" }, kNameKey { "vc2PatchName" };
constexpr int kArpState = 0x4b4, kArpNote = 0x4b5, kArpStep1 = 0x4b7;   // [FW] a line's Control Switch, Number, Step 1

} // namespace

XtArpPattern vc2ArpPattern(const std::vector<uint8_t>& store) {
    if (store.size() < (size_t)kVc2StoreBytes) return xtArpDefaultPattern();
    const auto read = [&store](int id, int line) { return xtStoreValue(vc2Store(), store, id, line); };
    return xtArpPatternRead(read, kArpState, kArpNote, kArpStep1);
}

std::optional<int> vc2PatchValue(const std::vector<uint8_t>& store, const Vc2HostParam& p) {
    if (p.id < kStoreFirst) return std::nullopt;
    if (p.id >= kVc2FirstTyped) return vc2StoreTyped(store, p.id, p.zone, p.cosm);
    const XtParamDesc* d = vc2ParamDesc(p.id);
    const int v = xtStoreValue(vc2Store(), store, p.id, p.zone, p.cosm);
    return d && d->min == d->max ? startOffsetParam(v) : v;   // [FW] the Start Offsets', on the V-Synth's range
}

void vc2PatchToState(const PluginVc2Patch& patch, juce::ValueTree& state) {
    const std::lock_guard<std::mutex> hold(patch.lock);
    if (patch.store.empty()) {   // a session restored may have left the last one's in the state
        state.removeProperty(kStoreKey, nullptr);
        state.removeProperty(kNameKey, nullptr);
        return;
    }
    state.setProperty(kStoreKey, juce::MemoryBlock(patch.store.data(), patch.store.size()).toBase64Encoding(), nullptr);
    state.setProperty(kNameKey, patch.name, nullptr);
}

void vc2PatchFromState(PluginVc2Patch& patch, const juce::ValueTree& state) {
    juce::MemoryBlock bytes;
    const bool has = bytes.fromBase64Encoding(state.getProperty(kStoreKey).toString())
                     && bytes.getSize() == (size_t)kVc2StoreBytes;
    const auto* b = static_cast<const uint8_t*>(bytes.getData());
    const std::lock_guard<std::mutex> hold(patch.lock);
    patch.store = has ? std::vector<uint8_t>(b, b + bytes.getSize()) : std::vector<uint8_t>();
    patch.name = has ? state.getProperty(kNameKey).toString() : juce::String();
}

bool VSynthEmuProcessor::loadVc2Patch(const juce::File& file) {
    juce::MemoryBlock bytes;
    Vc2PatchFile read;
    std::string why = "not a file";
    if (!file.loadFileAsData(bytes)
        || !vc2PatchRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), read, &why)) {
        setStatus("Not a VC-2 patch: " + file.getFileName() + ": " + juce::String(why));
        return false;
    }
    for (const Vc2HostParam& h : vc2HostParams())
        if (const auto v = vc2PatchValue(read.store, h)) set(h.paramId, (float)*v);
    {
        const std::lock_guard<std::mutex> hold(vc2Patch_.lock);
        vc2Patch_.store = read.store;
        vc2Patch_.name = read.name;
    }
    setStatus("VC-2 patch: " + juce::String(read.name));
    return true;
}

std::vector<uint8_t> VSynthEmuProcessor::vc2PatchStore() const {
    const std::lock_guard<std::mutex> hold(vc2Patch_.lock);
    return vc2Patch_.store;
}

juce::String VSynthEmuProcessor::vc2PatchName() const {
    const std::lock_guard<std::mutex> hold(vc2Patch_.lock);
    return vc2Patch_.name;
}
