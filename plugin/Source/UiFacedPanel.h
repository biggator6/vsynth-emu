#pragma once
// A rack panel whose controls follow a Type parameter (research/UI_DESIGN.md, "The rack"; UI step 2): for each of Type's
// values (a face), a box beside Type where it has one and its knobs, one face shown at a time.  Its title line: the
// block's switch, its name (a click selects the block), Type and the face's box.  The OSC and COSM panels are made from
// it, each with its own table of faces and its own display.
#include "UiControls.h"
#include <functional>
#include <memory>
#include <vector>

class FacedPanel : public juce::Component {
public:
    std::function<void()> onSelect;
    void setSelected(bool selected);
    virtual void refresh();   // its face, if Type has moved
    int face() const { return face_; }
    juce::Rectangle<int> nameArea() const { return titleArea_; }   // where a click selects the block

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

protected:
    // What the panel is: its title, and the IDs of its block's switch and Type
    struct Names {
        juce::String title, switchId, typeId;
    };
    FacedPanel(juce::AudioProcessorValueTreeState& state, const Names& names, int faces);

    // A face's box (none if the ID names no choice) and a knob of it (none if the ID names no parameter)
    void addSub(int face, const juce::String& id);
    void addKnob(int face, const juce::String& id, const char* name, int dial);
    // The title line laid out across r: the switch, the name, Type and the box, each as wide as w says; what is left
    struct TitleWidths {
        int name = 56, type = 0, sub = 0;
    };
    juce::Rectangle<int> layoutTitle(juce::Rectangle<int> r, const TitleWidths& w);

    virtual void faceShown() {}                            // what a panel shows besides, by its face
    virtual void paintDisplay(juce::Graphics&) const = 0;

    juce::AudioProcessorValueTreeState& state_;
    Controls::Attached attached_;
    std::vector<std::vector<std::unique_ptr<Knob>>> knobs_;   // by face, in the order added
    juce::ComboBox type_;

private:
    juce::String typeId_;
    juce::Label name_;
    juce::ToggleButton on_;
    std::vector<std::unique_ptr<juce::ComboBox>> subs_;   // by face; null where a face has none
    juce::Rectangle<int> titleArea_;
    int face_ = -1;
    bool selected_ = false;
    void showFace(int face);
};
