#pragma once
// What rom.4's boot loaders of the VC-2's waves read through the OS (v503): a file's bytes and a folder's entries by the
// path rom.4 gives ("D:\VC-2 Wave.bin", "A:\Internal.vc2\Wave\"), from whatever holds them.  Vc2WaveBoot answers the
// kernel's file and folder calls from one.  Vc2DiskBootFiles is the disk image's, the user's choice (2026-11-23,
// "Image, FAT links"): the preset drive D:, the XT's preset area past the partition, with its two files found as
// wlflib.py finds them (the WLF08 index whose first version names VC2W, the sample bank's '"TABLE"'), and drive A:
// through the FAT, its names matched without case as FAT matches them.  HARDWARE_FIRMWARE.md, "The VC-2's waves,
// loaded by rom.4 itself".
#include "XtFiles.h"
#include <cstdint>
#include <string>
#include <vector>

// A folder's entry as the kernel's directory calls give it: its long name, its 8.3 name, its FAT attributes (0x10 a
// folder) and its size
struct Vc2BootEntry {
    std::string name, shortName;
    uint8_t attributes = 0;
    uint32_t size = 0;
};

class Vc2BootFiles {
public:
    virtual ~Vc2BootFiles() = default;
    // A file's bytes by rom.4's path into `out`: false when there is none
    virtual bool file(const std::string& path, std::vector<uint8_t>& out) const = 0;
    // A folder's entries by rom.4's path, a '\' at its end or not: false when there is none
    virtual bool folder(const std::string& path, std::vector<Vc2BootEntry>& out) const = 0;
};

class Vc2DiskBootFiles : public Vc2BootFiles {
public:
    static constexpr const char* kIndexPath = "D:\\VC-2 Wave.bin";     // [FW] the index's parse's and start-up load's
    static constexpr const char* kBankPath = "D:\\VC-2 Preset.bin";    // [FW] the bank loader's
    // On an open disk image, which must outlive this
    explicit Vc2DiskBootFiles(const XtDiskImage& disk);
    bool file(const std::string& path, std::vector<uint8_t>& out) const override;
    bool folder(const std::string& path, std::vector<Vc2BootEntry>& out) const override;
    // The preset drive's two files as found past the partition: empty when the image has none
    const std::vector<uint8_t>& index() const { return index_; }
    const std::vector<uint8_t>& bank() const { return bank_; }

private:
    const XtDiskImage& disk_;
    std::vector<uint8_t> index_, bank_;
    // Drive A:'s entry at `path` (past "A:\"): its folder's path for XtDiskImage::list, and the entry; false when
    // there is none.  The root is a folder with no entry.
    bool onA(const std::string& path, std::string& folder, XtDiskImage::Entry& entry) const;
};
