#pragma once
// The V-Synth's preset waves in the update's disk image, and a project's link to one, a .w00 (v600).  A project's Wave
// folder holds its own waves (.wav, .vpw) and links: a link file is a "link" chunk holding the wave's number in the
// V-Synth's wave library and a "name" chunk, its 16 characters (36 bytes; Internal.prj's 224, and a user's project's,
// whose slot NNN may link to any preset wave).  The library ("V-Synth Wave.bin", a WLF08 file past the partition whose
// first version names VSYN2W) takes a wave to its map and a map to its sample, and sample n is the nth stream that
// starts on a sector past the partition.  That last step is the preset drive's (D:\), which a kernel no image holds
// serves, so it rests on two sources: each sample's kind byte is its stream's encode type, 224 of 224 in order (111
// shuffled), and 1stchoice's waves, 224 of them byte for byte the streams, name the same instrument in every slot.
// HARDWARE_FIRMWARE.md, "A project's wave links".  No JUCE; not for the audio thread.
#include "XtFiles.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// A link file's wave number and its name, trailing spaces gone; none (and why) for a file that isn't one
struct XtWaveLink {
    int number = 0;
    std::string name;
};
std::optional<XtWaveLink> xtWaveLinkRead(const uint8_t* file, size_t size, std::string* why = nullptr);

// One preset wave: its name in the library, its sample's kind byte (the encode type: 1 SOLO, 3 BACKING, 5 ENSEMBLE), and
// its stream's place in the image and its bytes, to the next stream
struct XtPresetWave {
    std::string name;
    int kind = 0;
    size_t at = 0, bytes = 0;
};

// The library's waves, each to its stream.  build() is false (and why) when there's no V-Synth library past the
// partition, or it doesn't lead each wave to one stream: a wave or a map of more than one split (the VC-2's library has
// them, the V-Synth's none), a number outside its section, a sample past the streams on a sector, or a stream whose type
// isn't its sample's kind byte.
class XtPresetWaves {
public:
    [[nodiscard]] bool build(const XtDiskImage& disk, std::string* why = nullptr);
    // By a link's number, from 1; nullptr outside the library
    const XtPresetWave* wave(int number) const;
    int count() const { return static_cast<int>(waves_.size()); }

private:
    std::vector<XtPresetWave> waves_;
};

#ifdef XT_FILES_MAIN
// xt_files' check of them (XtPresetWavesCheck.cpp): the library built from the disk image, every link file of
// Internal.prj against the wave it names, and each wave read whole; prints its line and gives how many came out wrong
int xtPresetWavesCheck(const XtDiskImage& disk, bool first);
#endif
