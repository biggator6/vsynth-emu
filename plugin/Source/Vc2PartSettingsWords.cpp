// What the part's settings' readers share: see Vc2PartSettingsWords.h (v288).
#include "Vc2PartSettingsWords.h"
#include <cstring>

namespace Vc2PartWords {
void put8(uint8_t* at, int32_t v) { *at = static_cast<uint8_t>(v); }

void put16(uint8_t* at, int32_t v) {
    const uint16_t h = static_cast<uint16_t>(v);
    std::memcpy(at, &h, sizeof h);
}

int32_t times(int32_t v, int32_t by) {
    return static_cast<int32_t>(static_cast<uint32_t>(v) * static_cast<uint32_t>(by));
}

bool everyRow(int rows, const std::function<bool(int, int)>& row) {
    for (int kind = 0; kind < kKinds; ++kind)
        for (int i = 0; i < rows; ++i)
            if (!row(kind, i)) return false;
    return true;
}
} // namespace Vc2PartWords
