// The editor's tabs: the bar made with the tabs' names, and a tab opened.  Moved from PluginEditor.cpp as they stood (v195),
// with the Zones tab's name before Effects, so every other tab keeps its number and Effects stays kEffectsTab.
#include "PluginEditorTabs.h"

void VSynthEmuEditor::makeTabBar() {
    tabBar_ = std::make_unique<TabBar>();
    tabBar_->names = { "Oscillator", "Envelopes", "LFOs", "COSM", "Amp & Pan", "Play", "MSM", "Matrix", "Zones",
                       "Effects" };
    tabBar_->onChange = [this](int t) { showTab(t); };
    tabBar_->setComponentID("tabs");                 // for PluginTest
    addAndMakeVisible(*tabBar_);
}

void VSynthEmuEditor::showTab(int t) {
    tab_ = juce::jlimit(0, kEffectsTab, t);
    for (int i = 0; i < kPages; ++i) pages_[i]->setVisible(i == tab_);
    fxPanel_->setVisible(tab_ == kEffectsTab);
    tabBar_->current = tab_;
    tabBar_->repaint();
}
