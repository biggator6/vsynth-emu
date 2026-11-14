// VSynthEmuEditorTest's checks of D's pages under the page bar but SYNTH (EditorTest.h; UI step 4)
#include "EditorTest.h"
#include "../Source/FxPanel.h"
#include "../Source/PluginMsm.h"
#include "../Source/PluginParamIds.h"
#include "../Source/UiModPage.h"
#include "../Source/UiZonesPage.h"

namespace {

constexpr int kEffects = 3, kFxPages = 4;   // MFX, Chorus, Reverb on the path; and System, on the FX page alone
constexpr int kMatrixRowIds = 1 + 2 * Matrix::kDests;

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

// The FX page's four pages, each laid out and its controls moved; an effect pressed on the path opens its own
void effects(LayoutCheck& l, SynthEditor& ed, TestChecks& check) {
    auto* fx = shownPage<FxPanel>(ed, Blocks::kFxPage, check);
    if (fx == nullptr) return;
    for (int page = 0; page < kFxPages; ++page) {
        fx->showPage(page);
        const juce::String where = "FX page " + juce::String(page);
        l.check(where);
        movesOwn(l, *fx, where, {}, check);
    }
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
