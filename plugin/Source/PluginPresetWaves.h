#pragma once
// The V-Synth's preset waves a project's link (.w00) reaches, once a process (v600): the update's disk image beside rom.2,
// then beside rom.4, where the unpacker leaves them, read the first time a link is followed with those images in, and
// only each wave's place kept (XtPresetWaves); a wave's stream then read from the image's file when a link asks for it.
// Any thread but the audio thread.
#include "XtFiles.h"
#include <string>

// Preset wave `number` (a link's) whole; false and why: no disk image with the V-Synth's library beside the firmware, no
// such wave, or its stream unread
bool presetWaveRead(int number, XtWaveFile& wave, std::string* why = nullptr);
