#pragma once
// A converter's input as the SH-4 reads it (v251, from Vc2VocoderOsc.cpp's of v222): little-endian, a byte, a
// halfword or a word, signed or not by its load.  The VOCODER OSC's converter's own part and the OSC block's converter
// read their input so.
#include <cstdint>
#include <cstring>

namespace vc2in {
inline uint8_t u8(const uint8_t* in, int at) { return in[at]; }
inline int s8(const uint8_t* in, int at) { return (int8_t)in[at]; }
inline int s16(const uint8_t* in, int at) {
    int16_t v = 0;
    std::memcpy(&v, in + at, sizeof v);
    return v;
}
inline int32_t s32(const uint8_t* in, int at) {
    int32_t v = 0;
    std::memcpy(&v, in + at, sizeof v);
    return v;
}
} // namespace vc2in
