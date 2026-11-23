// What rom.4's boot loaders read through the OS (Vc2BootFiles.h, v503).
#include "Vc2BootFiles.h"
#include <algorithm>
#include <cctype>
#include <string_view>

using xtfile::le32;

namespace {
// [COR] the index: a WLF08 library, its first version (16 characters from +0x10) naming VC2W, its sections' entries
// 16 bytes from +0x40 (+4 the place, +8 the size), their count at +8 (wlflib.py)
constexpr std::string_view kLibrary = "WLF08", kVc2Index = "VC2W";
constexpr size_t kVersionAt = 0x10, kVersionChars = 16, kCountAt = 8, kSectionsAt = 0x40, kSectionBytes = 16;
// [FW] the bank: '"TABLE"', a count of 4 ASCII digits (at most 1024, the loader's), 60-byte entries (a 52-byte name,
// the stream's size and its place from the streams' base), '"DATA"', the streams (0x89350970; wlfbank.py)
constexpr std::string_view kTable = "\"TABLE\"";
constexpr size_t kDigits = 4, kEntryBytes = 60, kSizeAt = 52, kPlaceAt = 56, kDataMark = 6;
constexpr int kMostEntries = 1024;
constexpr std::string_view kDriveA = "A:\\", kDriveD = "D:\\";

bool sameName(std::string_view a, std::string_view b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
               return std::tolower((unsigned char)x) == std::tolower((unsigned char)y);
           });
}

// The part of `path` past its drive, when it is on `drive` ("A:\"); false otherwise
bool onDrive(const std::string& path, std::string_view drive, std::string& rest) {
    if (path.size() < drive.size() || !sameName(std::string_view(path).substr(0, drive.size()), drive)) return false;
    rest = path.substr(drive.size());
    return true;
}

// The library past `from`: from its header to the end of its last section, as the parse reads the file
std::vector<uint8_t> indexIn(const std::vector<uint8_t>& d, size_t from) {
    const std::string_view all(reinterpret_cast<const char*>(d.data()), d.size());
    for (size_t at = all.find(kLibrary, from); at != std::string_view::npos; at = all.find(kLibrary, at + 1)) {
        if (at + kSectionsAt > d.size()) break;
        if (all.substr(at + kVersionAt, kVersionChars).find(kVc2Index) == std::string_view::npos) continue;
        size_t end = kSectionsAt;
        for (uint32_t s = 0; s < le32(&d[at + kCountAt]); ++s) {
            const size_t entry = at + kSectionsAt + kSectionBytes * s;
            if (entry + kSectionBytes > d.size()) return {};
            end = std::max<size_t>(end, (size_t)le32(&d[entry + 4]) + le32(&d[entry + 8]));
        }
        if (end > d.size() - at) return {};
        return { d.begin() + (long)at, d.begin() + (long)(at + end) };
    }
    return {};
}

// The bank past `from`: from '"TABLE"' to the end of its last stream (wlfbank.py's bank_bytes)
std::vector<uint8_t> bankIn(const std::vector<uint8_t>& d, size_t from) {
    const std::string_view all(reinterpret_cast<const char*>(d.data()), d.size());
    const size_t at = all.find(kTable, from);
    const size_t first = at + kTable.size() + kDigits;
    if (at == std::string_view::npos || first > d.size()) return {};
    int count = 0;
    for (const char c : all.substr(at + kTable.size(), kDigits)) {
        if (!std::isdigit((unsigned char)c)) return {};
        count = 10 * count + (c - '0');
    }
    const size_t base = first + kEntryBytes * (size_t)count + kDataMark;
    if (count > kMostEntries || base > d.size()) return {};
    size_t end = 0;
    for (int k = 0; k < count; ++k) {
        const size_t entry = first + kEntryBytes * (size_t)k;
        end = std::max<size_t>(end, (size_t)le32(&d[entry + kPlaceAt]) + le32(&d[entry + kSizeAt]));
    }
    if (end > d.size() - base) return {};
    return { d.begin() + (long)at, d.begin() + (long)(base + end) };
}
} // namespace

Vc2DiskBootFiles::Vc2DiskBootFiles(const XtDiskImage& disk)
    : disk_(disk), index_(indexIn(disk.bytes(), disk.pastPartition())),
      bank_(bankIn(disk.bytes(), disk.pastPartition())) {}

bool Vc2DiskBootFiles::onA(const std::string& path, std::string& at, XtDiskImage::Entry& entry) const {
    at.clear();
    entry = {};
    entry.isDir = true;   // the root
    for (size_t from = 0; from < path.size();) {
        const size_t end = std::min(path.find('\\', from), path.size());
        const std::string step = path.substr(from, end - from);
        from = end + 1;
        if (step.empty()) continue;   // two '\' together, or one first
        if (!entry.isDir) return false;
        const std::vector<XtDiskImage::Entry> here = disk_.list(at);
        const auto found = std::ranges::find_if(here, [&step](const auto& e) { return sameName(e.name, step); });
        if (found == here.end()) return false;
        entry = *found;
        at = at.empty() ? found->name : at + "/" + found->name;
    }
    return true;
}

bool Vc2DiskBootFiles::file(const std::string& path, std::vector<uint8_t>& out) const {
    const std::pair<std::string_view, const std::vector<uint8_t>*> onD[] = { { kIndexPath, &index_ },
                                                                            { kBankPath, &bank_ } };
    for (const auto& [name, bytes] : onD) {
        if (!sameName(path, name)) continue;
        out = *bytes;
        return !bytes->empty();
    }
    std::string rest, at;
    XtDiskImage::Entry entry;
    if (!onDrive(path, kDriveA, rest) || !onA(rest, at, entry) || entry.isDir) return false;
    out = disk_.read(entry);
    return true;
}

bool Vc2DiskBootFiles::folder(const std::string& path, std::vector<Vc2BootEntry>& out) const {
    out.clear();
    std::string rest, at;
    if (onDrive(path, kDriveD, rest) && rest.empty()) {   // the preset area: its two files, no 8.3 names or attributes
        const std::pair<std::string_view, const std::vector<uint8_t>*> onD[] = { { kIndexPath, &index_ },
                                                                                { kBankPath, &bank_ } };
        for (const auto& [name, bytes] : onD)
            if (!bytes->empty())
                out.push_back({ std::string(name.substr(kDriveD.size())), {}, 0, (uint32_t)bytes->size() });
        return true;
    }
    XtDiskImage::Entry entry;
    if (!onDrive(path, kDriveA, rest) || !onA(rest, at, entry) || !entry.isDir) return false;
    for (const XtDiskImage::Entry& e : disk_.list(at)) out.push_back({ e.name, e.shortName, e.attributes, e.size });
    return true;
}
