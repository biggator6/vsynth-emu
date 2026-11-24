// The patch box's Save…: see UiPatchSave.h (v602; Save Project As…, v603).
#include "UiPatchSave.h"
#include "PluginProject.h"
#include <vector>

namespace {
const juce::String kProjectExtension = "prj";
constexpr auto kSaveFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;

void write(const PatchSaving& s, const juce::String& name, const juce::File& file) {
    std::string why;
    if (s.save(name, file, &why)) {
        s.saved(file);
        return;
    }
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Not saved",
                                           file.getFileName() + ": " + juce::String(why));
}

void saveOver(const PatchSaving& s) {
    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
        juce::MessageBoxIconType::QuestionIcon, "Save the patch",
        "Replace " + s.own.getFileName() + " with the patch as it is now?", "Replace", "Cancel");
    juce::AlertWindow::showAsync(options, [s](int result) {
        if (result == 1) write(s, s.name, s.own);
    });
}

void saveAs(const PatchSaving& s, std::unique_ptr<juce::FileChooser>& chooser) {
    chooser = std::make_unique<juce::FileChooser>("Save the patch as", nextSlotFile(s.folder, s.name, s.extension),
                                                  "*." + s.extension);
    chooser->launchAsync(kSaveFlags, [s](const juce::FileChooser& fc) {
        const juce::File chosen = fc.getResult();
        if (chosen == juce::File()) return;
        const juce::File file = chosen.withFileExtension(s.extension);
        write(s, patchNameOfFile(file), file);
    });
}

// What a project's save wrote, and the plain audio the XT must import itself
juce::String reportOf(const juce::File& dest, const ProjectSaved& r) {
    juce::String text = dest.getFileName() + ": " + juce::String(r.patches) + " patches, " + juce::String(r.waves)
                        + " waves.";
    if (!r.toImport.isEmpty())
        text << "\n\nNot V-Synth waves, so the XT must import and encode these itself: "
             << r.toImport.joinIntoString(", ");
    return text;
}

void saveProjectAs(const PatchSaving& s, std::unique_ptr<juce::FileChooser>& chooser) {
    const juce::File offered = s.folder.getParentDirectory().getSiblingFile(s.name + "." + kProjectExtension);
    chooser = std::make_unique<juce::FileChooser>("Save the project as", offered, "*." + kProjectExtension);
    chooser->launchAsync(kSaveFlags, [s](const juce::FileChooser& fc) {
        const juce::File chosen = fc.getResult();
        if (chosen == juce::File()) return;
        const juce::File dest = chosen.withFileExtension(kProjectExtension);
        ProjectSaved report;
        std::string why;
        if (!s.saveProject(dest, report, &why)) {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Project not saved",
                                                   dest.getFileName() + ": " + juce::String(why));
            return;
        }
        s.saved(report.patch);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Project saved",
                                               reportOf(dest, report));
    });
}

// One item of the menu: what it says, whether it can be chosen, and what it does
struct SaveChoice {
    juce::String label;
    bool enabled = true;
    std::function<void()> act;
};
} // namespace

void showPatchSaveMenu(juce::Component& target, const PatchSaving& s, std::unique_ptr<juce::FileChooser>& chooser) {
    const bool own = s.own.existsAsFile();
    std::vector<SaveChoice> choices {
        { own ? "Save over " + s.own.getFileName() : juce::String("Save (the patch has no file)"), own,
          [s] { saveOver(s); } },
        { "Save As...", true, [s, &chooser] { saveAs(s, chooser); } },
    };
    if (s.saveProject) choices.push_back({ "Save Project As...", true, [s, &chooser] { saveProjectAs(s, chooser); } });
    juce::PopupMenu menu;
    for (size_t i = 0; i < choices.size(); ++i)
        menu.addItem(static_cast<int>(i) + 1, choices[i].label, choices[i].enabled);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target), [choices](int item) {
        if (item > 0 && static_cast<size_t>(item) <= choices.size()) choices[static_cast<size_t>(item) - 1].act();
    });
}
