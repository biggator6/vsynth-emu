// VSynthEmuEditorTest's checks of the Time Trip pad (EditorTest.h; v202): what a press, a drag and a release send, the
// XT's pad controllers by the assigns as they stand (X then Y, 1…127 across and up, 0 and 0 to lift); Hold keeping the
// finger, the session keeping Hold, and Hold let go lifting it; an assign OFF leaving the pad nothing to send.
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/UiTripColumn.h"

namespace {

const char* const kAssignX = "assignTripX";       // Time Trip Assign X's parameter
constexpr float kTripX = 82.0f, kOther = 20.0f;   // its default (CC82, the chart's) and another
constexpr float kOffs[] = { 0.0f, 32.0f };        // its two values that are OFF, "OFF" and "(OFF)" (0x894d82e0)

// The controllers as words: "82=64 83=64 "
std::string controllers(const std::vector<juce::MidiMessage>& got) {
    std::string s;
    for (const auto& m : got)
        s += std::to_string(m.getControllerNumber()) + "=" + std::to_string(m.getControllerValue()) + " ";
    return s;
}

void click(TripPad& pad, juce::Point<float> at) {
    LayoutCheck::pressAt(pad, at);
    pad.mouseUp(LayoutCheck::mouseAt(pad, at, at));
}

std::string where(juce::Point<float> at) {
    return std::to_string(juce::roundToInt(at.x)) + "," + std::to_string(juce::roundToInt(at.y));
}

// The centre, the top right and the bottom left, each clicked: X then Y, up being up, then 0 and 0; a drag from the
// centre to the right edge's middle
void padSends(TripPad& pad, VSynthEmuProcessor& p, TestChecks& check) {
    taken(p);
    const auto r = pad.getLocalBounds().toFloat();
    const juce::Point<float> topRight { r.getRight() - 0.5f, r.getY() + 0.5f };
    const juce::Point<float> bottomLeft { r.getX() + 0.5f, r.getBottom() - 0.5f };
    const std::pair<juce::Point<float>, std::string> cases[] = { { r.getCentre(), "82=64 83=64 82=0 83=0 " },
                                                                 { topRight, "82=127 83=127 82=0 83=0 " },
                                                                 { bottomLeft, "82=1 83=1 82=0 83=0 " } };
    for (const auto& [at, want] : cases) {
        click(pad, at);
        const std::string got = controllers(taken(p));
        check(got == want, "a click at " + where(at) + " sends " + got);
    }
    LayoutCheck::dragFrom(pad, r.getCentre(), { r.getRight() - 0.5f, r.getCentreY() });
    const std::string drag = controllers(taken(p));
    check(drag == "82=64 83=64 82=127 83=64 82=0 83=0 ", "a drag to the right edge sends " + drag);
}

// Hold on: a click leaves the finger where it was, the session keeps Hold; Hold off lifts it
void padHold(LayoutCheck& l, TripPad& pad, VSynthEmuProcessor& p, TestChecks& check) {
    juce::Button* hold = l.buttonOf("Pad Hold");
    check(hold != nullptr, "no Pad Hold under the pad");
    if (hold == nullptr) return;
    taken(p);
    hold->setToggleState(true, juce::sendNotificationSync);
    click(pad, pad.getLocalBounds().toFloat().getCentre());
    const std::string held = controllers(taken(p));
    check(held == "82=64 83=64 " && pad.finger().has_value(), "with Hold, a click sends " + held);
    check((bool)p.apvts.state.getProperty(TripColumn::kPadHold, false), "the session doesn't keep Hold");
    hold->setToggleState(false, juce::sendNotificationSync);
    const std::string lifted = controllers(taken(p));
    check(lifted == "82=0 83=0 " && !pad.finger().has_value(), "Hold let go sends " + lifted);
}

// Another controller on Time Trip Assign X: the pad sends it; each OFF: the pad sends nothing, and knows it (32 too, a
// controller's number that names none, as the firmware reads it)
void padAssigns(TripPad& pad, VSynthEmuProcessor& p, TestChecks& check) {
    taken(p);
    const auto centre = pad.getLocalBounds().toFloat().getCentre();
    setParam(p, kAssignX, kOther);
    click(pad, centre);
    const std::string other = controllers(taken(p));
    check(other == "20=64 83=64 20=0 83=0 ", "with Trip X on CC20, a click sends " + other);
    for (const float off : kOffs) {
        setParam(p, kAssignX, off);
        click(pad, centre);
        const std::string sent = controllers(taken(p));
        check(!pad.canSend() && sent.empty(), "with Trip X at " + std::to_string((int)off) + ", the pad sends " + sent);
    }
    setParam(p, kAssignX, kTripX);
}

} // namespace

void padChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    TripPad* pad = theOne<TripPad>(ed, "Time Trip pad", check);
    if (pad == nullptr) return;
    padSends(*pad, p, check);
    padHold(l, *pad, p, check);
    padAssigns(*pad, p, check);
}
