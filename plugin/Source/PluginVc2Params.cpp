// The VC-2's host parameters: see PluginVc2Params.h (v451).  The set is fixed here, by rom.4's IDs, so a host's list is
// the same with rom.4 or without; VSynthEmuArpPluginTest's VC-2 part (Vc2ParamPluginTest.cpp) holds it to rom.4's
// descriptors and names, and each parameter to its ID's name, range, default and text.
#include "PluginVc2Params.h"
#include "PluginParamIds.h"
#include "Vc2Params.h"
#include "Vc2SystemStore.h"
#include <algorithm>

namespace {

// [FW] rom.4's IDs of the set, as runs: `first` to `last` every `step`, the run `repeats` times `stride` IDs apart
// (research/VC2_VOCAL_DESIGNER.md, "The parameter tables"; their sections Vc2PatchStore.cpp's).  Left out: the patch's
// name (its data, as the V-Synth's), the dummies and reserved areas, the effects' and COSMs' slots (their types' own IDs
// are read through them).
struct IdRun {
    int first, last, step = 1, repeats = 1, stride = 0;
};
// The patch's, once: the common (tune, keys, portamento, tempo, bend, the effects' switches, the zones' splits, Scale
// Tune, the Matrix, the vocoder block, the patch EQ), the effects' Types and levels, Beam Type to the End Point
constexpr IdRun kPatch[] = {
    { 0x12c, 0x136 }, { 0x13a, 0x151 }, { 0x153, 0x169, 2 }, { 0x16a, 0x191 }, { 0x193, 0x1a5 }, { 0x1af, 0x1b4 },
    { 0x1b7, 0x1ba }, { 0x23c, 0x23e }, { 0x270, 0x271 }, { 0x4a6, 0x4b2 },
};
// A zone's: the tone (both OSCs with the VOCODER OSC's; their Start Offsets 0x306 and 0x33a besides), the 13
// envelopes and the 5 LFOs
constexpr IdRun kZone[] = {
    { 0x2d3, 0x2fa }, { 0x2fe, 0x2fe }, { 0x306, 0x32e }, { 0x332, 0x332 }, { 0x33a, 0x34b }, { 0x34d, 0x352 },
    { 0x354, 0x36f }, { 0x371, 0x37a }, { 0x37c, 0x383 }, { 0x389, 0x38d, 4 },
    { 0x390, 0x390, 1, 13, 13 }, { 0x392, 0x393, 1, 13, 13 }, { 0x395, 0x39b, 1, 13, 13 },
    { 0x439, 0x439, 1, 5, 8 }, { 0x43b, 0x440, 1, 5, 8 },
};
// A COSM's: its Switch and Type, then its types' own IDs
constexpr IdRun kCosm[] = { { 0x462, 0x463 }, { 0x927, 0x9ab } };
// The system's the V-Synth's parameters are: Master Level, Clock Source, Output Gain, External In Type, and the
// controller assigns (Beam 1-4 L and R, Pad X and Y, Time Trip X and Y, Knob 1 and 2)
constexpr IdRun kSystem[] = { { 0x40, 0x40 }, { 0x43, 0x43 }, { 0x78, 0x78 }, { 0x7a, 0x7a }, { 0x89, 0x96 } };
constexpr int kStartOffsets[] = { 0x306, 0x33a };   // [FW] 32 bits wide, their range one value: the V-Synth's rule
constexpr int kCosmFirst = 0x462, kCosmTypedFirst = 0x926;
constexpr int kZones = 2, kCosms = 2, kMostNamed = 512;

template <size_t N>
std::vector<int> runIds(const IdRun (&runs)[N]) {
    std::vector<int> ids;
    for (const IdRun& r : runs)
        for (int k = 0; k < r.repeats; ++k)
            for (int id = r.first + r.stride * k; id <= r.last + r.stride * k; id += r.step) ids.push_back(id);
    std::ranges::sort(ids);
    return ids;
}

void addIds(std::vector<Vc2HostParam>& set, const std::vector<int>& ids, int zone, int cosm) {
    for (const int id : ids) set.push_back({ id, zone, cosm, vc2HostParamId(id, zone, cosm) });
}

std::vector<Vc2HostParam> makeHostSet() {
    std::vector<Vc2HostParam> set;
    addIds(set, runIds(kPatch), 0, 0);
    for (int zone = 0; zone < kZones; ++zone) {
        addIds(set, runIds(kZone), zone, 0);
        for (int cosm = 0; cosm < kCosms; ++cosm) addIds(set, runIds(kCosm), zone, cosm);
    }
    addIds(set, runIds(kSystem), 0, 0);
    return set;
}

bool cosmHoldsId(int id) { return id >= kCosmTypedFirst || id == kCosmFirst || id == kCosmFirst + 1; }
bool startOffsetId(int id) { return std::ranges::find(kStartOffsets, id) != std::end(kStartOffsets); }

juce::String idHex(int id) { return juce::String::toHexString(id).paddedLeft('0', 3); }

// "VC-2 ", zone 2's "Z2 ", a COSM's "COSM n " for rom.4's "COSM ", then rom.4's name (or the ID, in a stand-in)
juce::String hostName(const Vc2HostParam& p, bool rom4) {
    juce::String name = rom4 ? juce::String(vc2ParamName(p.id)) : "0x" + idHex(p.id);
    juce::String head = p.zone ? "VC-2 Z2 " : "VC-2 ";
    if (cosmHoldsId(p.id)) {
        head << "COSM " << (p.cosm + 1) << " ";
        if (name.startsWith("COSM ")) name = name.substring(5);
    }
    return head + name;
}

// Its range and default as rom.4's descriptor reads them (the value less the centre; a typed ID's as they are)
struct HostRange {
    int lo, hi, def;
};
HostRange hostRange(int id, bool rom4) {
    if (startOffsetId(id)) return { 0, kStartOffsetMax, 0 };
    if (!rom4) return { kVc2StandInLo, kVc2StandInHi, 0 };
    if (const XtTypedDesc* t = vc2TypedDesc(id)) return { t->min, t->max, std::clamp(t->def, t->min, t->max) };
    const XtParamDesc* d = vc2ParamDesc(id);
    const int lo = d->min - d->centre, hi = d->max - d->centre;
    // v457: a system ID the part reads starts at rom.4's system default, as the XT starts with no system file to load
    // (its defaults' routines: the controller assigns' are not their descriptors' 0)
    int system = 0;
    return { lo, hi, std::clamp(vc2SystemDefault(id, system) ? system : d->def - d->centre, lo, hi) };
}

} // namespace

juce::String vc2HostParamId(int id, int zone, int cosm) {
    return kVc2ParamPrefix + (zone ? "z2" : "") + (cosm ? "c2" : "") + "_" + idHex(id);
}

const std::vector<Vc2HostParam>& vc2HostParams() {
    static const std::vector<Vc2HostParam> kSet = makeHostSet();
    return kSet;
}

std::unique_ptr<juce::RangedAudioParameter> vc2HostParameter(const Vc2HostParam& p) {
    const bool rom4 = vc2ParamDesc(0) != nullptr;
    const HostRange r = hostRange(p.id, rom4);
    const int id = p.id, lo = r.lo, hi = r.hi;
    const bool named = rom4 && !startOffsetId(id);
    const auto text = juce::AudioParameterIntAttributes()
        .withStringFromValueFunction([id, named](int v, int) {
            return named ? juce::String(vc2ValueText(id, v)) : juce::String(v);
        })
        .withValueFromStringFunction([id, lo, hi, named](const juce::String& t) {
            if (named && hi - lo < kMostNamed)
                for (int v = lo; v <= hi; ++v)
                    if (t.trim() == juce::String(vc2ValueText(id, v)).trim()) return v;
            return t.getIntValue();
        });
    return std::make_unique<juce::AudioParameterInt>(p.paramId, hostName(p, rom4), lo, hi, r.def, text);
}

void vc2AddHostParameters(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params) {
    for (const Vc2HostParam& p : vc2HostParams()) params.push_back(vc2HostParameter(p));
}
