#pragma once
// Names to choose from, one chosen, a click choosing another (UI step 3): the page bar's pages, a block's envelopes, the
// lower panel's list.  Drawn, not buttons, so no tab is taken for a control of a parameter.  Across by default, each as
// wide as its name, the chosen one underlined in the accent; a kind laid out or drawn otherwise overrides tabs and
// paintTab.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

class Tabs : public juce::Component {
public:
    // How the names are set: their face, where the first starts, the space each side of a name
    struct Style {
        juce::Font font;
        int left = 0, pad = 10;
    };
    Tabs(juce::StringArray names, Style style);

    std::function<void(int tab)> onChoose;   // a click's choice, after it is shown
    void choose(int tab);                    // shown, with no call
    int chosen() const { return chosen_; }
    const juce::StringArray& names() const { return names_; }
    virtual std::vector<juce::Rectangle<int>> tabs() const;   // where each name is, in their order

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

protected:
    virtual void paintTab(juce::Graphics& g, int tab, juce::Rectangle<int> r) const;
    const Style style_;

private:
    juce::StringArray names_;
    int chosen_ = 0;
};
