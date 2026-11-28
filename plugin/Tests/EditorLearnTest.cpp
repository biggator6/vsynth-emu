// MIDI learn in the editor (v463; research/UI_DESIGN.md, "MIDI learn"): OSC 1's Level knob's right-click opens its
// menu through the editor's listener; Learn arms its parameter, which the editor's line says; a controller through
// processBlock is then learnt, moves the parameter and the knob with it; the menu then offers Forget, which lets it
// go; the map lists each controller learnt and forgets one and all.  Every control's own learning, and a right-click
// moving nothing, are LayoutCheck::learns', on every page and block.
#include "EditorTest.h"
#include "TestMidi.h"
#include "../Source/UiMidiLearn.h"
#include "../Source/UiMidiMap.h"
#include <utility>

namespace {
using Items = std::vector<std::pair<int, juce::String>>;

Items itemsOf(const juce::PopupMenu& m) {
    Items out;
    for (juce::PopupMenu::MenuItemIterator it(m); it.next();)
        if (const auto& item = it.getItem(); item.itemID != 0) out.emplace_back(item.itemID, item.text);
    return out;
}

// A right-click's menu, as the editor's listener showed it, and the choice it would make
struct Shown {
    Items items;
    std::function<void(int)> chosen;
};

Shown rightClickOn(SynthEditor& ed, juce::Component& c) {
    Shown shown;
    ed.learnMenus().showWith([&shown](const juce::PopupMenu& m, juce::Component&, std::function<void(int)> chosen) {
        shown = { itemsOf(m), std::move(chosen) };
    });
    LayoutCheck::rightClick(c, ed.learnMenus());
    ed.learnMenus().showWith([](const juce::PopupMenu&, juce::Component&, std::function<void(int)>) {});
    return shown;
}

void theMap(VSynthEmuProcessor& p, TestChecks& check) {
    PluginMidiLearn& learn = p.midiIn().learn;
    for (const auto& [cc, id] : { std::pair { 21, "oscLevel" }, std::pair { 22, "portaTime" } }) {
        learn.arm(p.midiIn().edits.indexOf(id));
        playMessages(p, { controller(cc, 10) });
    }
    MidiMapView map(p);
    check(map.rows() == 2, "the map should list the two controllers learnt");
    check(map.rows() == 2 && map.rowText(0).startsWith("CC 21 ") && map.rowText(1).startsWith("CC 22 "),
          "by controller, each its CC first");
    map.forgetRow(0);
    check(learn.parameterOf(21) < 0 && learn.parameterOf(22) >= 0, "a row's Forget should let its controller go");
    map.forgetAll();
    check(learn.entries().empty(), "and Forget all every one");
    check(MidiMapView(p).rows() == 0, "a map of none should list none");
}
}  // namespace

void learnChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    check(ed.learnMenus().listening(), "the editor should listen for a right-click on every control under it");
    ed.showPage(Blocks::kSynthPage);
    LearnSlider* knob = l.firstOf<LearnSlider>([](LearnSlider& s) { return MidiLearn::learnsFor(s) == "oscLevel"; });
    check(knob != nullptr, "OSC 1's Level knob should learn for its parameter");
    if (knob == nullptr) return;
    const int level = p.midiIn().edits.indexOf("oscLevel");
    PluginMidiLearn& learn = p.midiIn().learn;
    Shown shown = rightClickOn(ed, *knob);
    check(shown.items == Items { { MidiLearn::learn, "Learn MIDI CC" }, { MidiLearn::map, "MIDI map..." } },
          "a knob's menu should offer Learn and the map");
    if (shown.chosen) shown.chosen(MidiLearn::learn);
    check(learn.armed() == level, "Learn should wait for a controller for the knob's parameter");
    check(MidiLearn::learning(p).contains(p.midiIn().edits.param(level)->getName(64)), "and the editor say so");
    check(rightClickOn(ed, *knob).items.front().second == "Cancel learning", "its menu then offering to cancel");
    playMessages(p, { controller(20, 64) });
    check(learn.parameterOf(20) == level && learn.armed() < 0, "the next controller should be the knob's");
    p.midiIn().edits.flush();
    check(juce::exactlyEqual(knob->getValue(), 64.0), "and its value move the knob");
    shown = rightClickOn(ed, *knob);
    check(shown.items.size() == 3 && shown.items[1].second == "Forget CC 20", "the menu should then offer Forget");
    if (shown.chosen) shown.chosen(MidiLearn::forget);
    check(learn.parameterOf(20) < 0, "which lets the controller go");
    theMap(p, check);
}
