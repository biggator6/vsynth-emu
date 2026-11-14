#pragma once
// A parameter's value as the XT's panel shows it, by the firmware's own routine (moved from XtFxControl.h as it stood,
// v199, then made an image's: rom.2's by default, rom.4's for Vc2Params).
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

class Sh4;

// What an image's value-text routine is (v199): its addresses in that image, and where its text is put right, by the
// image's IDs: an ID run as another with the format it lacks (formatOf), the row of strings its unit is read from
// (unitRow), and the IDs whose text is a time the image's driver works out (isTime, by `time`).
struct XtTextImage {
    uint32_t routine;            // (id, value, 1 to add the unit) → the text
    uint32_t strings;            // each ID's string below firstHigh: names split by 1, a range by 2, the unit after a 3
    uint32_t stringsHigh;        // and from firstHigh
    int firstHigh;
    uint32_t time;               // (value) → a time in samples at 44.1 kHz, for the IDs isTime says
    int (*formatOf)(int id);
    int (*unitRow)(int id);
    bool (*isTime)(int id);
};
const XtTextImage& xtVSynthText();   // rom.2's, with the user's decisions on its text (XtFxValueText.cpp)

// A name in an image, as its screens show it: at most `most` characters from `at`, up to a mark for the manual's
// footnotes (" *"), its spaces round it gone (from XtFxControl.cpp's catalog, v199, for rom.4's names too)
std::string xtFirmwareName(Sh4& image, uint32_t at, int most);

// A note value's text from the routine's: the LCD font's glyph for the note, then a triplet's 0x17, a dot or a space
// ("1/8T", "1/4."); empty if it isn't one.  Which note each glyph is, xt_fx control measures on rom.2's driver.
std::string xtNoteText(const std::string& routineText);

// A parameter's values as the XT's front panel shows them: "50 Hz", "-15 dB", "1/16", "L63", "MID+HIGH".  It runs the firmware's own routine (0x89461a70) for each one the first time it's asked for, in an
// interpreter of its own, so none of the text is in this source.  HARDWARE_FIRMWARE.md, "The effects' values as text".  One thread at a time, as the panel's the only one that asks.
// Another image's routine by its description (rom.4's, Vc2Params.cpp).
class XtFxValueText {
public:
    explicit XtFxValueText(const char* romPath, const XtTextImage& image = xtVSynthText());
    ~XtFxValueText();
    XtFxValueText(const XtFxValueText&) = delete;
    XtFxValueText& operator=(const XtFxValueText&) = delete;

    bool ok() const { return ok_; }

    // `value` as set() takes it, within the parameter's range: past it the firmware's routine runs off the end of its list.
    const std::string& text(int id, int value);

    // What someone typed, back to a value: its text whatever the case or spacing, a number with or without the unit (the nearest there is), or the start of a name.  `current` if it's none of those.
    int value(int id, int low, int high, const std::string& typed, int current);

private:
    std::string make(int id, int value);
    std::string unit(int id);

    const XtTextImage& image_;
    std::unique_ptr<Sh4> sh4_;
    bool ok_ = false;
    std::unordered_map<uint32_t, std::string> shown_;
};
