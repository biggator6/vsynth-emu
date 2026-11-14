// VSynthEmuEditorTest's checks of D's lower panel (EditorTest.h; UI step 3)
#include "EditorTest.h"
#include "TestControls.h"

namespace {

using View = LowerPanel::View;
constexpr int kViews = 4;
constexpr int kExampleControl = 0;   // example: the Matrix Control a route is made on (any would do)

float valueOf(VSynthEmuProcessor& p, const juce::String& id) {
    return p.apvts.getRawParameterValue(id)->load();
}

// The every-instance pass: each block's SELECTED on each envelope's tab and its DETAILS on each face of its Type, laid
// out, each row of its tables made, its controls moving its own instance's parameters alone
void eachBlock(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    LowerPanel& lower = ed.lower();
    for (int block = 0; block < Blocks::kCount; ++block) {
        const Blocks::Block& b = Blocks::kPath[(size_t)block];
        const juce::String name = Blocks::blockName(b);
        const Owner own = ownerOf(b);
        ed.select(block);
        lower.showView(View::selected);
        l.check(name + " SELECTED");
        EnvelopeView* env = lower.envelopes(block);
        check((env != nullptr ? env->count() : 0) == Blocks::envelopesOf(b),
              (name + " hasn't a tab for each envelope").toStdString());
        for (int e = 0; env != nullptr && e < env->count(); ++e) {
            env->showEnvelope(e);
            const juce::String where = name + " SELECTED, envelope " + juce::String(e);
            check(env->curve(e).complete(), (where + ": its curve misses a parameter").toStdString());
            l.check(where);
            movesOwn(l, lower, where, own, check);
        }
        if (LfoView* lfo = lower.lfo(block))
            check(lfo->shape().complete(), (name + "'s LFO misses a parameter").toStdString());
        lower.showView(View::details);
        DetailsView& d = lower.details(block);
        check(d.faces() == Blocks::kindOf(b).faces, (name + "'s DETAILS hasn't a face for each Type").toStdString());
        eachFace(p, Blocks::typeIdOf(b), d.faces(), [&](int face) {
            d.refresh();
            const juce::String where = name + " DETAILS, face " + juce::String(face);
            check(d.made(face) == DetailsView::rows(b, face),
                  (where + ": a row of its table names no parameter").toStdString());
            l.check(where);
            movesOwn(l, lower, where, own, check);
        });
        d.refresh();
    }
}

// The template once (the first selected block's first envelope): each point dragged sets its parameters
void curveDrag(SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    const Blocks::Block& b = Blocks::kPath[(size_t)Blocks::kFirstSelected];
    ed.select(Blocks::kFirstSelected);
    ed.lower().showView(View::selected);
    EnvelopeView* env = ed.lower().envelopes(Blocks::kFirstSelected);
    check(env != nullptr, "the first selected block has no envelopes");
    if (env == nullptr) return;
    env->showEnvelope(0);
    EnvCurve& curve = env->curve(0);
    const juce::String prefix = Blocks::kindOf(b).envs.front().prefix;
    const auto id = [&](const char* name) { return Blocks::blockParam(b, prefix + name); };
    const juce::String ids[] = { id("Attack"), id("Decay"), id("Sustain"), id("Release") };
    std::vector<float> was;
    for (const juce::String& i : ids) was.push_back(valueOf(p, i));
    for (const juce::String& i : ids) setParam(p, i, 64.0f);   // each point apart from the others, each free to move
    using Point = EnvCurve::Point;
    for (Point at : { Point::attack, Point::release }) {
        const juce::String moved = ids[at == Point::attack ? 0 : 3];
        const float before = valueOf(p, moved);
        LayoutCheck::dragFrom(curve, curve.pointAt(at), curve.pointAt(at).translated(24.0f, 0.0f));
        check(valueOf(p, moved) > before, ("a drag right of its point doesn't lengthen " + moved).toStdString());
    }
    const float sustain = valueOf(p, ids[2]);
    LayoutCheck::dragFrom(curve, curve.pointAt(Point::decay), curve.pointAt(Point::decay).translated(0.0f, -20.0f));
    check(valueOf(p, ids[2]) > sustain, "a drag up of the decay's point doesn't raise Sustain");
    // Decay 0 and Sustain full: the decay's point on the attack's, and a drag there draws the decay's off it
    setParam(p, ids[1], 0.0f);
    auto* top = p.apvts.getParameter(ids[2]);
    setParam(p, ids[2], top->convertFrom0to1(1.0f));
    const float attack = valueOf(p, ids[0]);
    LayoutCheck::dragFrom(curve, curve.pointAt(Point::attack), curve.pointAt(Point::attack).translated(24.0f, 0.0f));
    check(valueOf(p, ids[1]) > 0.0f && juce::exactlyEqual(valueOf(p, ids[0]), attack),
          "a drag from points on one another doesn't draw the later off");
    for (size_t i = 0; i < was.size(); ++i) setParam(p, ids[i], was[i]);   // as they were found
}

// Every Matrix destination lands on one block at most, none but OFF and the MFX's; a route made on the selected block
// appears in what reaches it, its amount moves its Sens, and it goes when its destination does
void routes(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    const juce::StringArray dests = VSynthEmuProcessor::matrixDestNames();
    int to = -1;
    for (int d = 0; d < dests.size(); ++d) {
        int lands = 0;
        for (const Blocks::Block& b : Blocks::kPath) lands += Blocks::reaches(b, dests[d]) ? 1 : 0;
        const bool none = dests[d] == "OFF" || dests[d].startsWith("MFX-PRM");
        check(lands == (none ? 0 : 1), (dests[d] + " lands on " + juce::String(lands) + " blocks").toStdString());
        if (to < 0 && Blocks::reaches(Blocks::kPath[(size_t)Blocks::kFirstSelected], dests[d])) to = d;
    }
    ed.select(Blocks::kFirstSelected);
    ed.lower().showView(View::selected);
    RoutesView* view = ed.lower().routes(Blocks::kFirstSelected);
    check(view != nullptr && to > 0, "the first selected block has no routes or no destination");
    if (view == nullptr || to <= 0) return;
    const juce::String source = VSynthEmuProcessor::matrixParamId(kExampleControl, "Source");
    const juce::String dest = VSynthEmuProcessor::matrixParamId(kExampleControl, "Dest1");
    setParam(p, source, 1.0f);
    setParam(p, dest, (float)to);
    ed.lower().refresh();
    check(view->routes() == 1, ("a route to " + dests[to] + " isn't shown").toStdString());
    if (view->routes() == 1) {
        juce::Slider& amount = view->route(0)->slider();
        auto* moved = l.changedBy([&] { amount.setValue(amount.getMaximum(), juce::sendNotificationSync); });
        check(moved != nullptr && moved->paramID == VSynthEmuProcessor::matrixParamId(kExampleControl, "Sens1"),
              "a route's amount doesn't move its Sens");
        l.check("a route to " + dests[to]);
    }
    setParam(p, dest, 0.0f);
    setParam(p, source, 0.0f);
    ed.lower().refresh();
    check(view->routes() == 0, "a route taken away is still shown");
}

// A press on each entry of the list shows its view; a press on an overview's last tile opens its block, at its envelope
void presses(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    LowerPanel& lower = ed.lower();
    ed.select(Blocks::kFirstSelected);
    const auto entries = lower.list().tabs();
    check((int)entries.size() == kViews, "the list hasn't an entry for each view");
    for (size_t v = 0; v < entries.size(); ++v) {
        LayoutCheck::pressAt(lower.list(), entries[v].getCentre().toFloat());
        check(lower.view() == (View)v, (lower.list().names()[(int)v] + " isn't shown by a press").toStdString());
        l.check(lower.list().names()[(int)v]);
    }
    int envelopes = 0, lfos = 0;
    for (const Blocks::Block& b : Blocks::kPath) {
        envelopes += Blocks::envelopesOf(b);
        lfos += Blocks::kindOf(b).lfo != nullptr ? 1 : 0;
    }
    check(lower.allEnvelopes().tiles() == envelopes && lower.allLfos().tiles() == lfos,
          "ENVELOPES or LFOs hasn't a tile for each one");
    for (Overview* o : { &lower.allEnvelopes(), &lower.allLfos() }) {
        lower.showView(o == &lower.allEnvelopes() ? View::envelopes : View::lfos);
        const int last = o->tiles() - 1;
        LayoutCheck::pressAt(o->drawing(last), o->drawing(last).getLocalBounds().getCentre().toFloat());
        EnvelopeView* env = lower.envelopes(o->blockOf(last));
        check(ed.selected() == o->blockOf(last) && lower.view() == View::selected
                  && (env == nullptr || env->shown() == o->envOf(last)),
              "a press on an overview's tile doesn't open its block at it");
    }
    ed.select(Blocks::kFirstSelected);
    lower.showView(View::selected);
}

}

void lowerChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    ed.showPage(Blocks::kSynthPage);
    eachBlock(l, ed, p, check);
    curveDrag(ed, p, check);
    routes(l, ed, p, check);
    presses(l, ed, check);
}
