#pragma once
// MIDI learn from a control (v463; research/NEXT_ISSUES.md item 1; research/UI_DESIGN.md, "MIDI learn"): every control
// made from a parameter's ID is marked learnable by its maker (MidiLearn::attach: the knob's, a bar's, Controls' boxes
// and switches, the path's Structure, the header's Effects, the FX page's), and a right-click on any of them opens its
// menu (MidiLearn::Menus, the editor's, listening to every control in it): Learn MIDI CC (the next controller the plugin
// gets is the parameter's; Cancel learning while it waits), Forget CC n when it has one, and MIDI map..., every
// controller learnt (MidiMapView, in a call-out).  A slider that learns (LearnSlider) leaves a right-click to the menu,
// as a box does, and a switch is kept from toggling on one.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

class VSynthEmuProcessor;

// A slider a right-click doesn't move: the press is the menu's
class LearnSlider : public juce::Slider {
public:
    using juce::Slider::Slider;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

private:
    bool menuPress_ = false;   // this press is a right-click's
};

namespace MidiLearn {

// The control learns for the parameter `id`
void attach(juce::Component& control, const juce::String& id);
// The parameter a control learns for; empty for none (the editor's test holds each control to the parameter it moves)
juce::String learnsFor(const juce::Component& control);

// The menu's items, and the menu for a parameter: Learn or Cancel, Forget if it has a controller, the map
enum Item { learn = 1, cancel, forget, map };
juce::PopupMenu menuFor(VSynthEmuProcessor& p, const juce::String& id);
// What an item does for a control's parameter; the map opens at the control
void choose(VSynthEmuProcessor& p, juce::Component& control, const juce::String& id, int item);
// What the editor says while a controller is being learnt, empty when none is (the page bar's line)
juce::String learning(VSynthEmuProcessor& p);

// The editor's: a right-click on any learnable control under it opens the control's menu
class Menus : public juce::MouseListener {
public:
    explicit Menus(VSynthEmuProcessor& p) : processor_(p) {}
    ~Menus() override;
    // Every press under the editor, its children's too, from now until this goes
    void listenTo(juce::Component& editor);
    bool listening() const { return editor_ != nullptr; }
    // How a menu is shown, the item chosen then given back (0 for none): the PopupMenu at the control, or a test's
    using Shower = std::function<void(const juce::PopupMenu& menu, juce::Component& control, std::function<void(int)>)>;
    void showWith(Shower shower) { shower_ = std::move(shower); }
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

private:
    VSynthEmuProcessor& processor_;
    Shower shower_;
    juce::Component* editor_ = nullptr;
};

}
