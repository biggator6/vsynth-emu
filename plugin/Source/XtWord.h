#pragma once
// A port's word and where it lies (v326): its place in the firmware's object, its member's offset in the port's object
// and its bytes.  A port that runs on another object's words (the VC-2's voice, Vc2OscBeat.cpp) gives a table of them.
#include <cstddef>
#include <cstdint>

struct XtWord {
    uint32_t at;
    size_t member, bytes;
};
