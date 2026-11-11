#pragma once
// V-Synth's own files: a wave (.vpw, and the preset waves in the firmware's disk image) and a patch (.p00), read the way
// the firmware reads them, through its own routines in the interpreter (XtFirmware) and its descriptors from rom.2 at run
// time.  HARDWARE_FIRMWARE.md, "V-Synth's files".  Needs xtFwOpen.  Not for the audio thread.
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

// What the readers (XtFiles.cpp, XtWaveFile.cpp) share: a little-endian word, a big-endian one (a patch file's header), and
// a refusal with its reason.  And a file's digest, FNV-1a over its bytes, which xt_files hashes waves and patches by and
// XtSynthRom.cpp knows an image by.
namespace xtfile {
inline uint32_t le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
inline uint32_t be32(const uint8_t* p) { return (uint32_t)p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
inline bool fail(std::string* why, const char* what) {
    if (why) *why = what;
    return false;
}
inline uint64_t fnv(const void* p, size_t n, uint64_t h = 0xcbf29ce484222325ull) {
    const auto* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 0x100000001b3ull;
    }
    return h;
}
} // namespace xtfile

// ── A wave ───────────────────────────────────────────────────────────────────
struct XtWaveFile {
    int channels = 1;
    long frames = 0;
    int encodeType = 0;              // vpsc's byte 1: 0 LITE, 1 SOLO, 3 BACKING, 5 ENSEMBLE
    int originalKey = 60;            // vpsc +0xa
    int tempo = 1200000;             // vpsc +0x1c, its Original Tempo: BPM x 10000
    int headerCents = 0;             // vpsc +0x18, what the voice's fine tuning starts from
    long loopStart = 0, loopEnd = 0; // vplp +4, +8: the forward loop, in frames
    long bwdStart = 0, bwdEnd = 0;   // vplp +0x10, +0xc: the backward loop
    int fwdMode = 1, bwdMode = 1;    // vplp +0x18, +0x19: 0 One Shot, 1 One Way, 2 Alternate
    bool loopSwitchDecides = true;   // vplp +0x1a: clear, the wave loops whatever the OSC's Loop switch says (the converter's tail, 0x894e4030)
    int loopRateFwd = 0, loopRateBwd = 0;   // vplp +0x14, +0x16: xtWaveLoopRate, in tenths of a cent
    int tuneFine = 0, tune = 0;      // vplp +0x1b and +0, in 1/100 cent: xtWaveTune.  Real: +0 of 55534 is a sample 5.55 semitones above its Original Key
    // vpno: the events Playback Mode STEP and EVENT go between, by the wave's own event numbers (u16 from +0xc), and its count (+4).
    // −1 with no chunk, as on LITE waves.  An XT's own encode lists every event in order; a list edited on the instrument leaves some out.
    std::vector<uint16_t> vpno;
    int vpnoCount = -1;
    // vpdt: its events' positions (1/16 sample: the start, the attacks, the end), as the wave was encoded or its event points
    // edited (V-Producer can).  A SOLO wave's at (stride·e + 8)·2 + 4 (stride 10, 16 stereo), a band wave's at (stride·e + 14)·2 + 4
    // (0x2e, 0x58 stereo).  Empty with no chunk, as on LITE waves.
    std::vector<int32_t> events;
    // and the vpdt chunk itself, which the plugin plays as it is (xtSynthEncode's `stored`), as the XT plays a wave it loads
    std::vector<uint8_t> vpdt;
    std::vector<int16_t> pcm;        // interleaved, frames * channels
    size_t size = 0;                 // bytes of the stream, its data chunk's end
};
// A .vpw's chunks, or a preset wave's in the disk image, with its RDAC audio unpacked; or a project's .wav, the same chunks in a RIFF
// with the audio as plain samples.  False (and why) for anything else, a WAV with no vpsc among them.
bool xtWaveRead(const uint8_t* file, size_t size, XtWaveFile& out, std::string* why = nullptr);

// ── A patch ──────────────────────────────────────────────────────────────────
constexpr int kXtPatchBytes = 0x14fa;
struct XtPatchFile {
    std::string name;                // its 12 characters, trailing spaces gone
    std::vector<uint8_t> store;      // the patch as the firmware holds it, in the layout the descriptors go by
};
// A .p00 into the store: the firmware's defaults under the file's bytes (a shorter file is an older one), then its layout
// upgrade.  False (and why) if it isn't a patch.
bool xtPatchRead(const uint8_t* file, size_t size, XtPatchFile& out, std::string* why = nullptr);
// The name alone, without the firmware: for a list of hundreds.  Empty if it isn't a patch.
std::string xtPatchName(const uint8_t* file, size_t size);

// The store's layout and the descriptors' reads below are XtPatchStore.cpp's (v187).
// One parameter's place and range, by the firmware's ID, from rom.2.  nullptr past the table or without a ROM.
struct XtParamDesc { int offset, width, bit, centre, def, min, max; };
const XtParamDesc* xtParamDesc(int id);
constexpr int kXtDescIds = 0x55d;   // the first bank's IDs; the second (xtTypedDesc) goes on from there
// The store's sections, as 0x8947dae0 writes them: the IDs first..last from `at`, and where a section repeats 16 times
// (a zone's, or the arpeggio pattern's lines), `zoneStride` bytes apart.  The COSMs (xtPatchIsCosm) repeat twice over that.
struct XtPatchSectionDef { int first, last, at, zoneStride; };
std::span<const XtPatchSectionDef> xtPatchSections();
bool xtPatchIsCosm(int id);
// Where an ID's section starts in the store: `zone` 0..15 for what a zone has (or the arpeggio pattern's line), `cosm` 0..1
// for the COSMs.  -1 for an ID outside a patch.
int xtPatchSection(int id, int zone = 0, int cosm = 0);
// The bits as stored, and the firmware's read of them (0x89481860): the default if they're out of range, less the centre.
int xtPatchRaw(const std::vector<uint8_t>& store, int id, int zone = 0, int cosm = 0);
int xtPatchValue(const std::vector<uint8_t>& store, int id, int zone = 0, int cosm = 0);
// From ID 0x55d, a parameter that a block's type gives its slot's meaning, stored in the block's slots: `base` the ID the slots count from
// (0x897a1012), the firmware's read of one (0x89481d70): the slot's value, the default if it's outside the slot's range or this ID's.
constexpr int kXtMfxSlots = 0x17b, kXtChorusSlots = 0x1ff, kXtReverbSlots = 0x232, kXtCosmSlots = 0x3e9;
struct XtTypedDesc { int slot, def, min, max; };
const XtTypedDesc* xtTypedDesc(int id);
int xtPatchTyped(const std::vector<uint8_t>& store, int base, int id, int zone = 0, int cosm = 0);
// A type's IDs, first and last, from rom.2 (block 0 MFX, 1 Chorus, 2 Reverb, 3 COSM; the tables at 0x897a8218, 0x897a82e4, 0x897a8310, 0x897cebd8).  False past its types.
bool xtEffectTypeIds(int block, int type, int& first, int& last);
int xtEffectTypes(int block);
// How many zones it has: the first whose Range Upper is 127 ends them.
int xtPatchZones(const std::vector<uint8_t>& store);

// ── The firmware's disk image ────────────────────────────────────────────────
// V-SynthXTDiskImage.bin: a FAT16 partition from sector 32 (Internal.prj, Internal.vc2), then the preset waves, not in any file.
// XtDiskImage.cpp.
class XtDiskImage {
public:
    struct Entry { std::string name; bool isDir = false; uint32_t cluster = 0, size = 0; };
    // False for a file that isn't a FAT16 image, or a damaged one: clusters of no sectors, a root past its end.
    bool open(const std::string& path);
    const std::vector<uint8_t>& bytes() const { return d; }
    // The entries of a folder by its path ("Internal.prj/Patch"; "" the root), and a file's bytes through the FAT.
    std::vector<Entry> list(const std::string& folder) const;
    std::vector<uint8_t> read(const Entry& file) const;
    // Where the preset waves start in bytes(): every "vpfl" past the partition.
    std::vector<size_t> presetWaves() const;
private:
    std::vector<uint8_t> d;
    size_t fat = 0, root = 0, data = 0, partitionEnd = 0;
    uint32_t cluster = 0, rootBytes = 0;
    uint32_t clusterEnd = 0;   // the cluster numbers a chain can take: their FAT entries and data are in the image
    std::vector<uint8_t> chain(uint32_t first, uint32_t limit) const;
    std::vector<Entry> entries(const std::vector<uint8_t>& raw) const;
};

#ifdef XT_FILES_MAIN
// xt_files' patches (XtPatchStoreCheck.cpp; its main is XtFiles.cpp's).  The values of the disk image's patches against the
// firmware's reads, which prints the lines of a type's parameters and of the blocks the firmware points at by its own
// routines; the counts, which xtPatchCheckPrint prints; and an older, shorter patch, which prints its line and gives how
// many came out wrong.
struct XtPatchCheck {
    int patches = 0, sizes[4] = {}, named = 0;
    long values = 0, outOfRange = 0, compared = 0, different = 0;
    uint64_t hash = 0xcbf29ce484222325ull;
};
XtPatchCheck xtPatchCheckValues(const XtDiskImage& disk, bool first);
void xtPatchCheckPrint(const XtPatchCheck& c);
int xtPatchCheckShort(const XtDiskImage& disk);
// The preset wave at `at`, edited in its chunks, as a project's RIFF and LITE wave, and plain and short WAVs that aren't
// waves (XtWaveFileCheck.cpp): prints its line, and gives how many came out wrong
int xtWaveCheckOwn(const std::vector<uint8_t>& disk, size_t at);
#endif
