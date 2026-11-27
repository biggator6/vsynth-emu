// D's patch box: see UiPatchBox.h (v601).
#include "UiPatchBox.h"
#include "PluginPatchInit.h"
#include "PluginPatchSave.h"
#include "PluginVc2PatchInit.h"
#include "Vc2PatchInit.h"
#include "UiLook.h"
#include "UiParts.h"
#include "UiPatchSave.h"
#include <array>

namespace {
// Where each part sits, left to right, in the drawing's units (UI_DESIGN.md, "The patch box")
constexpr int kBrowseW = 76, kPatchTagW = 56, kArrowW = 18, kArrowH = 14, kButtonH = 26, kNoteH = 11, kMarkW = 10;
constexpr int kSaveW = 56, kInitW = 44;
constexpr float kMark = 3.5f;   // the menu mark's half width
constexpr const char* kInitTitle = "INITIALIZE PATCH";   // [FW] the XT's own words over its PATCH Init's EXECUTE
constexpr auto kOpen = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                       | juce::FileBrowserComponent::canSelectDirectories;

// A mode's patches: what Browse… asks for, their files' extension, where the session keeps the one browsed, the load,
// the name and note shown, the save (v602) and the project's (v603): none for the VC-2's, whose patches rom.4 writes;
// the PATCH Init (v473) by the choice its menu offers, and those choices
struct PatchKind {
    const char* title;
    const char* extension;
    const char* fileProperty;
    bool (*load)(VSynthEmuProcessor&, const juce::File&);
    juce::String (*name)(const VSynthEmuProcessor&);
    juce::String (*note)(const VSynthEmuProcessor&);
    bool (*save)(const VSynthEmuProcessor&, const juce::String&, const juce::File&, std::string*);
    bool (*saveProject)(const VSynthEmuProcessor&, const juce::File&, const juce::File&, ProjectSaved&, std::string*);
    bool (*init)(VSynthEmuProcessor&, int choice, std::string*);
    juce::StringArray (*initChoices)();
};

// The V-Synth's PATCH Init takes no choice: its menu's one, Execute.  The VC-2's (v473) takes a Patch Category, its
// menu rom.4's 12 by its own names, as rom.4's window offers them
bool vsynthInit(VSynthEmuProcessor& p, int, std::string* why) { return initPatch(p, why); }
juce::StringArray vsynthInitChoices() { return juce::StringArray("Execute"); }
juce::StringArray vc2InitChoices() {
    juce::StringArray out;
    for (const std::string& name : vc2PatchCategoryNames()) out.add(juce::String(name));
    return out;
}

// In PluginMode's order
constexpr std::array<PatchKind, 2> kPatchKinds { {
    { "Load a V-Synth patch, or a project's folder", "p00", "patchFile",
      [](VSynthEmuProcessor& p, const juce::File& f) { return p.loadPatch(f); },
      [](const VSynthEmuProcessor& p) { return p.getPatchName(); },
      [](const VSynthEmuProcessor& p) { return p.getPatchNote(); }, savePatch, saveProject, vsynthInit,
      vsynthInitChoices },
    { "Load a VC-2 patch, or a project's folder", "p02", "vc2PatchFile",
      [](VSynthEmuProcessor& p, const juce::File& f) { return p.loadVc2Patch(f); },
      [](const VSynthEmuProcessor& p) { return p.vc2PatchName(); },
      [](const VSynthEmuProcessor&) { return juce::String(); }, nullptr, nullptr, initVc2Patch, vc2InitChoices },
} };

const PatchKind& patchKindOf(PluginMode mode) { return kPatchKinds[static_cast<size_t>(mode)]; }

using Parts = std::initializer_list<juce::Component*>;
} // namespace

PatchBox::PatchBox(VSynthEmuProcessor& p)
    : processor_(p), prev_("previous patch", 0.5f, Look::text), next_("next patch", 0.0f, Look::text) {
    Look::setTextLook(patchName_, Look::sans(13.0f, "Semibold"), Look::text, juce::Justification::centredLeft);
    Look::setTextLook(patchNote_, Look::sans(10.0f), Look::warning, juce::Justification::centredLeft);
    Look::setTextLook(projectLine_, Look::sans(10.0f), Look::muted, juce::Justification::centredLeft);
    patchNote_.setMinimumHorizontalScale(0.5f);
    projectLine_.setMinimumHorizontalScale(0.5f);
    patchName_.setMinimumHorizontalScale(0.5f);   // v473: as the lines under it, where INIT and the MIC chip leave less
    patchName_.setInterceptsMouseClicks(false, false);   // a click on the name is the box's: the project's menu
    browse_.onClick = [this] { choose(); };
    save_.onClick = [this] { save(); };
    init_.onClick = [this] { askInit(); };
    prev_.onClick = [this] { step(-1); };
    next_.onClick = [this] { step(1); };
    prev_.getProperties().set(Look::editorsOwn, true);   // the editor's own, no parameter's (LayoutCheck's walk)
    next_.getProperties().set(Look::editorsOwn, true);
    for (juce::Component* c : Parts { &patchNote_, &projectLine_ }) addChildComponent(c);
    for (juce::Component* c : Parts { &patchName_, &browse_, &prev_, &next_ }) addAndMakeVisible(c);
    addChildComponent(save_);
    addChildComponent(init_);
    setMode(mode_);
}

void PatchBox::setMode(PluginMode mode) {
    mode_ = mode;
    browsed_ = juce::File();
    project_ = {};
    save_.setVisible(patchKindOf(mode_).save != nullptr);
    init_.setVisible(patchKindOf(mode_).init != nullptr);
    resized();
    refresh();
}

// INIT: the mode's choices under the XT's own words, the one chosen run.  Told nothing once the box is gone.
void PatchBox::askInit() {
    const PatchKind& kind = patchKindOf(mode_);
    if (!kind.init) return;
    juce::PopupMenu menu;
    menu.addSectionHeader(kInitTitle);
    const juce::StringArray choices = initChoices();
    for (int i = 0; i < choices.size(); ++i) menu.addItem(i + 1, choices[i]);
    const juce::Component::SafePointer<PatchBox> box(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&init_), [box](int chosen) {
        if (box != nullptr && chosen > 0) box->initialize(chosen - 1);
    });
}

juce::StringArray PatchBox::initChoices() const {
    const PatchKind& kind = patchKindOf(mode_);
    return kind.initChoices ? kind.initChoices() : juce::StringArray();
}

void PatchBox::initialize(int choice) {
    const PatchKind& kind = patchKindOf(mode_);
    std::string why;
    if (kind.init && !kind.init(processor_, choice, &why))
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, kInitTitle, juce::String(why));
    refresh();
}

// The mode's save, from the menu Save… opens (UiPatchSave.h): over the browsed patch's file, or as a file chosen,
// offered in its project's Patch folder; the file written becomes the patch browsed.  Told nothing once the box is gone.
void PatchBox::save() {
    const PatchKind& kind = patchKindOf(mode_);
    if (!kind.save) return;
    const juce::File folder = browsed_ != juce::File() ? browsed_.getParentDirectory() : processor_.lastPatchFolder();
    const juce::Component::SafePointer<PatchBox> box(this);
    PatchSaving s { browsed_, folder, kind.name(processor_), kind.extension,
                    [box, save = kind.save](const juce::String& name, const juce::File& f, std::string* why) {
                        return box != nullptr && save(box->processor_, name, f, why);
                    },
                    [box, property = kind.fileProperty](const juce::File& f) {
                        if (box == nullptr) return;
                        box->processor_.apvts.state.setProperty(property, f.getFullPathName(), nullptr);
                        box->refresh();
                    } };
    if (kind.saveProject)
        s.saveProject = [box, save = kind.saveProject, own = browsed_](const juce::File& dest, ProjectSaved& r,
                                                                         std::string* why) {
            return box != nullptr && save(box->processor_, own, dest, r, why);
        };
    showPatchSaveMenu(save_, s, chooser_);
}

// The patch the session says was browsed, and its project, listed again only when it changes
void PatchBox::followBrowsed() {
    const juce::String path = processor_.apvts.state[patchKindOf(mode_).fileProperty].toString();
    const juce::File file = path.isEmpty() ? juce::File() : juce::File(path);
    if (file == browsed_) return;
    browsed_ = file;
    project_ = file == juce::File() ? ProjectPatches {} : projectOf(file, patchKindOf(mode_).extension);
}

void PatchBox::refresh() {
    const PatchKind& kind = patchKindOf(mode_);
    followBrowsed();
    const juce::String name = kind.name(processor_);
    Look::showText(patchName_, name.isNotEmpty() ? name : juce::String("No patch loaded"),
                   name.isNotEmpty() ? Look::text : Look::muted);
    // What the patch asks for that isn't here (its waves, an OSC's external input), under its name; else its project
    const juce::String note = kind.note(processor_);
    patchNote_.setText(note.isNotEmpty() ? "not here: " + note : juce::String(), juce::dontSendNotification);
    const bool listed = project_.at.has_value();
    const int at = project_.at.value_or(0), count = project_.patches.size();
    const juce::String where = listed
        ? projectName(project_) + ", " + juce::String(at + 1) + " of " + juce::String(count)
        : juce::String("not from a project: Browse... loads a patch or a project's folder");
    projectLine_.setText(listed ? where : juce::String(), juce::dontSendNotification);
    prev_.setEnabled(listed && at > 0);
    next_.setEnabled(listed && at + 1 < count);
    prev_.setTooltip(where);
    next_.setTooltip(where);
    const bool noted = note.isNotEmpty();
    if (noted == noted_ && listed == listed_) return;
    noted_ = noted;
    listed_ = listed;
    patchNote_.setVisible(noted_);
    projectLine_.setVisible(listed_ && !noted_);
    resized();
    repaint();
}

void PatchBox::resized() {
    auto box = getLocalBounds().reduced(4, 0);
    if (save_.isVisible()) save_.setBounds(box.removeFromRight(kSaveW).withSizeKeepingCentre(kSaveW, kButtonH));
    browse_.setBounds(box.removeFromRight(kBrowseW).withSizeKeepingCentre(kBrowseW, kButtonH));
    if (init_.isVisible()) init_.setBounds(box.removeFromRight(kInitW).withSizeKeepingCentre(kInitW, kButtonH));
    next_.setBounds(box.removeFromRight(kArrowW).withSizeKeepingCentre(kArrowW, kArrowH));
    prev_.setBounds(box.removeFromRight(kArrowW).withSizeKeepingCentre(kArrowW, kArrowH));
    box.removeFromLeft(kPatchTagW);
    auto lines = box.withTrimmedRight(4).reduced(0, 2);
    if (noted_ || listed_) {
        const auto under = lines.removeFromBottom(kNoteH);
        patchNote_.setBounds(under);
        projectLine_.setBounds(under);
    }
    menuMark_ = lines.removeFromRight(kMarkW);
    patchName_.setBounds(lines);
}

void PatchBox::paint(juce::Graphics& g) {
    const auto whole = getLocalBounds();
    Ui::fillRounded(g, whole.toFloat(), Look::panel, 6.0f);
    Ui::drawText(g, "PATCH", whole.withTrimmedLeft(12).withWidth(kPatchTagW), { Look::mono(11.0f), Look::muted });
    if (!listed_) return;
    const auto c = menuMark_.toFloat().getCentre();   // the name opens the project's menu: a mark that it does
    juce::Path mark;
    mark.addTriangle(c.x - kMark, c.y - kMark / 2, c.x + kMark, c.y - kMark / 2, c.x, c.y + kMark / 2);
    g.setColour(Look::label);
    g.fillPath(mark);
}

void PatchBox::mouseUp(const juce::MouseEvent& e) {
    if (patchName_.getBounds().contains(e.getPosition()) || menuMark_.contains(e.getPosition())) showMenu();
}

void PatchBox::showMenu() {
    if (!listed_) return;
    juce::PopupMenu menu;
    menu.addSectionHeader(projectName(project_));
    for (int i = 0; i < project_.patches.size(); ++i)
        menu.addItem(i + 1, project_.patches[i].getFileNameWithoutExtension(), true, project_.at == i);
    const auto options = juce::PopupMenu::Options().withTargetComponent(&patchName_).withMaximumNumColumns(1);
    menu.showMenuAsync(options, [this, list = project_.patches](int chosen) {
        if (chosen > 0 && chosen <= list.size()) load(list[chosen - 1]);
    });
}

void PatchBox::step(int by) {
    if (!project_.at) return;
    const int to = *project_.at + by;
    if (to >= 0 && to < project_.patches.size()) load(project_.patches[to]);
}

void PatchBox::load(const juce::File& patch) {
    const PatchKind& kind = patchKindOf(mode_);
    if (kind.load(processor_, patch))
        processor_.apvts.state.setProperty(kind.fileProperty, patch.getFullPathName(), nullptr);
    refresh();
}

// A patch of the mode's own, or a project's folder: its first patch, and the project to browse
void PatchBox::choose() {
    const PatchKind& kind = patchKindOf(mode_);
    chooser_ = std::make_unique<juce::FileChooser>(kind.title, processor_.lastPatchFolder(),
                                                   juce::String("*.") + kind.extension);
    chooser_->launchAsync(kOpen, [this](const juce::FileChooser& fc) {
        const juce::File chosen = fc.getResult();
        const juce::String extension = patchKindOf(mode_).extension;
        if (chosen.existsAsFile()) return load(chosen);
        if (!chosen.isDirectory()) return;
        const ProjectPatches p = projectAt(chosen, extension);
        if (!p.patches.isEmpty()) return load(p.patches.getFirst());
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon, "No patches",
                                               chosen.getFileName() + " holds no ." + extension
                                                   + " patches, in itself or in a Patch folder");
    });
}
