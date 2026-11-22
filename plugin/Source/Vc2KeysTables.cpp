// The part's keys' constants from rom.4: see Vc2KeysTables.h (v276).  Every address is rom.4's.
#include "Vc2KeysTables.h"
#include "Vc2RomTables.h"
#include <algorithm>

namespace {
constexpr uint32_t kDivisorAt = 0x89392400;   // [FW] the velocity factor's pool word

// A table read as it lies in rom.4's memory
struct Vc2KeysRead {
    uint32_t at;
    void* out;
    size_t bytes;
};

struct Read : Vc2KeysTables {
    // [FW] the tables, by address
    auto reads() {
        return std::to_array<Vc2KeysRead>({
            { 0x89535518, curves.data(), sizeof curves }, { 0x89535918, masks.data(), sizeof masks },
            { 0x89535938, bits.data(), sizeof bits },     { 0x89535958, zero.data(), sizeof zero },
            { 0x8950d6d9, sources.data(), sizeof sources }, { 0x8950d6e0, states.data(), sizeof states },
        });
    }
    // The tables and the word; usable when every bit is its own, each source's row is one read, and every state has an
    // address
    bool read(Sh4& image) {
        for (const Vc2KeysRead& r : reads()) image.read(r.at, r.out, r.bytes);
        vc2ReadPool(image, std::to_array<Vc2PoolWord>({ { kDivisorAt, &divisor } }));
        for (size_t n = 0; n < bits.size(); ++n)
            if (bits[n] != 1u << n) return false;
        const bool rows = std::ranges::all_of(sources, [](uint8_t row) { return row < kRows; });
        return rows && std::ranges::none_of(states, [](uint32_t s) { return s == 0; });
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadKeys(const char* path) { return tables.load(path); }
bool vc2KeysLoaded() { return tables.loaded(); }
const Vc2KeysTables& vc2KeysTables() { return tables.t; }
