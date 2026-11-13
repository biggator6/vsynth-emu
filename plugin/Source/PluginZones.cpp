// The zones' plugin side (v195): the sixteen Range Upper keys that split the keyboard as host parameters, appended after
// every other so a host's list keeps its places, read into XtSynth's controls each block and written back from a patch's;
// and a patch's zones 2…16, which notes on them play (loadZones, moved from PluginProcessor.cpp as it stood).
#include "PluginProcessor.h"
#include "PluginParamIds.h"
#include "PluginPatchFiles.h"
#include "PluginSetting.h"
#include "XtPatch.h"
#include <algorithm>
#include <array>
#include <map>

namespace {

// Each zone's Range Upper key by the XT's IDs 0x124…0x133: its name [FW 0x89790338], 0…127 and 127 [FW 0x897a167c], and
// the number as its text, as the firmware's routine gives it [FW 0x89461a70] and the manual says ("the top note number")
constexpr int kUpperLo = 0, kUpperHi = 127, kUpperDefault = 127;

// Each one's ID as the audio thread asks for it, made once at load and not each block
const std::array<juce::String, kXtZones> kZoneIds = [] {
    std::array<juce::String, kXtZones> ids;
    for (int z = 0; z < kXtZones; ++z) ids[(size_t)z] = Params::zoneUpperId(z);
    return ids;
}();

// What a zone plays, for the editor's Zones card: each OSC switched on, ANALOG or its wave by number, a wave the project's
// folder doesn't have said so; with neither on, why it is silent
juce::String playsOf(const XtSynth::Controls& c, const std::vector<uint8_t>& store, int z,
                     const std::map<int, XtWaveFile>& files) {
    juce::StringArray what;
    for (int k = 0; k < 2; ++k) {
        const XtSynth::OscControls& o = k ? c.osc2 : static_cast<const XtSynth::OscControls&>(c);
        const int number = xtPatchWaveNumber(store, z, k);
        const auto file = files.find(number);
        const bool here = file != files.end() && file->second.frames > 0;
        if (o.on && o.oscType == 0) what.add("analog");
        if (o.on && o.oscType == 1) what.add("wave " + juce::String(number) + (here ? "" : " not here"));
    }
    return what.isEmpty() ? juce::String("OSCs off, silent") : what.joinIntoString(", ");
}

} // namespace

// In the state, a line a zone, so the editor hears it change; message thread
juce::StringArray VSynthEmuProcessor::zonePlays() const {
    return juce::StringArray::fromLines(apvts.state.getProperty("zonePlays").toString());
}

void VSynthEmuProcessor::setZonePlays(const juce::StringArray& plays) {
    apvts.state.setProperty("zonePlays", plays.joinIntoString("\n"), nullptr);
}

// The store a session kept of its patch (loadZones), empty for none
std::vector<uint8_t> VSynthEmuProcessor::keptStore() const {
    juce::MemoryOutputStream bytes;
    const juce::String kept = apvts.state.getProperty("patchStore").toString();
    if (kept.isEmpty() || !juce::Base64::convertFromBase64(bytes, kept)) return {};
    const auto* b = static_cast<const uint8_t*>(bytes.getData());
    return { b, b + bytes.getDataSize() };
}

// A patch's zones 2…16, from the store it kept, their waves from its folder; what isn't there goes in `notes`.  A session
// from before v195 has no splits among its parameters (`hadSplits` false): its patch's, from that store, or none.
void VSynthEmuProcessor::restoreZones(juce::StringArray& notes, bool hadSplits) {
    const std::vector<uint8_t> store = xtSynthRomReady() ? keptStore() : std::vector<uint8_t>();
    for (int z = 0; z < kXtZones && !hadSplits; ++z)
        set(kZoneIds[(size_t)z], (float)(store.empty() ? kUpperDefault : xtPatchValue(store, 0x124 + z)));
    if (!store.empty()) {
        loadZones(store, juce::File(apvts.state.getProperty("patchWaves").toString()), notes);
        return;
    }
    ++zonesGen_;
    patchZones_.store(1);
    zonesReady_.store(true);
    setZonePlays({});
    synth_.setZones(nullptr);
}

void VSynthEmuProcessor::addZoneParams(ParamList& params) {
    for (int z = 0; z < kXtZones; ++z) {
        const juce::String name = "Zone " + juce::String(z + 1) + " Range Upper";
        const Setting::Spec s { kZoneIds[(size_t)z], name, Setting::Kind::Number, kUpperLo, kUpperHi, kUpperDefault, {},
                                Setting::kPlain };
        params.push_back(Setting::paramOf(s));
    }
}

void VSynthEmuProcessor::readZoneControls(XtSynth::Controls& c) const {
    for (int z = 0; z < kXtZones; ++z) c.zoneUpper[z] = knob(kZoneIds[(size_t)z]);
}

void VSynthEmuProcessor::writeZoneControls(const XtSynth::Controls& c) {
    for (int z = 0; z < kXtZones; ++z) set(kZoneIds[(size_t)z], (float)c.zoneUpper[z]);
}

// Zones 2…16 (the user's decision, 2026-11-05): the patch's data, not the parameters.  Their settings go to the synth at once, analog OSCs
// and all, then again with their waves once the pool has encoded them: each wave once, as its own type, its own tuning and loop on it.
// Every zone of every patch (v195): one past the patch's own splits plays when a split moves to it, as on the XT, so the store
// is kept for a patch of one zone too; what isn't here is noted only for a zone the patch's splits reach.
void VSynthEmuProcessor::loadZones(const std::vector<uint8_t>& store, const juce::File& waveFolder, juce::StringArray& notes) {
    const int gen = ++zonesGen_;
    const int zones = xtPatchZones(store);
    patchZones_.store(zones);
    apvts.state.setProperty("patchStore", juce::Base64::toBase64(store.data(), store.size()), nullptr);
    apvts.state.setProperty("patchWaves", waveFolder.getFullPathName(), nullptr);
    if (zones > 1) notes.add(juce::String(zones) + " zones, zone 1 on the panel");
    auto set = std::make_shared<XtSynth::Zones>();
    xtPatchZoneSet(store, xtControls(), *set);
    struct Want { int zone, osc, number; };
    std::vector<Want> want;
    std::map<int, XtWaveFile> files;
    for (int z = 1; z < kXtZones; ++z)
        for (int k = 0; k < 2; ++k) {
            XtSynth::OscControls& o = k ? set->ctl[z].osc2 : static_cast<XtSynth::OscControls&>(set->ctl[z]);
            const int number = xtPatchWaveNumber(store, z, k);
            if (o.oscType != 1 || number <= 0) continue;
            if (!files.count(number)) {
                XtWaveFile w;
                const juce::File f = findWave(waveFolder, number);
                if (!(f.existsAsFile() && readVSynthWave(f, w))) w = XtWaveFile{};
                files[number] = std::move(w);
            }
            const XtWaveFile& w = files[number];
            if (w.frames <= 0 && z < zones) notes.addIfNotAlreadyThere("no wave " + juce::String(number));
            if (w.frames <= 0) continue;
            xtWaveOntoControls(w, o);
            want.push_back({ z, k, number });
        }
    juce::StringArray plays;
    for (int z = 0; z < kXtZones; ++z) plays.add(z ? playsOf(set->ctl[z], store, z, files) : juce::String());
    setZonePlays(plays);
    synth_.setZones(set);
    xtActive_.store(true);
    zonesReady_.store(want.empty());
    if (want.empty()) return;
    addEncode([this, gen, set, want = std::move(want), files = std::move(files)](const std::atomic<bool>&) {
        std::map<int, std::shared_ptr<XtEncoded>> enc;
        for (const auto& [number, w] : files) {
            if (gen != zonesGen_.load()) return;
            if (w.frames <= 0) continue;
            std::vector<std::vector<float>> chans((size_t)w.channels);
            for (long i = 0; i < w.frames; ++i)
                for (int c = 0; c < w.channels; ++c) chans[(size_t)c].push_back((float)w.pcm[(size_t)(i * w.channels + c)] / 32768.0f);
            const int type = w.encodeType == 0 || w.encodeType == 1 || w.encodeType == 3 || w.encodeType == 5 ? w.encodeType : 1;
            auto e = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &w.events, &w.vpdt);   // on its own vpdt, as the file has it
            if (!e) continue;
            xtWaveOntoEncoded(w, *e);
            enc[number] = std::move(e);
        }
        auto withWaves = std::make_shared<XtSynth::Zones>(*set);
        for (const Want& x : want)
            if (const auto it = enc.find(x.number); it != enc.end()) withWaves->wave[x.zone][x.osc] = it->second;
        if (gen != zonesGen_.load()) return;
        synth_.setZones(withWaves);
        zonesReady_.store(true);
    });
}
