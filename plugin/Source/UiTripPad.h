#pragma once
// The Time Trip pad (v202; research/UI_DESIGN.md, "The rack"; CONTROLLERS.md, "Time Trip Pad"): the XT's pad, sending
// its controllers into the synth through the processor's editor queue as the XT's own pad and the iOS app send them,
// Time Trip Assign X then Y (CC82 and CC83 by default), 1…127 across and up.  A press is a finger on the pad, which
// freezes the wave where it plays; a drag round the centre scrubs it, clockwise forward, at the same pitch (XtTripPad
// counts the angle); a release sends 0 and 0, lifting the finger, unless Hold keeps it where it is.  An assign set to
// OFF (0, or 32) leaves the pad nothing to send, and the pad says so.
#include "PluginProcessor.h"
#include <functional>
#include <optional>

class TripPad : public juce::Component {
public:
    // The controllers it sends, X then Y, 0 for none: by default the V-Synth's Time Trip Assign X and Y; a mode's own
    // (v453: the VC-2's, the pair its Pad Mode names, d-10)
    using Assigns = std::function<juce::Point<int>()>;
    explicit TripPad(VSynthEmuProcessor& p, Assigns assigns = {}) : processor_(p), assigns_(std::move(assigns)) {}
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // A finger kept where it is when let go; Hold off lifts one that was kept
    void setHold(bool on);
    // Where a point is as the pad's controllers carry it: 1…127 across from the left and up from the bottom
    juce::Point<int> valueAt(juce::Point<float> at) const;
    // Whether both assigns name a controller, so the pad has something to send
    bool canSend() const;
    std::optional<juce::Point<int>> finger() const { return finger_; }   // as last sent; none when lifted

private:
    VSynthEmuProcessor& processor_;
    Assigns assigns_;
    std::optional<juce::Point<int>> finger_;
    bool hold_ = false, down_ = false;
    int assigned(int assign) const;               // the controller an assign names, 0 for none
    void send(juce::Point<int> value);           // X then Y: the synth's pad moves on Y
    void lift();
};
