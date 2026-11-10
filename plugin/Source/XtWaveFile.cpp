// A V-Synth wave's chunks (XtFiles.h): a .vpw, a project's .wav (the same chunks in a RIFF), or a preset wave in the
// firmware's disk image.  Moved here from XtFiles.cpp in v169, split by what each part reads.  HARDWARE_FIRMWARE.md,
// "V-Synth's files".
#include "XtFiles.h"
#include "XtFirmware.h"
#include <algorithm>
#include <array>
#include <cstring>

using xtfile::fail;
using xtfile::le32;

namespace {

// The chunks the reader takes, each by its place here
constexpr std::array<const char*, 6> kIds = { "vpfl", "vpsc", "vplp", "vpno", "vpdt", "data" };
enum ChunkAt : size_t { Fl, Sc, Lp, No, Dt, Data };
struct Chunks {
    std::array<const uint8_t*, kIds.size()> at {};
    std::array<uint32_t, kIds.size()> bytes {};
};

// Where a type's events sit in its vpdt, in halfwords: a SOLO wave's every 10 (16 stereo) from 8, a band wave's every
// 0x2e (0x58 stereo) from 14.  A LITE wave (0) has none.
struct VpdtLayout { uint32_t mono, stereo, first; };
constexpr VpdtLayout kSolo { 10, 16, 8 }, kBand { 0x2e, 0x58, 14 };
constexpr std::array<const VpdtLayout*, 6> kLayouts = { nullptr, &kSolo, nullptr, &kBand, nullptr, &kBand };

// The chunks up to the data, each where it last came, and the stream's end; false, and why, if one runs past the end.
bool findChunks(const uint8_t* file, size_t size, Chunks& c, size_t& end, std::string* why) {
    // A project's .wav is a RIFF round the same chunks
    size_t i = size >= 12 && !std::memcmp(file, "RIFF", 4) && !std::memcmp(file + 8, "WAVE", 4) ? 12 : 0;
    while (i + 8 <= size && !c.at[Data]) {
        const uint32_t n = le32(file + i + 4);
        if (n > size - i - 8) return fail(why, "a chunk runs past the end");
        const auto id = std::ranges::find_if(kIds, [&](const char* k) { return !std::memcmp(file + i, k, 4); });
        if (id != kIds.end()) {
            c.at[(size_t)(id - kIds.begin())] = file + i + 8;
            c.bytes[(size_t)(id - kIds.begin())] = n;
        }
        i += 8 + (size_t)n + (n & 1);
    }
    end = i < size ? i : size;
    return true;
}

// What the sample chunk (vpsc) says of the wave
void readHeader(const uint8_t* vpsc, uint32_t nSc, XtWaveFile& out) {
    out.encodeType = vpsc[1];
    out.channels = vpsc[2] ? 2 : 1;
    out.frames = (long)le32(vpsc + 4);
    if (nSc >= 32) {
        out.originalKey = (int8_t)vpsc[0xa];
        out.headerCents = (int32_t)le32(vpsc + 0x18);
        out.tempo = (int32_t)le32(vpsc + 0x1c);
    }
}

// The loop record (vplp); with none, the whole wave
void readLoops(const uint8_t* vplp, uint32_t nLp, XtWaveFile& out) {
    out.loopEnd = out.bwdEnd = out.frames - 1;   // a wave with no loop record: all of it
    if (!vplp || nLp < 28) return;
    out.tune = (int32_t)le32(vplp);
    out.loopStart = (long)le32(vplp + 4);
    out.loopEnd = (long)le32(vplp + 8);
    out.bwdEnd = (long)le32(vplp + 0xc);
    out.bwdStart = (long)le32(vplp + 0x10);
    out.loopRateFwd = (int16_t)(vplp[0x14] | (vplp[0x15] << 8));
    out.loopRateBwd = (int16_t)(vplp[0x16] | (vplp[0x17] << 8));
    out.fwdMode = vplp[0x18];
    out.bwdMode = vplp[0x19];
    out.loopSwitchDecides = vplp[0x1a] != 0;
    out.tuneFine = (int8_t)vplp[0x1b];
}

// The events: vpno's list of them, and their positions in the vpdt, kept whole (the plugin can play it as it is)
void readEvents(const Chunks& c, XtWaveFile& out) {
    const uint8_t* vpno = c.at[No];
    const uint32_t nNo = c.bytes[No];
    if (vpno && nNo >= 12) {
        out.vpnoCount = (int)le32(vpno + 4);
        for (uint32_t k = 12; k + 2 <= nNo; k += 2) out.vpno.push_back((uint16_t)(vpno[k] | vpno[k + 1] << 8));
    }
    const uint8_t* vpdt = c.at[Dt];
    const uint32_t nDt = c.bytes[Dt];
    const VpdtLayout* layout = (size_t)out.encodeType < kLayouts.size() ? kLayouts[(size_t)out.encodeType] : nullptr;
    if (!vpdt || nDt < 12 || !layout) return;
    out.vpdt.assign(vpdt, vpdt + nDt);
    const uint32_t nev = le32(vpdt + 8) >> 16;
    const uint32_t stride = out.channels == 2 ? layout->stereo : layout->mono, first = layout->first;
    for (uint32_t e = 0; e < nev && e < 4096; ++e) {
        const uint32_t at = (stride * e + first) * 2 + 4;
        if (at + 4 > nDt) {
            out.events.clear();
            break;
        }
        out.events.push_back((int32_t)le32(vpdt + at));
    }
}

// The samples: as they are, or RDAC through the firmware's block routine.  What the header says is held to what the data
// holds, two bytes a sample as they are and one after RDAC's 8-byte header: every real wave has at least that (v169: 745
// of them, the disk image's presets and two projects' waves), and the plugin's buffers count samples in an int.
bool readSamples(const Chunks& c, bool linear, XtWaveFile& out, std::string* why) {
    const uint8_t* data = c.at[Data];
    const uint32_t nData = c.bytes[Data];
    const size_t want = (size_t)out.frames * (size_t)out.channels;
    if (want > (size_t)INT32_MAX) return fail(why, "more samples than a wave can have");
    if ((linear ? nData / 2 : nData - 8) < want) return fail(why, "fewer samples than the header says");
    if (linear) {
        out.pcm.resize(want);
        for (size_t k = 0; k < want; ++k) out.pcm[k] = (int16_t)(data[2 * k] | data[2 * k + 1] << 8);
        return true;
    }
    if (!xtFwRdacDecode(data + 8, nData - 8, out.frames, out.channels, out.pcm))
        return fail(why, "the firmware's decoder failed");
    return true;
}

} // namespace

bool xtWaveRead(const uint8_t* file, size_t size, XtWaveFile& out, std::string* why) {
    out = {};
    Chunks c;
    if (!findChunks(file, size, c, out.size, why)) return false;
    const uint8_t* vpfl = c.at[Fl];
    const uint32_t nFl = c.bytes[Fl];
    const uint8_t* vpsc = c.at[Sc];
    const uint32_t nSc = c.bytes[Sc];
    const uint8_t* data = c.at[Data];
    const uint32_t nData = c.bytes[Data];
    // A project's LITE wave is a RIFF with vpsc and vplp but no vpfl, and its samples as they are (Emu_test's 005, 348)
    const bool riff = size >= 12 && !std::memcmp(file, "RIFF", 4);
    if (!vpsc || nSc < 8 || !data || (!vpfl && !riff) || (vpfl && nFl < 44)) return fail(why, "not a V-Synth wave");
    const bool linear = !vpfl || !std::memcmp(vpfl + 40, "LINR", 4);
    if (!linear && std::memcmp(vpfl + 40, "RDAC", 4)) return fail(why, "neither RDAC nor plain samples");
    // 0x8952ee80 checks the data's first two bytes; the next six are nothing the firmware reads.
    if (!linear && (nData < 8 || data[0] != 0 || data[1] != 8)) return fail(why, "an RDAC wave without its header");
    readHeader(vpsc, nSc, out);
    readLoops(c.at[Lp], c.bytes[Lp], out);
    readEvents(c, out);
    return readSamples(c, linear, out, why);
}
