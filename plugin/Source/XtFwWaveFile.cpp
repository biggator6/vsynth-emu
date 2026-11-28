// A wave saved by rom.2's own writer: XtFwWaveFile.h (v482).
#include "XtFwWaveFile.h"
#include "XtFirmware.h"
#include "XtFwFileOut.h"
#include "XtFwMachine.h"
#include <array>
#include <memory>

using xtfile::fail;

namespace {
// [FW] the writer, (path, the wave's number), and the store's table of 128 lists it finds the wave through
// (0x8949feb0: the list by the number's low 7 bits, a node's next at +4 and number at +8, its entry 0x18 before it)
constexpr uint32_t kWriter = 0x89496590, kWaves = 0x89ce87fc;
constexpr uint32_t kNode = 0x18, kNumberAt = 8, kNumber = 0;
// [FW] where the entry points at each block, in the writer's order (vpfl at none: the reader's open builds it)
constexpr std::array<uint32_t, XtWaveBlocks::kCount> kPointerAt { 0, 0x24, 0x28, 0x2c, 0x30, 0x34, 0x40, 0x3c, 0x38 };
// [FW] a block's buffer: its size at +0x10 and its bytes from +0x1c (0x8949ea30, 0x8949e850)
constexpr uint32_t kSizeAt = 0x10, kBytesAt = 0x1c;
// [FW] the data's: its bytes from +0x40, its size 0x20 more than they are
constexpr uint32_t kDataAt = 0x40, kDataSizeMore = 0x20;
// [HW] the XT's word at address 0x10, in its boot flash: the vpfl's builder (0x894a06d0) reads each block's size
// through the entry's pointer unchecked, so for a block the wave has none of it reads this, 9 in every vpfl the XT
// wrote for a SOLO wave with no vmdl (the user's project, analysis/projects/Emu_test.prj)
constexpr uint32_t kLowPage = 0, kLowPageBytes = 0x1000, kLowWord = 0x10, kLowWordIs = 9;
// Where the harness puts them: the path, the entry, then each block's buffer, below the machine's heap
constexpr uint32_t kPath = xtfw::kSamp, kEntry = xtfw::kSamp + 0x100, kBuffers = xtfw::kSamp + 0x200;
constexpr char kPathText[] = "A:\\WAVE.WAV";

// Each block the wave has in its own buffer from kBuffers, the entry pointing at it; false if they don't fit
bool putBlocks(Sh4& c, const XtWaveBlocks& w) {
    uint32_t at = kBuffers;
    for (size_t k = XtWaveBlocks::kVmdl; k < XtWaveBlocks::kCount; ++k) {
        if (!w.bytes[k]) continue;
        const auto n = static_cast<uint32_t>(w.bytes[k]->size());
        const bool data = k == XtWaveBlocks::kData;
        const uint32_t from = data ? kDataAt : kBytesAt;
        if (n > xtfw::kHeap - at - from) return false;
        c.wr32(at + kSizeAt, data ? n + kDataSizeMore : n);
        c.write(at + from, w.bytes[k]->data(), n);
        c.wr32(kEntry + kPointerAt[k], at);
        at = (at + from + n + 15) & ~15u;
    }
    return true;
}
} // namespace

bool xtFwWaveFile(const XtWaveBlocks& w, std::vector<uint8_t>& file, std::string* why) {
    file.clear();
    const auto& vpsc = w.bytes[XtWaveBlocks::kVpsc];
    if (!vpsc || vpsc->size() < 8) return fail(why, "no vpsc");
    const int type = (*vpsc)[1];
    if (xtWaveHasBands(type)) return fail(why, "a band wave: its samples come through its bands");
    const size_t samples = w.bytes[XtWaveBlocks::kData] ? w.bytes[XtWaveBlocks::kData]->size() / 2 : 0;
    auto m = std::make_unique<xtfw::Machine>(xtFwRomPath(), samples);
    if (!m->ok) return fail(why, "no firmware image");
    Sh4& c = m->cpu;
    c.mapRam(kLowPage, kLowPageBytes);
    c.wr32(kLowWord, kLowWordIs);
    if (!putBlocks(c, w)) return fail(why, "the wave doesn't fit the machine");
    c.wr32(kEntry + kNode + kNumberAt, kNumber);   // its node alone in its list (no next)
    c.wr32(kWaves + 4 * (kNumber & 0x7f), kEntry + kNode);
    return xtfw::runWriter(c, { kWriter, kPath, kPathText, kNumber }, file, why);
}
