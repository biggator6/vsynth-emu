#pragma once
// rom.4's file calls (v500), which the XT's OS answers and the VC-2's wave loaders make: the index's parse
// (Vc2WaveIndex) opens "D:\VC-2 Wave.bin", reads, seeks and closes through them.  The loaders' ports make the same calls
// in the same order, so whatever answers them (the plugin's files, or a check's, which answers rom.4's own calls the
// same way) gives both the same bytes.  HARDWARE_FIRMWARE.md, "The VC-2's wave index, parsed".
#include <cstdint>
#include <string>

class Vc2Files {
public:
    virtual ~Vc2Files() = default;
    // 0x89336c60(path, 0): a handle, negative when the file isn't there
    virtual int32_t open(const std::string& path) = 0;
    // 0x89336cf0(handle, into, n): how many bytes it read into `into`, at most n
    virtual uint32_t read(int32_t handle, uint8_t* into, uint32_t n) = 0;
    // 0x89336db0(handle, offset, whence): from the file's start for whence 0, else from where it is
    virtual int32_t seek(int32_t handle, int32_t offset, int32_t whence) = 0;
    // 0x89336ca0(handle)
    virtual void close(int32_t handle) = 0;
};
