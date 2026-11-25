// The part's voices' constants from rom.4: see Vc2PartVoicesTables.h (v272).  Every address is rom.4's.
#include "Vc2PartVoicesTables.h"
#include "Vc2RomTables.h"
#include <algorithm>

namespace {
constexpr uint32_t kVoicesAt = 0x895354b0, kZeroAgeAt = 0x89535510;   // [FW] the voices' table, the zero age
constexpr uint32_t kRetuneWordAt = 0x8953fb2c;                         // [FW] the note voice's retune's word (v345)

// [FW] the budget's setters', the lists' init's and the static initializer's pool words
auto poolWords(Vc2PartVoicesTables& t) {
    return std::to_array<Vc2PoolWord>({
        { 0x89391370, &t.perBudget }, { 0x89391378, &t.perOffset }, { 0x8939138c, &t.budget },
        { 0x893914b4, &t.unread[0] }, { 0x893914a4, &t.unread[1] }, { 0x893914ac, &t.unread[2] },
    });
}

struct Read : Vc2PartVoicesTables {
    // The table, the word and the pool words; usable when every voice has an address
    bool read(Sh4& image) {
        for (size_t i = 0; i < voices.size(); ++i) voices[i] = image.rd32(kVoicesAt + 4 * (uint32_t)i);
        zeroAge = image.rd32(kZeroAgeAt);
        retuneWord = image.rd32(kRetuneWordAt);
        vc2ReadPool(image, poolWords(*this));
        return std::ranges::none_of(voices, [](uint32_t v) { return v == 0; });
    }
};
constinit Vc2RomTables<Read> tables;
} // namespace

bool vc2LoadPartVoices(const char* path) { return tables.load(path); }
bool vc2PartVoicesLoaded() { return tables.loaded(); }
const Vc2PartVoicesTables& vc2PartVoicesTables() { return tables.t; }
