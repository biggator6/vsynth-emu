// VSynthEmuEditorTest's checks of D's pages under the page bar but SYNTH (EditorTest.h; UI step 4)
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/FxPanel.h"
#include "../Source/PluginMsm.h"
#include "../Source/PluginParamIds.h"
#include "../Source/UiModPage.h"
#include "../Source/UiZonesPage.h"
#include <algorithm>
#include <climits>

namespace {

constexpr int kEffects = 3, kFxPages = 4;   // MFX, Chorus, Reverb on the path; and System, on the FX page alone
constexpr int kMatrixRowIds = 1 + 2 * Matrix::kDests;
constexpr int kListRows = 15, kParamH = 24;   // the rows the effects' list must have room for, each FxPanel's 24 high
const char* const kTypeOf[kEffects] = { "fxMfxType", "fxChoType", "fxRevType" };   // each effect's Type

// The prefix two IDs share: an instance's, from two of its own parameters
juce::String sharedPrefix(const juce::String& a, const juce::String& b) {
    int n = 0;
    while (n < a.length() && n < b.length() && a[n] == b[n]) ++n;
    return a.substring(0, n);
}

juce::String trackPrefix(int t) {
    return sharedPrefix(msmParamId(msmTrackFwId(t, 0)), msmParamId(msmTrackFwId(t, 1)));
}

// A page as the kind it should be, shown; none, and said, if it isn't
template <class T>
T* shownPage(SynthEditor& ed, int page, TestChecks& check) {
    auto* view = dynamic_cast<T*>(ed.pageView(page));
    check(view != nullptr, std::string(Blocks::kPages[(size_t)page]) + " isn't the page it should be");
    if (view != nullptr) ed.showPage(page);
    return view;
}

// The every-instance passes of MODULATION: each MSM track, each Matrix Control, their controls all made and moving
// their own parameters alone; the assigns all made
void modulation(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    auto* page = shownPage<ModulationPage>(ed, Blocks::kModulationPage, check);
    if (page == nullptr) return;
    for (int t = 0; t < MsmView::kTracks; ++t) {
        page->msm().showTrack(t);
        const juce::String where = "MSM track " + juce::String(t + 1);
        check(page->msm().track(t).made() == MsmTrack::kSettings + MsmTrack::kSteps,
              (where + " misses a control").toStdString());
        l.check(where);
        movesOwn(l, page->msm().track(t), where, { trackPrefix, MsmView::kTracks, t }, check);
    }
    page->msm().showTrack(0);
    const auto control = [](int m) { return VSynthEmuProcessor::matrixParamId(m, ""); };
    for (int m = 0; m < Matrix::kControls; ++m) {
        const juce::String where = "Matrix Control " + juce::String(m + 1);
        check(page->matrix().row(m).made() == kMatrixRowIds, (where + " misses a control").toStdString());
        movesOwn(l, page->matrix().row(m), where, { control, Matrix::kControls, m }, check);
    }
    check(page->assigns().made() == VSynthEmuProcessor::kAssigns, "an assign has no control");
}

// Each zone's cell moving its own split alone
void zones(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    auto* page = shownPage<ZonesPage>(ed, Blocks::kZonesPage, check);
    if (page == nullptr) return;
    const auto zone = [](int z) { return Params::zoneUpperId(z); };
    for (int z = 0; z < kXtZones; ++z)
        movesOwn(l, page->cell(z), "zone " + juce::String(z + 1), { zone, kXtZones, z }, check);
}

// A page's left cards start level with its list, a block's card in the first one's place (sameAs): a card left in its
// own place sits under an empty one's height (v196; the tabbed editor's shorter page had pushed it off the page)
void cardsLevel(const FxPanel& fx, int page, TestChecks& check) {
    int left = INT_MAX, right = INT_MAX;
    for (const Ui::Page::Card& c : fx.cards) {
        if (c.bare || c.col < 0 || (c.shown && !c.shown())) continue;
        int& top = c.col == 0 ? left : right;
        top = std::min(top, c.bounds.getY());
    }
    check(left == right, "FX page " + std::to_string(page) + ": its left cards start at " + std::to_string(left) +
                             ", its list at " + std::to_string(right));
}

// A page of the FX page on each of its block's types, as what's on it follows them (v196, as TEST_UI_LAYOUT had it of
// the tabbed editor's Effects tab): laid out, the list's names and values whole, and room for 15 of its rows
void effectTypes(LayoutCheck& l, FxPanel& fx, int page, TestChecks& check) {
    const int types = page < kEffects ? l.p.apvts.getParameter(kTypeOf[page])->getNumSteps() : 1;
    for (int type = 0; type < types; ++type) {
        if (page < kEffects) setParam(l.p, kTypeOf[page], (float)type);
        fx.showPage(page);
        const juce::String where = "FX page " + juce::String(page) + " type " + juce::String(type);
        l.check(where);
        l.checkList(where);
        check(l.view->getHeight() >= kListRows * kParamH, (where + ": the list shows too few rows").toStdString());
    }
    if (page < kEffects) setParam(l.p, kTypeOf[page], 0.0f);
}

// The page's buttons open their pages, and follow a page opened another way
void pageButtons(LayoutCheck& l, FxPanel& fx, TestChecks& check) {
    juce::Button* chorus = l.buttonOf("Chorus");
    juce::Button* system = l.buttonOf("System");
    check(chorus != nullptr && system != nullptr, "no page buttons on the FX page");
    if (chorus == nullptr || system == nullptr) return;
    chorus->setToggleState(true, juce::sendNotificationSync);
    check(fx.page() == 1, "the FX page's Chorus button doesn't open its page");
    fx.showPage(kFxPages - 1);
    check(system->getToggleState(), "the FX page's System button doesn't follow its page");
}

// The FX page's four pages, each on every type, laid out and its controls moved; its buttons; an effect pressed on the
// path opens its own
void effects(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    auto* fx = shownPage<FxPanel>(ed, Blocks::kFxPage, check);
    if (fx == nullptr) return;
    check(l.view != nullptr, "the FX page has no list");
    for (int page = 0; page < kFxPages && l.view != nullptr; ++page) {
        effectTypes(l, *fx, page, check);
        fx->showPage(page);
        cardsLevel(*fx, page, check);
        movesOwn(l, *fx, "FX page " + juce::String(page), {}, check);
    }
    pageButtons(l, *fx, check);
    ed.showPage(Blocks::kSynthPage);
    PathBar* path = LayoutCheck::allOf<PathBar>(ed.drawing()).front();
    const PathBar::Chips chips = path->laidOut();
    for (int e = 0; e < kEffects && e < (int)chips.effects.size(); ++e) {
        LayoutCheck::pressAt(*path, chips.effects[(size_t)e].r.getCentre().toFloat());
        check(ed.page() == Blocks::kFxPage && fx->page() == e,
              ("a press on " + chips.effects[(size_t)e].name + " doesn't open its page").toStdString());
        ed.showPage(Blocks::kSynthPage);
    }
    fx->showPage(0);
}

}

void pagesChecks(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    modulation(l, ed, check);
    zones(l, ed, check);
    effects(l, ed, check);
    ed.showPage(Blocks::kSynthPage);
}
