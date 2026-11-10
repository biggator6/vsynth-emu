// The firmware's disk image reader on a damaged image (.claude/fix-plan.md, section 9: XtDiskImage).  A FAT16 image is
// built here, field by field.  The good one lists and reads.  One whose root directory runs past its end is refused
// (the FAT comes before the root, so this covers the FAT too); read as it was, it read past the image's end.  One whose
// clusters have no sectors is refused; as it was, a folder on it whose FAT loops never ended.  A FAT that loops gives
// each cluster once; as it was, the file came back at whatever size its entry claimed (16 MB here, up to 4 GB).  A
// chain ends before a cluster the FAT has no entry for (added with the fix, so seen red only by its plant).
// Run: VSynthEmuDiskImageTest <rom.2.bin> (the image is unused).  Exit 0 if every check holds.
#include "TestChecks.h"
#include "TestMain.h"
#include "../Source/XtFiles.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <numeric>
#include <string>
#include <vector>

namespace {

constexpr size_t kSector = 512, kImage = 0x40000, kPartition = 32 * kSector;
constexpr uint32_t kFileBytes = 1000;   // A.BIN, on clusters 2 and 3

// What a case changes in the good image's boot sector, FAT and root
struct Layout {
    uint8_t sectorsPerCluster = 1;
    uint16_t fatSectors = 1;   // 1: 476 clusters, a FAT of 256 entries; 2: 474 clusters, 512 entries
    uint16_t rootEntries = 16;
    uint16_t afterFile = 0xffff;       // FAT entry 3, after A.BIN's second cluster: its end; 2 loops it back to its first
    uint16_t afterFolder = 0xffff;     // entry 4, after DIR's one cluster: its end; 4 loops it on itself
    uint32_t fileBytes = kFileBytes;   // A.BIN's size, as its entry says
};

void put16(std::vector<uint8_t>& d, size_t at, uint32_t v) {
    d[at] = static_cast<uint8_t>(v);
    d[at + 1] = static_cast<uint8_t>(v >> 8);
}

void put32(std::vector<uint8_t>& d, size_t at, uint32_t v) {
    put16(d, at, v & 0xffff);
    put16(d, at + 2, v >> 16);
}

// A short-named entry, its name and extension padded to 8 + 3
void entry(std::vector<uint8_t>& d, size_t at, const char* name83, uint8_t attributes, uint16_t first, uint32_t bytes) {
    std::memcpy(&d[at], name83, 11);
    d[at + 11] = attributes;
    put16(d, at + 26, first);
    put32(d, at + 28, bytes);
}

// The bytes A.BIN holds: each its offset's low byte
std::vector<uint8_t> fileBytes(size_t n) {
    std::vector<uint8_t> v(n);
    std::iota(v.begin(), v.end(), uint8_t { 0 });
    return v;
}

// A partition from sector 32: one reserved sector, two FATs, the root, then clusters of one sector each: A.BIN on 2
// and 3, the folder DIR on 4, holding B.BIN on 5
std::vector<uint8_t> image(const Layout& l) {
    std::vector<uint8_t> d(kImage, 0);
    put32(d, 446 + 8, kPartition / kSector);
    put32(d, 446 + 12, (kImage - kPartition) / kSector);
    const size_t boot = kPartition, fat = boot + kSector;
    const size_t root = fat + 2 * l.fatSectors * kSector, data = root + 16 * 32;
    put16(d, boot + 11, kSector);
    d[boot + 13] = l.sectorsPerCluster;
    put16(d, boot + 14, 1);
    d[boot + 16] = 2;
    put16(d, boot + 17, l.rootEntries);
    put16(d, boot + 22, l.fatSectors);
    std::memcpy(&d[boot + 54], "FAT16", 5);
    const uint16_t links[] = { 0xfff8, 0xffff, 3, l.afterFile, l.afterFolder, 0xffff };
    for (size_t c = 0; c < std::size(links); ++c) put16(d, fat + 2 * c, links[c]);
    entry(d, root, "A       BIN", 0x20, 2, l.fileBytes);
    entry(d, root + 32, "DIR        ", 0x10, 4, 0);
    const auto clusterAt = [data](size_t c) { return data + (c - 2) * kSector; };
    const std::vector<uint8_t> file = fileBytes(2 * kSector);
    std::ranges::copy(file, d.begin() + static_cast<long>(clusterAt(2)));
    entry(d, clusterAt(4), "B       BIN", 0x20, 5, 10);
    return d;
}

// The case's image as a file in `work`, its path; a file that can't be written fails the run
std::string written(TestChecks& check, const juce::File& work, const std::string& name, const Layout& l) {
    const std::vector<uint8_t> d = image(l);
    const juce::File file = work.getChildFile(name + ".bin");
    check(file.replaceWithData(d.data(), d.size()), "can't write " + name);
    return file.getFullPathName().toStdString();
}

// Images whose chain for A.BIN goes wrong after its two clusters: each should give those two and no more
struct Ending {
    const char* name;
    Layout layout;
    const char* what;
};
const Ending kEndings[] = {
    { "loop", { .afterFile = 2, .fileBytes = 16u << 20 },
      "a file whose FAT loops should give each of its clusters once" },
    { "past_fat", { .afterFile = 300, .fileBytes = 4 * kSector },
      "a chain should end before a cluster its FAT has no entry for" },
    { "past_image", { .fatSectors = 2, .afterFile = 500, .fileBytes = 4 * kSector },
      "a chain should end before a cluster past the image's end" },
};

// A.BIN's bytes, read through the FAT; nothing if it isn't listed
std::vector<uint8_t> readFile(const XtDiskImage& disk) {
    const auto top = disk.list("");
    const auto a = std::ranges::find(top, std::string("A.BIN"), &XtDiskImage::Entry::name);
    return a == top.end() ? std::vector<uint8_t> {} : disk.read(*a);
}

int diskImageTest(const juce::File&, const juce::File& work) {
    TestChecks check { "disk image" };
    XtDiskImage disk;
    const bool good = disk.open(written(check, work, "good", {}));
    const auto inFolder = disk.list("DIR");
    check(good && disk.list("").size() == 2 && inFolder.size() == 1 && inFolder.front().name == "B.BIN",
          "the good image should list A.BIN and DIR, and B.BIN in DIR");
    check(readFile(disk) == fileBytes(kFileBytes), "the good image's A.BIN should read as written");

    check(!disk.open(written(check, work, "root_past_end", { .rootEntries = 0xffff })),
          "an image whose root directory runs past its end should be refused");

    for (const Ending& e : kEndings) {
        const bool opened = disk.open(written(check, work, e.name, e.layout));
        const std::vector<uint8_t> got = readFile(disk);
        check(opened && got == fileBytes(2 * kSector),
              std::string(e.what) + ": " + std::to_string(got.size()) + " bytes");
    }

    // Last: as the reader was, the folder's walk doesn't come back
    check(!disk.open(written(check, work, "no_sectors", { .sectorsPerCluster = 0, .afterFolder = 4 })),
          "an image whose clusters have no sectors should be refused");
    const auto walked = check.within(std::chrono::seconds(3), "a folder whose FAT loops on clusters of no sectors "
                                     "should end at once", [&disk] { return disk.list("DIR"); });
    check(walked.empty(), "a refused image should list nothing");
    return check.summary();
}

} // namespace

int main(int argc, char** argv) {
    return testMain(argc, argv, "disk_image", diskImageTest);
}
