#pragma once
// D's patch box (research/UI_DESIGN.md, "The patch box"; moved out of the header as it stood in v601, the header laying
// it out): the patch's name, under it what the patch asks for that isn't here, Browse… to load a patch or (v601) a
// project; and the patch's project to browse (v601, the user's choice of 2026-11-24): ◀ and ▶ load the previous and next
// of its patches, a click on the name opens a menu of them all, the project's name over them; and (v602) Save…, Save over
// its file or Save As (UiPatchSave.h).  Each mode's patches are a row of one table (kPatchKinds: its file type, its
// load, its name and note, where the session keeps the patch browsed, its save: none for the VC-2's).
#include "PluginMode.h"
#include "PluginProcessor.h"
#include "PluginProject.h"
#include <memory>

class PatchBox : public juce::Component {
public:
    explicit PatchBox(VSynthEmuProcessor& p);
    void setMode(PluginMode mode);
    // The name, the note, the project and the arrows as they are now (the header's refresh, the editor's timer)
    void refresh();
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;   // on the name: the project's menu

private:
    VSynthEmuProcessor& processor_;
    PluginMode mode_ = PluginMode::vsynth;
    juce::Label patchName_, patchNote_, projectLine_;
    bool noted_ = false, listed_ = false;   // a note under the name, a project to browse: as it was laid out
    juce::TextButton browse_ { "Browse..." }, save_ { "Save..." };
    juce::ArrowButton prev_, next_;
    std::unique_ptr<juce::FileChooser> chooser_;
    juce::Rectangle<int> menuMark_;   // the name's menu mark, drawn
    juce::File browsed_;              // the patch the session says was browsed, as last followed
    ProjectPatches project_;          // and its project
    void followBrowsed();
    void load(const juce::File& patch);
    void step(int by);
    void showMenu();
    void choose();
    void save();   // Save… (v602)
};
