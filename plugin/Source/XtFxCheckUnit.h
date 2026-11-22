#pragma once
// What xt_fx's checks of the frame's way through the unit share (XtFxCheckUnit.cpp, XtFxCheckUnitRef.cpp; v274): an
// image's way as the check knows it, and the image's own truncation of the bus.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"
#include <numbers>

namespace xtfxcheck {
// [FW] The frame's way through the unit as this check knows it, an image a row, written apart from XtFxImage.cpp so that
// a gain wrong there shows: the bus's truncation routine with the floats it takes and the words it leaves (rom.2's
// 0x894db2b0, rom.4's 0x89396200, one routine on each image's buffers), the frame's gain into the chip, the system EQ's
// Switch and Master Level, the level MAIN and MULTI come out at, and whether DIR comes out
struct UnitWay {
    const FxIds& fx;
    const char* whose;     // what the results' lines call the frame: "the" (rom.2's, as before v274) or "vc2's"
    uint32_t truncate, floats, words;
    float frameGain;
    int eqSwitch, masterLevel;
    double level;          // rom.2's ⅛, the plugin's tones at Out Gain 0; rom.4's frame whole, the chip's ×2
    bool dirOut;           // rom.2's DIR reaches DIR out; rom.4's goes nowhere (VC2_VOCAL_DESIGNER.md, "Program A's routing")
};
const UnitWay& rom2Way();
const UnitWay& vc2Way();

// The image's own truncation in an interpreter: a block of 32 frames of 8 floats in, their int32 words out
struct Truncation {
    Sh4 c;
    const UnitWay& way;
    Truncation(const char* rom, const UnitWay& w);
    bool run(const float* in, int32_t* out);
    static constexpr int kWords = 256;
};

// A unit on an image with these parameters set by ID, sent and played until the chip has them (v400: what the input's
// checks each start from)
inline std::unique_ptr<XtFxUnit> settledUnit(const char* rom, const XtFxImage& image,
                                             const std::vector<std::pair<int, int>>& sets) {
    auto u = std::make_unique<XtFxUnit>(rom, image);
    for (const auto& [id, value] : sets) u->control().set(id, value);
    u->control().update();
    u->settle();
    return u;
}

// The outside input `in` (int32 L then R a frame) through the unit a frame at a time, a silent bus beside it: the words
// handed back for each frame, words 4 and 5
inline std::vector<int32_t> handedBack(XtFxUnit& u, const std::vector<int32_t>& in) {
    std::vector<int32_t> back(in.size());
    float frame[8] = {}, lr[2] = {};
    for (size_t i = 0; i + 1 < in.size(); i += 2) {
        u.pushInput(&in[i], 1);
        u.process(frame, lr, 1);
        back[i] = u.returned()[0];
        back[i + 1] = u.returned()[1];
    }
    return back;
}

void unitConversion(const char* rom, uint32_t seed, const UnitWay& way);   // XtFxCheckUnit.cpp
void unitLevels(const char* rom, bool print, const UnitWay& way);
void unitAgainstReference(const char* rom, int randomEvents, uint32_t seed, const UnitWay& way);   // XtFxCheckUnitRef.cpp
} // namespace xtfxcheck
#endif
