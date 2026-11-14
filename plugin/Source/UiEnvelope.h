#pragma once
// A block's envelopes in SELECTED (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): a tab for each
// envelope its kind has (UiBlockKinds.h), the chosen one's curve, whose points drag its Attack, its Decay with its
// Sustain, and its Release, and a bar for each of its parameters.  The curve alone, not dragged, is ENVELOPES' tile.
#include "UiBlockKinds.h"
#include "UiFields.h"
#include "UiTabs.h"
#include "UiWatch.h"
#include <functional>
#include <memory>
#include <vector>

// An envelope's curve: the attack up to the top, the decay down to the sustain's level, a hold, the release to the
// floor; each time across up to a quarter of the width, by its proportion of its range
class EnvCurve : public juce::Component {
public:
    EnvCurve(Controls::State& state, const Blocks::Block& b, const Blocks::EnvName& env, bool editable);

    // The points to drag: the attack's peak (Attack), the decay's end (Decay across, Sustain up), the release's end
    enum class Point { attack, decay, release };
    static constexpr int kPoints = 3;
    juce::Point<float> pointAt(Point p) const;
    bool complete() const { return watch_.complete(); }   // its four parameters were all found
    std::function<void()> onClick;                         // a click on a curve not edited here (ENVELOPES')

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    Watch watch_;   // Attack, Decay, Sustain, Release
    bool editable_;
    int dragging_ = -1;
    juce::Rectangle<float> area() const;
    juce::Path shape() const;
};

class EnvelopeView : public juce::Component {
public:
    EnvelopeView(Controls::State& state, const Blocks::Block& b);
    int count() const { return (int)envs_.size(); }
    void showEnvelope(int env);
    int shown() const { return tabs_.chosen(); }
    Tabs& tabs() { return tabs_; }
    EnvCurve& curve(int env) { return *envs_[(size_t)env].curve; }
    void resized() override;

private:
    struct Envelope {
        std::unique_ptr<EnvCurve> curve;
        std::vector<std::unique_ptr<Bar>> bars;
    };
    Tabs tabs_;
    std::vector<Envelope> envs_;
};
