#pragma once
// A V-Synth patch saved (v602; HARDWARE_FIRMWARE.md, "A patch saved"): the loaded patch's store (the processor's, kept in
// the session; the firmware's default patch with none loaded) with what the plugin now holds written into it, a value
// wherever it differs from the store's own read, by rom.2's setters; then the file as rom.2 writes one (XtFwPatchFile.h).
// So a patch saved unchanged is its file byte for byte and an edited one carries its edits, while zones 2…16, which the
// plugin doesn't edit, and each OSC's Wave Number stay the store's.  Message thread.
#include <juce_core/juce_core.h>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class VSynthEmuProcessor;

// Each OSC's Wave Number, where a save gives it one of its own (v603: a project's save, an outside wave at its new slot)
using PatchWaveSlots = std::array<std::optional<int>, 2>;

// The wave file OSC `osc` plays, as the processor keeps it (its loadedFilePath); none for none (v603)
juce::File oscWaveFile(const VSynthEmuProcessor& p, int osc);

// The patch as a .p00's bytes, named `name` (its first 12 characters, padded with spaces), each OSC's Wave Number the
// store's but where `slots` gives one; false (and why) with no firmware or a firmware routine that fails
bool patchFileOf(const VSynthEmuProcessor& p, const juce::String& name, const PatchWaveSlots& slots,
                 std::vector<uint8_t>& file, std::string* why = nullptr);
bool patchFileOf(const VSynthEmuProcessor& p, const juce::String& name, std::vector<uint8_t>& file,
                 std::string* why = nullptr);
// And written to `file`; false (and why) as above, or a file that can't be written
bool savePatch(const VSynthEmuProcessor& p, const juce::String& name, const juce::File& file,
               std::string* why = nullptr);
