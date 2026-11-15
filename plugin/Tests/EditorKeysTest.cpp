// VSynthEmuEditorTest's checks of the footer's keyboard (EditorTest.h; v201): a key's press, a drag across keys and the
// release reach the processor's editor queue as their notes, the velocity by where on the key; a press sounds through
// processBlock; folding it away shortens the window by the footer's height, holds the layout, and an editor on the
// folded session opens folded.
#include "EditorTest.h"
#include "TestControls.h"
#include "../Source/PluginParamIds.h"
#include "../Source/UiFooter.h"
#include <vector>

namespace {

constexpr int kWhiteKey = 60, kBlackKey = 61, kDragTo = 64;   // example keys: C4, C#4 over C4 and D4, E4
constexpr double kRate = 48000.0;
constexpr int kBlock = 512, kBlocks = 8;
constexpr float kAnalog = 0.0f, kPcm = 1.0f;                  // OSC Type's values; PCM its default

// The keys' messages as words: "on 60 off 60 "
std::string said(const std::vector<juce::MidiMessage>& got) {
    std::string s;
    for (const auto& m : got) {
        const char* kind = m.isNoteOn() ? "on " : m.isNoteOff() ? "off " : "? ";
        s += kind + std::to_string(m.getNoteNumber()) + " ";
    }
    return s;
}

void click(Footer& footer, juce::Point<float> at) {
    LayoutCheck::pressAt(footer, at);
    footer.mouseUp(LayoutCheck::mouseAt(footer, at, at));
}

// A white key, a black one where it lies over two white ones, the velocity lower on a key, a drag across keys
void keyPresses(Footer& footer, VSynthEmuProcessor& p, TestChecks& check) {
    taken(p);
    for (const int key : { kWhiteKey, kBlackKey }) {
        click(footer, footer.keyBounds(key).getCentre());
        const std::string got = said(taken(p)), k = std::to_string(key);
        check(got == "on " + k + " off " + k + " ", "a click on key " + k + " sends " + got);
    }
    const auto b = footer.keyBounds(kWhiteKey);
    click(footer, { b.getCentreX(), b.getY() + 1.0f });
    click(footer, { b.getCentreX(), b.getBottom() - 1.0f });
    const auto got = taken(p);
    check(got.size() == 4 && got[0].getVelocity() < got[2].getVelocity(), "a press lower on a key isn't louder");
    LayoutCheck::dragFrom(footer, footer.keyBounds(kWhiteKey).getCentre(), footer.keyBounds(kDragTo).getCentre());
    const std::string drag = said(taken(p));
    check(drag == "on 60 off 60 on 64 off 64 ", "a drag from key 60 to 64 sends " + drag);
    check(!footer.held().has_value(), "a key stays held after its release");
}

// OSC 1 on ANALOG, which needs no wave: silent with no key down, sounding with one pressed on the footer
void keySounds(Footer& footer, VSynthEmuProcessor& p, TestChecks& check) {
    taken(p);
    setParam(p, Params::kOscType, kAnalog);
    p.prepareToPlay(kRate, kBlock);
    juce::AudioBuffer<float> buffer(2, kBlock);
    juce::MidiBuffer none;
    const auto loudest = [&] {
        float peak = 0.0f;
        for (int b = 0; b < kBlocks; ++b) {
            p.processBlock(buffer, none);
            peak = std::max(peak, buffer.getMagnitude(0, kBlock));
        }
        return peak;
    };
    check(loudest() == 0.0f, "the synth sounds with no key pressed");
    const auto at = footer.keyBounds(kWhiteKey).getCentre();
    LayoutCheck::pressAt(footer, at);
    check(loudest() > 0.0f, "a key pressed on the footer doesn't sound");
    footer.mouseUp(LayoutCheck::mouseAt(footer, at, at));
    loudest();
    setParam(p, Params::kOscType, kPcm);
}

// Folded: the footer gone and the window shorter by its height at the window's scale, the layout holding, the session
// keeping it; shown again, the window as it was
void keyboardFold(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    juce::Button* sw = l.buttonOf("Keyboard");
    check(sw != nullptr, "no Keyboard switch");
    if (sw == nullptr) return;
    const int w = ed.getWidth(), shownH = ed.getHeight();
    sw->setToggleState(false, juce::sendNotificationSync);
    const int foldedH = juce::roundToInt((float)(Look::height - Look::footerH) * ed.scale());
    check(!ed.keyboardShown() && ed.getWidth() == w && ed.getHeight() == foldedH,
          "folded, the window is " + std::to_string(ed.getWidth()) + "x" + std::to_string(ed.getHeight()));
    l.check("the keyboard folded");
    check(!(bool)p.apvts.state.getProperty(SynthEditor::kKeyboardShown, true), "the session doesn't keep the fold");
    {
        SynthEditor second(p);
        check(!second.keyboardShown(), "an editor on the folded session opens with the keyboard");
    }
    sw->setToggleState(true, juce::sendNotificationSync);
    check(ed.keyboardShown() && ed.getHeight() == shownH, "shown again, the window isn't as it was");
}

} // namespace

void keyChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check) {
    Footer* footer = theOne<Footer>(ed, "footer", check);
    if (footer == nullptr) return;
    keyPresses(*footer, p, check);
    keySounds(*footer, p, check);
    keyboardFold(l, ed, p, check);
}
