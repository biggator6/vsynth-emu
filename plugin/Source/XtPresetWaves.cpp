// The V-Synth's preset waves in the disk image, and a project's link to one: see XtPresetWaves.h (v600).
#include "XtPresetWaves.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <iterator>
#include <string_view>

using xtfile::fail;
using xtfile::le32;

namespace {
constexpr size_t kSector = 512;
// [COR] a link file: "link", its 4 bytes, the number; "name", its 16, the name (all 282 of the image's .w00 and .w02,
// and Altered States' 30)
constexpr size_t kLinkBytes = 36, kLinkChunk = 4, kLinkNumberAt = 8, kNameTagAt = 12, kNameAt = 20, kNameChars = 16;
constexpr uint32_t kLastNumber = 0xffff;   // a library's numbers are halfwords
// [COR] the library, as research/tools/wlflib.py reads both of the image's and rom.4's parser (0x8934d120) takes the
// VC-2's: "WLF08", the section count at +8, the first version's 16 characters at +0x10, and a 16-byte entry a section
// from +0x40 (u16 number, u16 1, u32 offset from the library, u32 size).  rom.2's own readers (0x8949a088…) aren't run.
constexpr std::string_view kLibraryTag = "WLF08", kVSynthVersion = "VSYN2W";
constexpr size_t kCountAt = 8, kVersionAt = 0x10, kVersionChars = 16, kSectionsAt = 0x40, kSectionEntry = 16;
// A section: u32 count, u32 where its index or its entries start, u32 an entry's size (8: an index of offset and size
// pairs, each offset from the section)
constexpr size_t kSectionHead = 12, kPairBytes = 8;
// A wave's record: its name, then a 4-byte split each (upper velocity, a byte, u16 map number); a map's: its name, then a
// 6-byte split each (upper key, level, a u16, u16 sample number); a sample's: a 32-byte path, then its kind byte
constexpr size_t kWaveSplit = 4, kWaveMapAt = 2, kMapSplit = 6, kMapSampleAt = 4, kPathBytes = 32;
enum Section : size_t { Waves, Maps, Samples, kSections };

uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }

// `n` characters, cut at a 0, trailing spaces gone
std::string trimmed(const uint8_t* p, size_t n) {
    std::string s(reinterpret_cast<const char*>(p), n);
    s.erase(std::find(s.begin(), s.end(), '\0'), s.end());
    s.erase(s.find_last_not_of(' ') + 1);
    return s;
}

struct Record {
    const uint8_t* p = nullptr;
    size_t n = 0;
};
using Records = std::vector<Record>;

// Room for `n` bytes at `at`
bool fits(const std::vector<uint8_t>& d, size_t at, size_t n) { return at <= d.size() && d.size() - at >= n; }

// A section's records, by its index of pairs or inline; false if one runs past the image
bool records(const std::vector<uint8_t>& d, size_t at, Records& out) {
    out.clear();
    if (!fits(d, at, kSectionHead)) return false;
    const uint32_t count = le32(&d[at]), start = le32(&d[at + 4]), size = le32(&d[at + 8]);
    if (count > d.size()) return false;
    for (uint32_t k = 0; k < count; ++k) {
        size_t o = at + start + static_cast<size_t>(size) * k, n = size;
        if (size == kPairBytes) {
            const size_t pair = at + start + kPairBytes * k;
            if (!fits(d, pair, kPairBytes)) return false;
            o = at + le32(&d[pair]);
            n = le32(&d[pair + 4]);
        }
        if (!fits(d, o, n)) return false;
        out.push_back({ &d[o], n });
    }
    return true;
}

// The V-Synth's library past the partition, its sections by their numbers as rom.4's parser takes the VC-2's (a number
// past 3, or an entry whose second halfword isn't 1, passed over); false with none, or one that runs past the image
bool library(const std::vector<uint8_t>& d, size_t from, std::array<Records, kSections>& out) {
    if (from > d.size()) return false;
    const auto search = [&d](auto it) { return std::search(it, d.end(), kLibraryTag.begin(), kLibraryTag.end()); };
    const auto first = std::next(d.begin(), static_cast<std::ptrdiff_t>(from));
    for (auto it = search(first); it != d.end(); it = search(it + 1)) {
        const auto at = static_cast<size_t>(it - d.begin());
        if (!fits(d, at, kSectionsAt)) continue;
        const std::string_view version(reinterpret_cast<const char*>(&d[at + kVersionAt]), kVersionChars);
        if (version.find(kVSynthVersion) == std::string_view::npos) continue;
        const uint32_t sections = le32(&d[at + kCountAt]);
        for (uint32_t s = 0; s < sections; ++s) {
            const size_t e = at + kSectionsAt + kSectionEntry * s;
            if (!fits(d, e, kSectionEntry)) return false;
            const uint16_t number = le16(&d[e]);
            if (number < 1 || number > kSections || le16(&d[e + 2]) != 1) continue;
            if (!records(d, at + le32(&d[e + 4]), out[number - 1u])) return false;
        }
        return true;
    }
    return false;
}
} // namespace

std::optional<XtWaveLink> xtWaveLinkRead(const uint8_t* file, size_t size, std::string* why) {
    if (size < kLinkBytes || std::memcmp(file, "link", 4) || le32(file + 4) != kLinkChunk
        || std::memcmp(file + kNameTagAt, "name", 4) || le32(file + kNameTagAt + 4) != kNameChars) {
        fail(why, "not a wave link");
        return std::nullopt;
    }
    const uint32_t number = le32(file + kLinkNumberAt);
    if (number < 1 || number > kLastNumber) {
        fail(why, "a wave link with no wave number");
        return std::nullopt;
    }
    return XtWaveLink { static_cast<int>(number), trimmed(file + kNameAt, kNameChars) };
}

bool XtPresetWaves::build(const XtDiskImage& disk, std::string* why) {
    waves_.clear();
    const std::vector<uint8_t>& d = disk.bytes();
    std::array<Records, kSections> s;
    if (!library(d, disk.pastPartition(), s))
        return fail(why, "no V-Synth wave library past the disk image's partition");
    const std::vector<size_t> streams = disk.presetWaves();   // every stream past the partition, in order
    std::vector<size_t> onSector;
    std::ranges::copy_if(streams, std::back_inserter(onSector), [](size_t at) { return at % kSector == 0; });
    std::vector<XtPresetWave> built;
    for (const Record& w : s[Waves]) {
        if (w.n != kNameChars + kWaveSplit) return fail(why, "a preset wave of more than one split");
        const size_t map = le16(w.p + kNameChars + kWaveMapAt);
        if (map < 1 || map > s[Maps].size()) return fail(why, "a preset wave's map outside the library");
        const Record& m = s[Maps][map - 1];
        if (m.n != kNameChars + kMapSplit) return fail(why, "a preset map of more than one split");
        const size_t sample = le16(m.p + kNameChars + kMapSampleAt);
        if (sample < 1 || sample > s[Samples].size() || sample > onSector.size())
            return fail(why, "a preset map's sample outside the library or past the streams");
        const Record& path = s[Samples][sample - 1];
        if (path.n <= kPathBytes) return fail(why, "a preset sample without its kind byte");
        const size_t at = onSector[sample - 1];
        const auto next = std::ranges::upper_bound(streams, at);
        const size_t bytes = (next == streams.end() ? d.size() : *next) - at;
        const int kind = path.p[kPathBytes];
        if (xtWaveEncodeType(&d[at], bytes) != kind)
            return fail(why, "a preset stream whose type isn't its sample's kind");
        built.push_back({ trimmed(w.p, kNameChars), kind, at, bytes });
    }
    if (built.empty()) return fail(why, "a V-Synth wave library with no waves");
    waves_ = std::move(built);
    return true;
}

const XtPresetWave* XtPresetWaves::wave(int number) const {
    return number >= 1 && number <= count() ? &waves_[static_cast<size_t>(number - 1)] : nullptr;
}
