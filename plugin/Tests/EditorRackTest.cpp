// VSynthEmuEditorTest's checks of D's rack (EditorTest.h; UI step 2)
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/PluginParamIds.h"
#include <functional>
#include <set>

namespace {

using Prefix = std::function<juce::String(int)>;

// Which instance a parameter is, by the processor's own prefixes: the longest one it starts with
int instanceOf(const juce::String& id, const Prefix& prefix, int count) {
    int best = -1, length = -1;
    for (int i = 0; i < count; ++i) {
        const juce::String pre = prefix(i);
        if (id.startsWith(pre) && pre.length() > length) {
            best = i;
            length = pre.length();
        }
    }
    return best;
}

int visibleKnobs(juce::Component& panel) {
    int n = 0;
    for (Knob* k : LayoutCheck::allOf<Knob>(panel)) n += k->isVisible() ? 1 : 0;
    return n;
}

// What one instance's panel is checked against: which instance it is, how its Type is set, how many knobs each
// face's table gives, and the prefixes its kind's parameters take
struct PanelCase {
    FacedPanel* panel;
    int instance, count, faces;
    juce::String typeId;
    std::function<int(int face)> tableKnobs;
    Prefix prefix;
};

// The every-instance pass: each panel, on each face, moved through the real controls; each reaches only its own
void ownParameters(LayoutCheck& l, VSynthEmuProcessor& p, TestChecks& check, const PanelCase& c) {
    auto* type = p.apvts.getParameter(c.typeId);
    const float was = type->convertFrom0to1(type->getValue());
    for (int face = 0; face < c.faces; ++face) {
        setParam(p, c.typeId, (float)face);
        c.panel->refresh();
        const juce::String where = c.typeId + " " + juce::String(face);
        l.check(where);
        check(visibleKnobs(*c.panel) == c.tableKnobs(face),
              (where + ": " + juce::String(visibleKnobs(*c.panel)) + " knobs, the table gives " +
               juce::String(c.tableKnobs(face))).toStdString());
        const std::set<juce::String> before = l.reached;
        l.reached.clear();
        juce::Component* const top = l.root;
        l.root = c.panel;
        l.moveAll(where);
        l.root = top;
        for (const juce::String& id : l.reached)
            check(instanceOf(id, c.prefix, c.count) == c.instance, (where + "'s controls move " + id).toStdString());
        l.reached.insert(before.begin(), before.end());
    }
    setParam(p, c.typeId, was);   // as it was found
    c.panel->refresh();
}

}

void rackChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    ed.showPage(Blocks::kSynthPage);
    const auto oscs = LayoutCheck::allOf<OscPanel>(ed.drawing());
    const auto cosms = LayoutCheck::allOf<CosmPanel>(ed.drawing());
    check(oscs.size() == Blocks::kOscs && cosms.size() == Blocks::kCosms, "the rack hasn't a panel for each instance");
    const Prefix oscPrefix = [](int osc) { return Params::oscId(osc, ""); };
    const Prefix cosmPrefix = [](int cosm) { return Params::cosmId(cosm, ""); };
    for (size_t i = 0; i < oscs.size(); ++i) {
        const int osc = (int)i;
        const juce::String typeId = Params::oscId(osc, Params::kOscType);
        ownParameters(l, p, check, { oscs[i], osc, Blocks::kOscs, OscPanel::kTypes, typeId,
                                     [](int) { return OscPanel::kTableKnobs; }, oscPrefix });
        LayoutCheck::pressAt(*oscs[i], oscs[i]->nameArea().getCentre().toFloat());
        check(ed.selected() == Blocks::indexOf(Blocks::Kind::osc, osc), "a press on an OSC's name doesn't select it");
    }
    for (size_t i = 0; i < cosms.size(); ++i) {
        const int cosm = (int)i;
        ownParameters(l, p, check, { cosms[i], cosm, Blocks::kCosms, CosmPanel::kTypes, Params::cosmId(cosm, "Type"),
                                     CosmPanel::tableKnobs, cosmPrefix });
        LayoutCheck::pressAt(*cosms[i], cosms[i]->nameArea().getCentre().toFloat());
        check(ed.selected() == Blocks::indexOf(Blocks::Kind::cosm, cosm), "a press on a COSM's name doesn't select it");
    }
    ed.select(Blocks::kFirstSelected);
}
