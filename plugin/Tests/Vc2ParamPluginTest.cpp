// The VC-2's host parameters (v451; PluginVc2Params.h), VSynthEmuArpPluginTest's part for them.  On a processor made with
// rom.4's tables in: every parameter after the V-Synth's 763 is the set's, in its order, and each rom.4's ID's, its name
// rom.4's, its range and default by rom.4's descriptor, its text at five values by rom.4's routine, and that text read
// back to a value with that text.  The set against rom.4's own: every ID of the agreed sections that rom.4 gives a range
// and a name that isn't a dummy's, each in its zones and COSMs.  And a fresh run of this program with no rom.4 to find:
// its list has the same IDs in the same order, each a stand-in holding every value rom.4's range does.
#include "Vc2ParamPluginTest.h"
#include "TestChecks.h"
#include "TestControls.h"
#include "TestMain.h"
#include "TestPatches.h"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginVc2Params.h"
#include "../Source/Vc2Params.h"
#include "../Source/Vc2SystemStore.h"
#include <map>
#include <tuple>

namespace {

constexpr int kVSynthParams = 763;   // the parameters before the VC-2's, as v207 left them
constexpr int kAgreed = 1332;        // the user's scope (2026-11-21): a patch's zones 1 and 2 and the system's
constexpr const char* kRom4 = "V-SynthXT.rom.4.bin";

// [FW] the agreed sections by rom.4's IDs, the test's own reading of research/VC2_VOCAL_DESIGNER.md: a span, and which
// of the patch's parts it is (once, a zone's, a COSM's, the system's)
enum class Part { patch, zone, cosm, system };
struct Span {
    int first, last;
    Part part;
};
constexpr Span kSpans[] = {
    { 0x12c, 0x1b5, Part::patch }, { 0x1b7, 0x1ba, Part::patch }, { 0x23c, 0x23e, Part::patch },
    { 0x270, 0x271, Part::patch }, { 0x4a6, 0x4b3, Part::patch }, { 0x2d3, 0x461, Part::zone },
    { 0x462, 0x463, Part::cosm },  { 0x926, 0x9ab, Part::cosm },  { 0x40, 0x40, Part::system },
    { 0x43, 0x43, Part::system },  { 0x78, 0x78, Part::system },  { 0x7a, 0x7a, Part::system },
    { 0x89, 0x96, Part::system },
};
constexpr int kStartOffsets[] = { 0x306, 0x33a };   // [FW] a range of one value, 32 bits wide: there by the V-Synth's rule

bool startOffset(int id) { return id == kStartOffsets[0] || id == kStartOffsets[1]; }

// rom.4 gives it a range and a name that isn't a dummy's or a reserved area's
bool rom4Ranged(int id) {
    const std::string name = vc2ParamName(id);
    if (name.empty() || name[0] == '!' || name.rfind("Reserv", 0) == 0 || name == "reserved" || name == "Dummy")
        return false;
    if (const XtTypedDesc* t = vc2TypedDesc(id)) return t->min != t->max;
    const XtParamDesc* d = vc2ParamDesc(id);
    return d && d->width != 0 && d->min != d->max;
}

std::vector<int> partIds(Part part) {
    std::vector<int> ids;
    for (const Span& s : kSpans)
        for (int id = s.first; id <= s.last; ++id)
            if (s.part == part && (rom4Ranged(id) || startOffset(id))) ids.push_back(id);
    std::ranges::sort(ids);
    return ids;
}

// The set by rom.4: each part's IDs in order, a zone's and a COSM's in each of theirs
std::vector<std::tuple<int, int, int>> rom4Set() {
    std::vector<std::tuple<int, int, int>> out;
    for (const int id : partIds(Part::patch)) out.emplace_back(id, 0, 0);
    for (int zone = 0; zone < 2; ++zone) {
        for (const int id : partIds(Part::zone)) out.emplace_back(id, zone, 0);
        for (int cosm = 0; cosm < 2; ++cosm)
            for (const int id : partIds(Part::cosm)) out.emplace_back(id, zone, cosm);
    }
    for (const int id : partIds(Part::system)) out.emplace_back(id, 0, 0);
    return out;
}

void setAgainstRom4(TestChecks& check) {
    const auto want = rom4Set();
    const auto& set = vc2HostParams();
    check((int)set.size() == kAgreed, "the set should be the agreed " + std::to_string(kAgreed) + ", not "
                                          + std::to_string(set.size()));
    check(want.size() == set.size(), "rom.4 gives " + std::to_string(want.size()) + " IDs a range in the sections");
    check(vc2HostParamId(0x193, 0, 0) == "vc2_193" && vc2HostParamId(0x2e5, 1, 0) == "vc2z2_2e5"
              && vc2HostParamId(0x927, 0, 1) == "vc2c2_927" && vc2HostParamId(0x927, 1, 1) == "vc2z2c2_927",
          "the IDs should be the agreed form, rom.4's hex ID");
    for (size_t k = 0; k < std::min(want.size(), set.size()); ++k)
        if (want[k] != std::tuple(set[k].id, set[k].zone, set[k].cosm)) {
            check(false, "the set's " + std::to_string(k) + "th, " + set[k].paramId.toStdString() + ", isn't rom.4's "
                         + std::to_string(std::get<0>(want[k])));
            return;
        }
}

// What rom.4 says of an ID: its range and default
struct Rom4Range {
    int lo, hi, def;
};
Rom4Range rom4Range(int id) {
    if (startOffset(id)) return { 0, (1 << 24) - 1, 0 };
    if (const XtTypedDesc* t = vc2TypedDesc(id)) return { t->min, t->max, std::clamp(t->def, t->min, t->max) };
    const XtParamDesc* d = vc2ParamDesc(id);
    Rom4Range r { d->min - d->centre, d->max - d->centre, std::clamp(d->def, d->min, d->max) - d->centre };
    // v457: a system ID the part reads starts at rom.4's system default (VSynthEmuVc2PlayTest reads it as the part does)
    if (int system = 0; vc2SystemDefault(id, system)) r.def = std::clamp(system, r.lo, r.hi);
    return r;
}

// Its name: "VC-2 ", "Z2 " in zone 2, "COSM n " for a COSM's (0x462, 0x463 and the typed from 0x926), then rom.4's
// name, a COSM's without its "COSM "
bool namedAsRom4(const juce::String& name, const Vc2HostParam& h) {
    const juce::String rom4 = juce::String(vc2ParamName(h.id));
    const bool cosm = h.id >= 0x926 || h.id == 0x462 || h.id == 0x463;
    const juce::String bare = cosm && rom4.startsWith("COSM ") ? rom4.substring(5) : rom4;
    const juce::String head = juce::String("VC-2 ") + (h.zone ? "Z2 " : "");
    return name == head + (cosm ? "COSM " + juce::String(h.cosm + 1) + " " : juce::String()) + bare;
}

// Its text at five values rom.4's, and read back to a value with that text
bool textsAsRom4(juce::RangedAudioParameter& r, int id, const Rom4Range& want) {
    for (const int v : { want.lo, want.def, want.hi, (want.lo + want.def) / 2, (want.def + want.hi) / 2 }) {
        const juce::String text = r.getText(r.convertTo0to1((float)v), 64);
        const juce::String rom = startOffset(id) ? juce::String(v) : juce::String(vc2ValueText(id, v));
        const float back = r.convertFrom0to1(r.getValueForText(text));
        if (text != rom || r.getText(r.convertTo0to1(back), 64) != text) return false;
    }
    return true;
}

// Each parameter of the set on `p`: rom.4's name, range, default and text
void eachAgainstRom4(TestChecks& check, VSynthEmuProcessor& p) {
    const auto& params = p.getParameters();
    check(params.size() == kVSynthParams + kAgreed, "the processor should have the V-Synth's 763 and the VC-2's");
    const auto* last = dynamic_cast<juce::RangedAudioParameter*>(params[kVSynthParams - 1]);
    check(last && last->paramID == "fxExtInSource", "the V-Synth's last should stay External In Source");
    long wrong = 0;
    for (size_t k = 0; k < vc2HostParams().size() && kVSynthParams + (int)k < params.size(); ++k) {
        const Vc2HostParam& h = vc2HostParams()[k];
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(params[kVSynthParams + (int)k]);
        const Rom4Range want = rom4Range(h.id);
        const bool ok = r && r->paramID == h.paramId && r->getNormalisableRange().start == (float)want.lo
                        && r->getNormalisableRange().end == (float)want.hi
                        && r->convertFrom0to1(r->getDefaultValue()) == (float)want.def && namedAsRom4(r->name, h)
                        && textsAsRom4(*r, h.id, want);
        if (!ok && wrong++ < 5) check(false, h.paramId.toStdString() + (r ? " \"" + r->name.toStdString() + "\"" : "")
                                                 + " isn't rom.4's");
    }
    check(wrong == 0, std::to_string(wrong) + " of the VC-2's parameters aren't rom.4's");
}

const Vc2HostParam* inSet(const juce::String& paramId) {
    const auto it = std::ranges::find_if(vc2HostParams(), [&](const Vc2HostParam& h) { return h.paramId == paramId; });
    return it == vc2HostParams().end() ? nullptr : &*it;
}

// A fresh run's list against the set: the same IDs in order.  The child is a run with an app folder of its own
// (TestMain.h), where it finds no image: then each is a stand-in holding rom.4's range.  With VSE_XT_ROM naming rom.2,
// the processor finds rom.4 beside it before its parameters are made (vc2FindRomEarly): then each has rom.4's range.
void childList(TestChecks& check, const juce::File& rom, bool early) {
    const juce::String was = juce::SystemStats::getEnvironmentVariable("VSE_XT_ROM", {});
    if (early) setenv("VSE_XT_ROM", rom.getFullPathName().toRawUTF8(), 1);
    const juce::StringArray lines = juce::StringArray::fromLines(thisProgramSays({ rom.getFullPathName(), "vc2ids" }));
    if (early && was.isEmpty()) unsetenv("VSE_XT_ROM");
    if (early && was.isNotEmpty()) setenv("VSE_XT_ROM", was.toRawUTF8(), 1);
    const std::string run = early ? "with VSE_XT_ROM, " : "with no image, ";
    const char* said = early ? "rom.4 in" : "rom.4 out";
    check(lines.contains(said), run + "the child should say " + said);
    std::vector<juce::String> ids;
    long wrong = 0;
    for (const juce::String& line : lines) {
        const juce::StringArray f = juce::StringArray::fromTokens(line, " ", "");
        if (f.size() != 3 || !f[0].startsWith(kVc2ParamPrefix)) continue;
        ids.push_back(f[0]);
        const Vc2HostParam* h = inSet(f[0]);
        const Rom4Range want = h ? rom4Range(h->id) : Rom4Range {};
        const int lo = f[1].getIntValue(), hi = f[2].getIntValue();
        if (!h || (early ? lo != want.lo || hi != want.hi : lo > want.lo || hi < want.hi)) ++wrong;
    }
    check(ids.size() == vc2HostParams().size(), run + "the list should hold every VC-2 parameter, not "
                                                    + std::to_string(ids.size()));
    for (size_t k = 0; k < std::min(ids.size(), vc2HostParams().size()); ++k)
        if (ids[k] != vc2HostParams()[k].paramId) {
            check(false, run + "the " + std::to_string(k) + "th is " + ids[k].toStdString());
            break;
        }
    check(wrong == 0, run + std::to_string(wrong) + (early ? " not rom.4's range" : " can't hold rom.4's range"));
}

// Every VC-2 parameter of `p` moved off its default: one step up, or down from the top
void moveAll(VSynthEmuProcessor& p) {
    for (const Vc2HostParam& h : vc2HostParams())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(h.paramId))) {
            const auto& range = r->getNormalisableRange();
            const float def = r->convertFrom0to1(r->getDefaultValue());
            r->setValueNotifyingHost(r->convertTo0to1(def + 1 <= range.end ? def + 1 : def - 1));
        }
}

// A session keeps them: saved with every one moved, it comes back so in a new instance; and one saved before them
// loads as it did, the V-Synth's as saved and the VC-2's at their defaults
void sessions(TestChecks& check, const juce::File& rom) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    setParam(p, "patchCoarse", 5.0f);
    moveAll(p);
    juce::MemoryBlock state;
    p.getStateInformation(state);
    VSynthEmuProcessor back;
    back.setStateInformation(state.getData(), (int)state.getSize());
    check(parameterValues(back, true) == parameterValues(p, true), "a session should bring the VC-2's back as saved");
    const juce::MemoryBlock before = sessionBefore(state, kVc2ParamPrefix, nullptr);
    VSynthEmuProcessor old, fresh;
    old.setStateInformation(before.getData(), (int)before.getSize());
    check(parameterValues(old, false) == parameterValues(p, false), "a session from before: the V-Synth's as saved");
    check(parameterValues(old, true) == parameterValues(fresh, true), "and the VC-2's at their defaults");
}

} // namespace

int vc2ParamPluginTest(const juce::File& rom, const juce::File&) {
    TestChecks check { "vc2params" };
    const std::string rom4 = rom.getSiblingFile(kRom4).getFullPathName().toStdString();
    check(xtSynthSetVc2Rom(rom4), "rom.4 should load beside rom.2");
    setAgainstRom4(check);
    VSynthEmuProcessor p;
    eachAgainstRom4(check, p);
    childList(check, rom, false);
    childList(check, rom, true);
    sessions(check, rom);
    return check.summary();
}

int vc2ParamStandIns(const juce::File&, const juce::File&) {
    VSynthEmuProcessor p;
    std::printf("rom.4 %s\n", vc2ParamDesc(0) ? "in" : "out");
    for (auto* base : p.getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(base); r && r->paramID.startsWith(kVc2ParamPrefix))
            std::printf("%s %d %d\n", r->paramID.toRawUTF8(), (int)r->getNormalisableRange().start,
                        (int)r->getNormalisableRange().end);
    return 0;
}
