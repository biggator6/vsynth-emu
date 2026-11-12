#pragma once
// A host parameter against the firmware's own: its name [FW 0x89790338], its descriptor's range and default [FW
// 0x897a167c] and each value's text (the firmware's routine, XtFxValueText).  VSynthEmuArpPluginTest's (v190), shared
// with the MSM's checks (MsmPluginTest.cpp, v193).
#include "TestChecks.h"
#include "../Source/Sh4Interp.h"
#include "../Source/XtFiles.h"
#include "../Source/XtFxControl.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <cmath>
#include <string>

// The firmware's image, for its names, and its text routine
struct Firmware {
    explicit Firmware(const juce::File& rom) : path(rom.getFullPathName().toStdString()), text(path.c_str()) {
        image.loadRom(path.c_str());
    }
    std::string path;
    XtFxValueText text;
    Sh4 image;
};

// A parameter's name as the firmware has it, up to " *" and trimmed
inline juce::String firmwareName(Sh4& image, int id) {
    juce::String name;
    for (uint32_t a = image.rd32(0x89790338 + 4 * (uint32_t)id); image.rd8(a); ++a) name += (char)image.rd8(a);
    return name.upToFirstOccurrenceOf(" *", false, false).trim();
}

// What a parameter should be: the XT's ID, the name, whether each value's text is checked, and whether it counts from 1
// where the firmware's routine counts from 0 (an End Point, a Zone: the manual's)
struct FirmwareParam {
    int fw;
    juce::String name;
    bool texts, fromOne;
};

inline void paramAgainstFirmware(TestChecks& check, juce::RangedAudioParameter* prm, const FirmwareParam& f,
                                 XtFxValueText& text) {
    const XtParamDesc* d = xtParamDesc(f.fw);
    const std::string what = (prm ? prm->paramID.toStdString() : "ID " + std::to_string(f.fw)) + ": ";
    if (!prm || !d) {
        check(false, what + "the parameter and its descriptor should be there");
        return;
    }
    check(prm->name == f.name, what + "named " + prm->name.toStdString());
    const auto& r = prm->getNormalisableRange();
    const int def = (int)std::lround(prm->convertFrom0to1(prm->getDefaultValue()));
    check((int)r.start == d->min - d->centre && (int)r.end == d->max - d->centre, what + "the descriptor's range");
    check(def == d->def - d->centre, what + "the descriptor's default, not " + std::to_string(def));
    for (int v = (int)r.start; v <= (int)r.end && f.texts; ++v) {
        const juce::String want = f.fromOne ? juce::String(v + 1) : juce::String(text.text(f.fw, v));
        const juce::String got = prm->getText(prm->convertTo0to1((float)v), 64);
        const std::string reads = std::to_string(v) + " reads " + got.toStdString();
        check(got == want, what + reads + ", not " + want.toStdString());
    }
}
