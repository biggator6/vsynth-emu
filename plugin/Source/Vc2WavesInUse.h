#pragma once
// The VC-2's waves the process plays from (v504): rom.4's own load of them (Vc2WaveBoot, v503) on a disk image, once a
// process, as rom.4's tables are (XtSynthRom.cpp), and at the plugin's start beside them, so a switch of modes finds
// them in (the user's decision, 2026-11-14).  A disk image is taken on the thread that offers it: read, and the VC-2's
// index and sample bank found in it (Vc2DiskBootFiles).  The load then runs on a thread of its own, about 2 s, so the
// start isn't held up, and the waves are published whole by an atomic pointer.  The process's end stops the load
// (Sh4StopScope) and waits for it, which takes milliseconds.  PluginRom.cpp offers the disk image it finds beside rom.4
// (the user's choice, 2026-11-23: V-SynthXTDiskImage.bin.0.bin, where the firmware update's unpacker leaves it).
// HARDWARE_FIRMWARE.md, "The VC-2's waves at the plugin's start".
#include "Vc2WaveBoot.h"
#include <string>

// Offers the disk image at `path`, with rom.4 at `rom4`: true when it holds the VC-2's files and the load has started,
// or one already had (nothing is read again); false, `why` said, when it isn't a disk image holding them
bool vc2TakeWaves(const std::string& path, const std::string& rom4, std::string* why = nullptr);
// Whether a disk image has been taken
bool vc2WavesTaken();
// The waves, once their load is whole; none while it runs, with no disk image taken, or when it failed
const Vc2LoadedWaves* vc2Waves();
// What the load says: that no disk image has been taken, or the one it runs on and how it went
std::string vc2WavesStatus();
// Waits at most `seconds` for a load taken to end: whether none is running (the tests')
bool vc2WavesWait(double seconds);
