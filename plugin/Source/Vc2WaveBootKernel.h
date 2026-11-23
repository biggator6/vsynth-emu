#pragma once
// The kernel's part of rom.4's boot load (Vc2WaveBoot.h, v503): the entries at 0x88000000… that rom.4's loaders reach
// through its stubs, answered from a Vc2BootFiles.  What each gives is read from rom.4's own use of it and from its own
// Z: drive, which answers the same calls in rom.4's code (0x8933c4e0, 0x8933c5d0, 0x8933cad0, …):
// - files: open (path, mode) a handle or −1, read, seek (0 back), close, fstat (the size at +0), and a file's bytes in
//   memory (the preset area is read as memory: the bank loader's +4);
// - folders: open a handle or 0, read an entry (+0 its place, which seek and the stat by entry take; +8 the name's
//   length; +10 the name; +0x10a the 8.3 name), rewind, seek, close, and the stat by entry (+0 the size, +8 the FAT
//   attributes);
// - the drive's five queries, as rom.4's Z: answers them (0x89339040 0, 0x89338e50 1, 0x89338e90 0, 0x89338ed0 1,
//   0x89338f10 1);
// - the string compares, without case (their case isn't known: the link files' names differ in their first three
//   digits, so the order is the same either way), and a path's file name, the part past its last '\';
// - the allocator's lock and unlock, the display's text, and the boot's key test (0: the normal boot, which reads
//   "C:\VC-2 Project.ini"), each 0.
// A write (an open of any other mode) is refused and named: the loaders only write the link files when their folder
// is missing (0x8934db30), which no image the load is given lacks.
#include "Vc2WaveBoot.h"
#include <map>
#include <vector>

struct Vc2WaveBoot::Kernel {
    explicit Kernel(const Vc2BootFiles& f) : files(f) {}
    // Every entry the load answers, a hook each on the machine
    void serve(Sh4& c);
    static bool answers(uint32_t at);

    struct File {
        std::vector<uint8_t> bytes;
        uint32_t at = 0, mapped = 0;
    };
    struct Folder {
        std::vector<Vc2BootEntry> entries;
        uint32_t at = 0;
    };
    // Where a file's bytes go when rom.4 reads it as memory: past the heap, which ends at 0x8ffffffc
    static constexpr uint32_t kMappedFrom = 0x90000000;
    // The kernel's entry a folder's read gives, in the machine's RAM below the heap
    static constexpr uint32_t kEntryAt = 0x8C0F0000, kEntryBytes = 0x118;
    const Vc2BootFiles& files;
    std::map<uint32_t, File> openFiles;
    std::map<uint32_t, Folder> folders;
    uint32_t nextFile = 1, nextFolder = 1, nextMapped = kMappedFrom;
    std::string refused;   // a call the load refused, said when the stage ends

    void open(Sh4& c);
    void close(Sh4& c);
    void read(Sh4& c);
    void seek(Sh4& c);
    void fstat(Sh4& c);
    void mapped(Sh4& c);
    void openFolder(Sh4& c);
    void closeFolder(Sh4& c);
    void readFolder(Sh4& c);
    void rewindFolder(Sh4& c);
    void seekFolder(Sh4& c);
    void statEntry(Sh4& c);
    void compare(Sh4& c);
    void compareSome(Sh4& c);
    void fileName(Sh4& c);
};
