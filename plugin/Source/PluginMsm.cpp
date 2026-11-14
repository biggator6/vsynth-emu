// The Multi Step Modulator's plugin side (v193): its 98 settings as host parameters, appended after every other (the
// arpeggio's last) so a host's list keeps its places; read into XtSynth's controls each block and written back from a
// patch's; and a patch's load, which stops it as the XT's does.  HARDWARE_FIRMWARE.md, "The Multi Step Modulator".
#include "PluginMsm.h"
#include "PluginProcessor.h"
#include "PluginSetting.h"
#include <array>

namespace {

using namespace Setting;

// The 72 destinations [FW 0x89461a70, ID 0x496], in xtmsm::Dest's order: the patch's three, each OSC's 14, each COSM's
// 13 (CSM1-, CSM2-), the TVA's 9, the sends and MFX-PRM1…3
juce::StringArray destNames() {
    const auto osc = juce::StringArray::fromTokens("PIT-CRS PIT-FINE PITCH PENV-DPT TIME/PW TENV-DPT FORMA/FAT "
                                                   "FENV-DPT LFO-RATE LFO-PCH LFO-TM/PW LFO-FR/FT LFO-LVL LVL", false);
    const auto cosm = juce::StringArray::fromTokens("PRM1 PRM2 ENV1-ATK ENV1-DCY ENV1-SUS ENV1-REL ENV2-ATK "
                                                    "ENV2-DCY ENV2-SUS ENV2-REL LFO-RATE LFO-PRM1 LFO-PRM2", false);
    juce::StringArray names { "OSC1+2-PIT-CRS", "OSC1+2-PIT-FINE", "PORTA-TIME" };
    const std::array<std::pair<const char*, const juce::StringArray*>, 4> blocks { {
        { "OSC1-", &osc }, { "OSC2-", &osc }, { "CSM1-", &cosm }, { "CSM2-", &cosm } } };
    for (const auto& [prefix, list] : blocks)
        for (const juce::String& n : *list) names.add(prefix + n);
    names.addTokens("TVA-LVL TVA-PAN TVA-ENV-ATK TVA-ENV-DCY TVA-ENV-SUS TVA-ENV-REL TVA-LFO-RATE TVA-LFO-LVL "
                    "TVA-LFO-PAN MFX-SEND CHO-SEND REV-SEND MFX-PRM1 MFX-PRM2 MFX-PRM3", false);
    return names;
}

// One kind of setting: its ID and name after the track's part, its range and default as the getter gives them [FW
// 0x897a167c, the centre taken off], a Choice's values [FW 0x89461a70] and a Number's text
struct MsmSetting {
    const char* id;          // the plugin's after "msm" or "msmN", never changed
    const char* name;        // after "MSM " or "MSM Track N ": the firmware's [FW 0x89790338]
    Kind kind;
    int lo, hi, def;
    const char* values;      // a Choice's, "|" between; nullptr for the destinations
    Text text;
};

// The two of the whole, IDs 0x48d and 0x48e
const MsmSetting kWhole[] = {
    { "Switch", "Master Switch", Kind::Toggle, 0, 1, 0, nullptr, kPlain },
    { "KeySync", "Key Sync", Kind::Choice, 0, 2, 1, "OFF|PART|VOICE", kPlain },
};
// A track's eight, from 0x48f + 24·n.  The firmware names the Loop "Track Loop Switch" in every track, which a host's
// list can't tell apart, so here it has its track's number as the others have.  End Point and Zone show 1…16, the
// manual's [MAN p. 84], where the firmware's routine gives 0…15, as the arpeggio's End Point (v190)
const MsmSetting kTrack[] = {
    { "Switch", "Switch", Kind::Toggle, 0, 1, 1, nullptr, kPlain },
    { "Loop", "Loop Switch", Kind::Toggle, 0, 1, 1, nullptr, kPlain },
    { "Type", "Type", Kind::Choice, 0, 1, 0, "STEP|SMOOTH", kPlain },
    { "Grid", "Grid", Kind::Choice, 0, 5, 3, "QUARTER|8TH|8TH-T|16TH|16TH-T|32TH", kPlain },
    { "EndPoint", "End Point", Kind::Number, 0, 15, 15, nullptr, { stepText, stepValue } },
    { "Direction", "Direction", Kind::Choice, 0, 5, 0, "FWD1|FWD2|FWD3|BWD1|BWD2|BWD3", kPlain },
    { "Zone", "Zone", Kind::Number, 0, 15, 0, nullptr, { stepText, stepValue } },
    { "Dest", "Destination", Kind::Choice, 0, 71, 0, nullptr, kPlain },
};
// Its 16 steps, from 0x497 + 24·n: "Step 01 Value" …, −63…+63, "+0" as the firmware's routine has it
const MsmSetting kStep { "Step", "Step", Kind::Number, -63, 63, 0, nullptr, { signedText, plainValue } };

// One of the 98: its parameter, and the XT's ID it is
struct MsmParam {
    Spec spec;
    int fwId;
};

MsmParam paramFor(const MsmSetting& s, const juce::String& id, const juce::String& name, int fwId) {
    juce::StringArray values;
    if (s.kind == Kind::Choice) values = s.values ? juce::StringArray::fromTokens(s.values, "|", "") : destNames();
    return { { id, name, s.kind, s.lo, s.hi, s.def, values, s.text }, fwId };
}

void addTrack(std::vector<MsmParam>& list, int n) {
    const juce::String id = "msm" + juce::String(n + 1), name = "MSM Track " + juce::String(n + 1) + " ";
    const int first = kXtMsmFirstId + 2 + kXtMsmTrackIds * n;
    for (int i = 0; i < 8; ++i)
        list.push_back(paramFor(kTrack[i], id + kTrack[i].id, name + kTrack[i].name, first + i));
    for (int s = 1; s <= 16; ++s) {
        const juce::String step = juce::String(s), stepName = "Step " + step.paddedLeft('0', 2) + " Value";
        list.push_back(paramFor(kStep, id + "Step" + step, name + stepName, first + 7 + s));
    }
}

// In the XT's order: 0x48d, 0x48e, then each track's 24
std::vector<MsmParam> makeParams() {
    std::vector<MsmParam> list;
    for (int i = 0; i < 2; ++i)
        list.push_back(paramFor(kWhole[i], "msm" + juce::String(kWhole[i].id), "MSM " + juce::String(kWhole[i].name),
                                kXtMsmFirstId + i));
    for (int n = 0; n < XtMsmSettings::kTracks; ++n) addTrack(list, n);
    return list;
}
const std::vector<MsmParam> kMsmParams = makeParams();

} // namespace

const juce::String& msmParamId(int fwId) {
    static const juce::String none;
    const int i = fwId - kXtMsmFirstId;
    return i >= 0 && i < (int)kMsmParams.size() ? kMsmParams[(size_t)i].spec.id : none;
}

int msmTrackFwId(int t, int i) { return kXtMsmFirstId + 2 + kXtMsmTrackIds * t + i; }

void VSynthEmuProcessor::addMsmParams(ParamList& params) {
    for (const MsmParam& p : kMsmParams) params.push_back(Setting::paramOf(p.spec));
}

// Audio thread, each block: every setting to the field the firmware's loaders read its ID into (xtMsmField)
void VSynthEmuProcessor::readMsmControls(XtSynth::Controls& c) const {
    for (const MsmParam& p : kMsmParams) *xtMsmField(c.msm, p.fwId) = knob(p.spec.id);
}

void VSynthEmuProcessor::writeMsmControls(const XtSynth::Controls& c) {
    for (const MsmParam& p : kMsmParams) set(p.spec.id, (float)xtMsmValue(c.msm, p.fwId));
}

// A patch's controls (applyPatch): onto the parameters, then the synth told a patch came, so the Multi Step Modulator
// stops and reads them at its next setControls, as the firmware's patch message does (0x894d9950), where an edit, a
// host's automation, only reads them (0x894d9b20)
void VSynthEmuProcessor::setPatchControls(const XtSynth::Controls& c) {
    setControls(c);
    synth_.msmPatch();
}
