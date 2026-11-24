// The patch box's Save…: see UiPatchSave.h (v602).
#include "UiPatchSave.h"
#include <algorithm>

namespace {
constexpr int kSlotDigits = 3;   // the XT's "NNN name" files
enum MenuItem { kSaveOver = 1, kSaveAs };
constexpr auto kSaveFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting;

// The slot a file's name starts with ("002 …" gives 2), 0 for none
int fileSlot(const juce::File& f) {
    const juce::String n = f.getFileNameWithoutExtension();
    const juce::String digits = n.initialSectionContainingOnly("0123456789");
    return digits.length() == kSlotDigits && n[kSlotDigits] == ' ' ? digits.getIntValue() : 0;
}

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
} // namespace

void showPatchSaveMenu(juce::Component& target, const PatchSaving& s, std::unique_ptr<juce::FileChooser>& chooser) {
    juce::PopupMenu menu;
    const bool own = s.own.existsAsFile();
    menu.addItem(kSaveOver, own ? "Save over " + s.own.getFileName() : juce::String("Save (the patch has no file)"),
                 own);
    menu.addItem(kSaveAs, "Save As...");
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&target), [s, &chooser](int item) {
        if (item == kSaveOver) saveOver(s);
        if (item == kSaveAs) saveAs(s, chooser);
    });
}

juce::File nextSlotFile(const juce::File& folder, const juce::String& name, const juce::String& extension) {
    juce::Array<juce::File> files;
    if (folder.isDirectory())
        folder.findChildFiles(files, juce::File::findFiles | juce::File::ignoreHiddenFiles, false, "*." + extension);
    int last = 0;
    for (const juce::File& f : files) last = std::max(last, fileSlot(f));
    return folder.getChildFile(juce::String(last + 1).paddedLeft('0', kSlotDigits) + " " + name + "." + extension);
}

juce::String patchNameOfFile(const juce::File& file) {
    const juce::String n = file.getFileNameWithoutExtension();
    return fileSlot(file) ? n.substring(kSlotDigits + 1) : n;
}
