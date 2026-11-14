// VSynthEmuEditorTest's checks of D's rack, and what the lower panel's share (EditorTest.h; UI steps 2 and 3)
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/PluginParamIds.h"
#include <set>

namespace {

// Which instance a parameter is, by the processor's own prefixes: the longest one it starts with
int instanceOf(const juce::String& id, const Owner& own) {
    int best = -1, length = -1;
    for (int i = 0; i < own.count; ++i) {
        const juce::String pre = own.prefix(i);
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

// What one instance's panel is checked against: how its Type is set, how many knobs each face's table gives, and
// which instance it is
struct PanelCase {
    FacedPanel* panel;
    int faces;
    juce::String typeId;
    std::function<int(int face)> tableKnobs;
    Owner own;
};

// The every-instance pass: each panel, on each face, moved through the real controls; each reaches only its own
void ownParameters(LayoutCheck& l, VSynthEmuProcessor& p, TestChecks& check, const PanelCase& c) {
    eachFace(p, c.typeId, c.faces, [&](int face) {
        c.panel->refresh();
        const juce::String where = c.typeId + " " + juce::String(face);
        l.check(where);
        check(visibleKnobs(*c.panel) == c.tableKnobs(face),
              (where + ": " + juce::String(visibleKnobs(*c.panel)) + " knobs, the table gives " +
               juce::String(c.tableKnobs(face))).toStdString());
        movesOwn(l, *c.panel, where, c.own, check);
    });
    c.panel->refresh();
}

// The pad's column following each OSC in turn: that OSC's Time Trip switch and Robot each reached by its controls
void tripColumn(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    const auto columns = LayoutCheck::allOf<TripColumn>(ed.drawing());
    check(columns.size() == 1, "the rack hasn't one Time Trip column");
    if (columns.empty()) return;
    TripColumn& column = *columns.front();
    for (int osc = 0; osc < TripColumn::kOscs; ++osc) {
        column.follow(osc);
        const juce::String where = "the pad following OSC " + juce::String(osc + 1);
        movesOwn(l, column, where, {}, check);
        for (const juce::String& id : { Params::oscId(osc, Params::kTripSw), Params::oscId(osc, Params::kRobot) })
            check(l.reached.count(id) == 1, (where + ": nothing moves " + id).toStdString());
    }
    column.follow(0);
}

}

Owner ownerOf(const Blocks::Block& b) {
    if (!Blocks::kindOf(b).numbered) return {};
    int count = 0;
    for (const Blocks::Block& on : Blocks::kPath) count += on.kind == b.kind ? 1 : 0;
    const Blocks::Kind kind = b.kind;
    return { [kind](int i) { return Blocks::blockParam({ kind, i }, ""); }, count, b.index };
}

void movesOwn(LayoutCheck& l, juce::Component& root, const juce::String& where, const Owner& own, TestChecks& check) {
    const std::set<juce::String> before = l.reached;
    l.reached.clear();
    juce::Component* const top = l.root;
    l.root = &root;
    l.moveAll(where);
    l.root = top;
    for (const juce::String& id : l.reached)
        if (own.count > 0) check(instanceOf(id, own) == own.index, (where + "'s controls move " + id).toStdString());
    l.reached.insert(before.begin(), before.end());
}

void eachFace(VSynthEmuProcessor& p, const juce::String& typeId, int faces, const std::function<void(int)>& at) {
    auto* type = typeId.isEmpty() ? nullptr : p.apvts.getParameter(typeId);
    const float was = type != nullptr ? type->convertFrom0to1(type->getValue()) : 0.0f;
    for (int face = 0; face < faces; ++face) {
        if (type != nullptr) setParam(p, typeId, (float)face);
        at(face);
    }
    if (type != nullptr) setParam(p, typeId, was);   // as it was found
}

void rackChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    ed.showPage(Blocks::kSynthPage);
    const auto oscs = LayoutCheck::allOf<OscPanel>(ed.drawing());
    const auto cosms = LayoutCheck::allOf<CosmPanel>(ed.drawing());
    check(oscs.size() == Blocks::kOscs && cosms.size() == Blocks::kCosms, "the rack hasn't a panel for each instance");
    for (size_t i = 0; i < oscs.size(); ++i) {
        const int osc = (int)i;
        const juce::String typeId = Params::oscId(osc, Params::kOscType);
        ownParameters(l, p, check, { oscs[i], OscPanel::kTypes, typeId, [](int) { return OscPanel::kTableKnobs; },
                                     ownerOf({ Blocks::Kind::osc, osc }) });
        LayoutCheck::pressAt(*oscs[i], oscs[i]->nameArea().getCentre().toFloat());
        check(ed.selected() == Blocks::indexOf(Blocks::Kind::osc, osc), "a press on an OSC's name doesn't select it");
    }
    for (size_t i = 0; i < cosms.size(); ++i) {
        const int cosm = (int)i;
        ownParameters(l, p, check, { cosms[i], CosmPanel::kTypes, Params::cosmId(cosm, "Type"), CosmPanel::tableKnobs,
                                     ownerOf({ Blocks::Kind::cosm, cosm }) });
        LayoutCheck::pressAt(*cosms[i], cosms[i]->nameArea().getCentre().toFloat());
        check(ed.selected() == Blocks::indexOf(Blocks::Kind::cosm, cosm), "a press on a COSM's name doesn't select it");
    }
    tripColumn(l, ed, check);
    ed.select(Blocks::kFirstSelected);
}
