#pragma once
// The VC-2 part's start-up (v304, step 4.3's thirty-second version, item 10's ninth part): rom.4's 0x89392ab0, which
// the notes' task runs first (0x8938fc2e): Time Trip's assigns and the two bytes after them −1, the routing object's
// (0x899893f8, the routing reset's and the receive channel's `this`) bytes and floats, the settings object's
// (0x899826fc) word its constructor 0x89391610 sets, then the part's init (v302's 0x89393370), the voices' and the
// lists' inits (v272's 0x89391ba0 and 0x89391270), and 0x8939b200: part 0's reload object idle, eight bare returns
// (0x893a7f40, 0x893a1ae0, 0x893a0330, 0x8939a8b0, 0x8939b7d0, 0x89399c10, 0x893a6f10, 0x893a6200), and the static M's
// part init (v295's 0x893a2fe0).  π/2 read by Vc2PartSettingsTables.  Not in the sound yet.  Bit for bit:
// xt_vc2settings' eighteenth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's start-up".  The notes' task.
#include "Vc2PartNumbers.h"
#include "Vc2PartReloads.h"
#include "Vc2PartVoices.h"
#include "Vc2StaticMatrix.h"
#include <array>
#include <cstdint>

// The words the start-up sets that no port held before, by their addresses
struct Vc2PartStartWords {
    static constexpr uint32_t kRoutingAt = 0x899893f8, kFloatsAt = 0x899893fc, kAfterAt = 0x89989406;   // [FW]
    static constexpr uint32_t kRemainAt = 0x89982710, kSettingsObjectAt = 0x899826fc;                  // [FW]
    std::array<uint8_t, 2> routing {};   // 0x899893f8: the routing object's first two bytes, 0 and 127
    std::array<float, 2> floats {};      // 0x899893fc: its +4 and +8, acos 0 × 2 (π) and that ÷ 2 (π/2)
    std::array<int8_t, 2> after {};      // 0x89989406: the two bytes after Time Trip's assigns, −1
    uint32_t remain = 0;                 // 0x89982710: the settings object's +1 (its Patch Remain byte, v296's)
};

// What the start-up reaches
struct Vc2PartStartWorld {
    Vc2PartInitWorld& init;
    Vc2PartStartWords& words;
    Vc2PartVoices& voices;
    Vc2PartReloads& reloads;
    Vc2StaticMatrix& staticM;
    Vc2StaticMatrixParts& staticParts;
};
void vc2PartStartUp(Vc2PartStartWorld& w);   // 0x89392ab0
