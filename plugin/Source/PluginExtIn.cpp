// The outside input's two settings as host parameters (v206): External In Type and Source, the system's IDs 0x78 and
// 0x7c, with the firmware's names, ranges, defaults and values' text (its routine 0x89461a70, read against them in
// PluginTest's TEST_FX), appended after the zones' so every other keeps its place.  They reach the effects' driver
// through kFxKnobs (PluginProcessor.cpp); HARDWARE_FIRMWARE.md, "The input's way in, the chip's side".
#include "PluginProcessor.h"
#include "PluginSetting.h"

void VSynthEmuProcessor::addExtInParams(ParamList& params) {
    using Setting::Kind;
    const Setting::Spec specs[] = {
        { "fxExtInType", "External In Type", Kind::Choice, 0, 3, 1, { "STEREO", "MONO-L+R", "MONO-L", "MONO-R" },
          Setting::kPlain },
        { "fxExtInSource", "External In Source", Kind::Choice, 0, 1, 0, { "Analog", "USB" }, Setting::kPlain },
    };
    for (const Setting::Spec& s : specs) params.push_back(Setting::paramOf(s));
}
