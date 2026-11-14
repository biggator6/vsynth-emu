#pragma once
// A titled card of a page in D's look (research/UI_DESIGN.md, "The pages"; UI step 4): a rack panel's frame, its name at
// the top left, what its title line holds besides from the right (switches, boxes, tabs), and its body under that.  It
// owns what it holds.
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <utility>
#include <vector>

class Card : public juce::Component {
public:
    explicit Card(const juce::String& title);
    static constexpr int kTitleH = 24, kPad = 12, kGap = 8;

    // A component on the title line, `width` wide, laid from the right in the order given
    template <class T>
    T& toTitle(std::unique_ptr<T> c, int width) {
        T& made = *c;
        addAndMakeVisible(made);
        head_.push_back({ std::move(c), width });
        return made;
    }
    // The body, under the title line
    template <class T>
    T& setBody(std::unique_ptr<T> b) {
        T& made = *b;
        addAndMakeVisible(made);
        body_ = std::move(b);
        return made;
    }
    juce::Component* body() const { return body_.get(); }
    void setHeading(const juce::String& title);
    int heightFor(int bodyH) const { return 2 * kPad + kTitleH + kGap + bodyH; }   // the card's, for a body that high
    int bodyWidthFor(int width) const { return width - 2 * kPad; }

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label name_;
    std::vector<std::pair<std::unique_ptr<juce::Component>, int>> head_;
    std::unique_ptr<juce::Component> body_;
};
