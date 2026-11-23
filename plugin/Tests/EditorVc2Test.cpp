// VSynthEmuEditorTest's checks of the VC-2 mode (EditorTest.h; v453; research/UI_DESIGN_VC2.md, "7. What building it
// takes"): the header's switch and the session's mode; each page and each block at three sizes; for each zone, chosen by
// the path's chips, each OSC's and COSM's panel on every face, each block's SELECTED and DETAILS on every face, the Time
// Trip column and AMP moving their own instance's parameters alone, the zone's and the patch's; each Matrix Control and
// zone cell its own.
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/PluginVc2Params.h"
#include "../Source/UiModPage.h"
#include "../Source/UiVc2Kinds.h"
#include "../Source/UiVc2Text.h"
#include "../Source/UiZonesPage.h"
#include <set>

namespace {

using Blocks::Kind;
constexpr int kCurrentZone = 0x140, kMatrixFirst = 0x16a, kMatrixLast = 0x191, kMatrixStride = 5, kFirstUpper = 0x141;
constexpr int kOscType = 0x2e5, kCosmType = 0x463, kOscKnobs = OscPanel::kBig + OscPanel::kSmall;

// A VC-2 host ID read back: its rom.4 ID, zone and COSM, and whether it is a zone's (its zone 2 twin is in the set)
struct HostId {
    int rom = -1, zone = 0, cosm = 0;
    bool zoned = false;
};
HostId hostIdOf(const juce::String& id, const std::set<juce::String>& set) {
    HostId h;
    h.rom = Vc2Text::romIdOf(id);
    if (h.rom < 0) return h;
    const juce::String head = id.upToFirstOccurrenceOf("_", false, false).substring(kVc2ParamPrefix.length());
    h.zone = head.contains("z2") ? 1 : 0;
    h.cosm = head.contains("c2") ? 1 : 0;
    h.zoned = set.count(vc2HostParamId(h.rom, 1, h.cosm)) != 0;
    return h;
}

// A zone's instance of a kind as an owner: its own parameters are the zone's of that instance; the patch's and the
// system's (Pad Mode and Hold, the Matrix's) every instance's; another zone's, or another instance's, none of its own
Owner vc2Owner(Kind kind, int index, int zone, const std::set<juce::String>& set) {
    const auto instance = [kind, index, zone, &set](const juce::String& id) {
        const HostId h = hostIdOf(id, set);
        if (h.rom < 0) return -1;
        if (!h.zoned) return index;
        if (h.zone != zone) return -1;
        return kind == Kind::mod || kind == Kind::amp ? 0 : Vc2Blocks::whichInstance(kind, h.rom, h.cosm);
    };
    return { {}, 2, index, instance };
}

// Every one of a page's controls of a run of IDs, an instance `stride` apart (a Matrix Control's, a zone's split)
Owner runOwner(int first, int last, int stride, int index, int count) {
    const auto instance = [first, last, stride](const juce::String& id) {
        const int rom = Vc2Text::romIdOf(id);
        return rom >= first && rom <= last ? (rom - first) / stride : -1;
    };
    return { {}, count, index, instance };
}

// Each OSC panel on each of its four faces, each COSM panel on each of its seventeen: its table's knobs, its own
void rack(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check, int zone,
          const std::set<juce::String>& set) {
    Vc2SynthPage& page = ed.vc2Synth();
    const auto oscs = LayoutCheck::allOf<OscPanel>(page);
    const auto cosms = LayoutCheck::allOf<CosmPanel>(page);
    check(oscs.size() == Blocks::kOscs && cosms.size() == Blocks::kCosms, "the VC-2 rack hasn't a panel an instance");
    const juce::String z = "zone " + juce::String(zone + 1) + " ";
    for (size_t i = 0; i < oscs.size(); ++i) {
        const juce::String typeId = vc2HostParamId(Vc2Blocks::instanceId(Kind::osc, (int)i, kOscType), zone, 0);
        eachFace(p, typeId, oscs[i]->faces(), [&](int face) {
            page.refresh();
            const juce::String where = z + "OSC " + juce::String((int)i + 1) + " face " + juce::String(face);
            l.check(where);
            check(visibleKnobs(*oscs[i]) == kOscKnobs, (where + " misses a knob").toStdString());
            movesOwn(l, *oscs[i], where, vc2Owner(Kind::osc, (int)i, zone, set), check);
        });
    }
    for (size_t c = 0; c < cosms.size(); ++c) {
        eachFace(p, vc2HostParamId(kCosmType, zone, (int)c), cosms[c]->faces(), [&](int face) {
            page.refresh();
            const juce::String where = z + "COSM " + juce::String((int)c + 1) + " face " + juce::String(face);
            l.check(where);
            check(visibleKnobs(*cosms[c]) == cosms[c]->knobsOf(face), (where + " misses a knob").toStdString());
            movesOwn(l, *cosms[c], where, vc2Owner(Kind::cosm, (int)c, zone, set), check);
        });
    }
    for (TripColumn* trip : LayoutCheck::allOf<TripColumn>(page))
        for (int osc = 0; osc < TripColumn::kOscs; ++osc) {
            trip->follow(osc);
            movesOwn(l, *trip, z + "the pad following OSC " + juce::String(osc + 1),
                     vc2Owner(Kind::osc, osc, zone, set), check);
        }
    for (AmpPanel* amp : LayoutCheck::allOf<AmpPanel>(page))
        movesOwn(l, *amp, z + "AMP", vc2Owner(Kind::amp, 0, zone, set), check);
}

// Each block's SELECTED (each envelope's tab, its LFO, what reaches it) and DETAILS on every face, its own
void lower(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check, int zone,
           const std::set<juce::String>& set) {
    const Blocks::Path path = Vc2Blocks::path(zone);
    for (int block = 0; block < Blocks::kCount; ++block) {
        ed.select(block);
        LowerPanel& panel = ed.vc2Synth().lower();
        const Blocks::Block& b = path[(size_t)block];
        const Owner own = vc2Owner(b.kind, b.index, zone, set);
        const juce::String where = "zone " + juce::String(zone + 1) + " " + Blocks::blockName(b);
        panel.showView(LowerPanel::View::selected);
        if (EnvelopeView* env = panel.envelopes(block))
            for (int e = 0; e < env->count(); ++e) {
                env->showEnvelope(e);
                movesOwn(l, *env, where + " envelope " + juce::String(e), own, check);
            }
        for (juce::Component* c : { (juce::Component*)panel.lfo(block), (juce::Component*)panel.routes(block) })
            if (c != nullptr) movesOwn(l, *c, where + " SELECTED", own, check);
        panel.showView(LowerPanel::View::details);
        DetailsView& details = panel.details(block);
        eachFace(p, Blocks::typeIdOf(b), details.faces(), [&](int face) {
            details.refresh();
            const juce::String at = where + " DETAILS " + juce::String(face);
            l.check(at);
            check(details.made(face) == DetailsView::rows(b, face), (at + " misses a control").toStdString());
            movesOwn(l, details, at, own, check);
        });
        panel.showView(LowerPanel::View::selected);
    }
}

// The other pages: each laid out and moved; each Matrix Control and each zone's split its own
void pages(LayoutCheck& l, SynthEditor& ed, TestChecks& check, const std::function<void()>& sizes) {
    for (int page = 1; page < (int)Blocks::kVc2Pages.size(); ++page) {
        ed.showPage(page);
        const juce::String name = juce::String("VC-2 ") + Blocks::kVc2Pages[(size_t)page];
        check(ed.pageView(page) != nullptr && ed.pageView(page)->isVisible(), (name + " doesn't show").toStdString());
        sizes();
        l.check(name);
        l.moveAll(name);
    }
    if (auto* mod = dynamic_cast<ModulationPage*>(ed.pageView(Blocks::kVc2ModulationPage))) {
        check(!mod->hasMsm(), "the VC-2's MODULATION has a Multi Step Modulator");
        for (int m = 0; m < Matrix::kControls; ++m)
            movesOwn(l, mod->matrix().row(m), "VC-2 Matrix Control " + juce::String(m + 1),
                     runOwner(kMatrixFirst, kMatrixLast, kMatrixStride, m, Matrix::kControls), check);
        check(mod->assigns().made() == 15, "the VC-2's assigns and Beam Type should be 15 controls");
    }
    if (auto* zones = dynamic_cast<ZonesPage*>(ed.pageView(Blocks::kVc2ZonesPage))) {
        ed.showPage(Blocks::kVc2ZonesPage);
        for (int z = 0; z < kXtZones; ++z)
            movesOwn(l, zones->cell(z), "VC-2 zone cell " + juce::String(z + 1),
                     runOwner(kFirstUpper, kFirstUpper + kXtZones - 1, 1, z, kXtZones), check);
    }
}

}

void vc2Checks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check,
               const std::function<void()>& sizes) {
    juce::Button* vc2 = l.buttonOf("VC-2");
    check(vc2 != nullptr && vc2->isEnabled(), "the header's VC-2 button isn't there to press");
    if (vc2 != nullptr) vc2->onClick();
    check(ed.vc2() && pluginMode(p.apvts.state) == PluginMode::vc2, "a press on VC-2 doesn't choose the mode");
    PageBar* bar = l.firstOf<PageBar>([](PageBar&) { return true; });
    juce::StringArray names;
    for (const char* name : Blocks::kVc2Pages) names.add(name);
    check(bar != nullptr && bar->names() == names, "the page bar doesn't name the VC-2's pages");
    juce::MemoryBlock saved;
    p.getStateInformation(saved);
    p.setStateInformation(saved.getData(), (int)saved.getSize());
    check(pluginMode(p.apvts.state) == PluginMode::vc2, "the session doesn't keep the VC-2 mode");
    std::set<juce::String> set;
    for (const Vc2HostParam& h : vc2HostParams()) set.insert(h.paramId);
    const int was = Controls::paramValue(p.apvts, vc2HostParamId(kCurrentZone, 0, 0));
    for (int zone = 0; zone < Vc2Blocks::kZones; ++zone) {
        ed.showPage(Blocks::kSynthPage);
        PathBar& path = ed.vc2Synth().path();   // the zone's chip pressed: Current Zone, then the page made again
        const auto chips = path.laidOut().zones;
        check(chips.size() == (size_t)Vc2Blocks::kZones, "the VC-2 path hasn't a chip a zone");
        if (chips.size() > (size_t)zone) LayoutCheck::pressAt(path, chips[(size_t)zone].r.getCentre().toFloat());
        ed.vc2Synth().refresh();
        const bool chosen = Controls::paramValue(p.apvts, vc2HostParamId(kCurrentZone, 0, 0)) == zone;
        check(chosen && ed.vc2Synth().zone() == zone, "a press on a zone's chip doesn't choose it");
        if (chosen && zone > 0) l.reached.insert(vc2HostParamId(kCurrentZone, 0, 0));   // the chips are its control
        for (int block = 0; block < Blocks::kCount; ++block) {
            ed.select(block);
            sizes();
        }
        rack(l, ed, p, check, zone, set);
        lower(l, ed, p, check, zone, set);
        l.moveAll("VC-2 SYNTH, zone " + juce::String(zone + 1));   // the path's Structure besides
    }
    setParam(p, vc2HostParamId(kCurrentZone, 0, 0), (float)was);
    ed.vc2Synth().refresh();
    pages(l, ed, check, sizes);
    ed.showPage(Blocks::kSynthPage);
}
