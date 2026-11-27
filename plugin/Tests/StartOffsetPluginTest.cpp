// Start Offset's text in time (v478; the user's d-7, research/NEXT_ISSUES.md item 5), the plugin's own where the XT
// shows the sample count (HARDWARE_FIRMWARE.md {#wave-edit-read}):
// - the law at chosen values, as literals written from the rule (ms to two places below 44100 samples, s to three from
//   it), not from the code;
// - each Start Offset parameter by its ID, the V-Synth's two and the VC-2's in both zones: what it says at those values,
//   and what it says read back to a value (the same sample below a second, the same text above);
// - the forms a person types into the box, each to its sample.
// Run by VSynthEmuArpPluginTest after the VC-2's parameters.
#include "StartOffsetPluginTest.h"
#include "TestChecks.h"
#include "../Source/PluginParamIds.h"
#include "../Source/PluginProcessor.h"
#include "../Source/PluginStartOffset.h"
#include "../Source/PluginVc2Params.h"
#include <string>
#include <vector>

namespace {

struct Said {
    int samples;
    const char* text;
};
constexpr Said kSaid[] = {
    { 0, "0.00 ms" },      { 1, "0.02 ms" },     { 441, "10.00 ms" },  { 11025, "250.00 ms" },
    { 44099, "999.98 ms" }, { 44100, "1.000 s" }, { 55125, "1.250 s" }, { kStartOffsetMax, "380.436 s" },
};

struct Typed {
    const char* text;
    int samples;
};
constexpr Typed kTyped[] = {
    { "250 ms", 11025 },  { "250ms", 11025 }, { " 250.00 MS ", 11025 }, { "1.25 s", 55125 },
    { "1.25 sec", 55125 }, { "11025 smp", 11025 }, { "250", 11025 }, { "0.02 ms", 1 },
    { "-5 ms", 0 },        { "999 s", kStartOffsetMax }, { "", 0 },
};

// Every Start Offset parameter's ID: OSC 1's and OSC 2's, then the VC-2's (rom.4's 0x306 and 0x33a in each zone)
std::vector<juce::String> startOffsetIds() {
    std::vector<juce::String> ids { Params::kStartOffset, Params::oscId(1, Params::kStartOffset) };
    for (const Vc2HostParam& h : vc2HostParams())
        if (h.id == 0x306 || h.id == 0x33a) ids.push_back(h.paramId);
    return ids;
}

void law(TestChecks& check) {
    for (const Said& s : kSaid)
        check(startOffsetText(s.samples) == s.text,
              std::to_string(s.samples) + " says \"" + startOffsetText(s.samples).toStdString() + "\", not " + s.text);
    for (const Typed& t : kTyped)
        check(startOffsetFromText(t.text) == t.samples, std::string("\"") + t.text + "\" sets " +
                  std::to_string(startOffsetFromText(t.text)) + ", not " + std::to_string(t.samples));
}

void eachParameter(TestChecks& check) {
    VSynthEmuProcessor p;
    const std::vector<juce::String> ids = startOffsetIds();
    check(ids.size() == 6, "there should be six Start Offsets: two V-Synth OSCs', the VC-2's two in two zones");
    for (const juce::String& id : ids) {
        auto* r = dynamic_cast<juce::RangedAudioParameter*>(p.apvts.getParameter(id));
        check(r != nullptr, id.toStdString() + " isn't a parameter");
        if (r == nullptr) continue;
        check(r->getNormalisableRange().end == (float)kStartOffsetMax, id.toStdString() + "'s range isn't 2^24 - 1");
        for (const Said& s : kSaid) {
            const juce::String text = r->getText(r->convertTo0to1((float)s.samples), 64);
            const int back = juce::roundToInt(r->convertFrom0to1(r->getValueForText(text)));
            const std::string at = id.toStdString() + " at " + std::to_string(s.samples);
            check(text == s.text, at + " says \"" + text.toStdString() + "\", not " + s.text);
            check(s.samples < kStartOffsetRate ? back == s.samples
                                               : r->getText(r->convertTo0to1((float)back), 64) == text,
                  at + ": its text read back is " + std::to_string(back));
        }
    }
}

} // namespace

int startOffsetPluginTest(const juce::File&, const juce::File&) {
    TestChecks check { "startoffset" };
    law(check);
    eachParameter(check);
    return check.summary();
}
