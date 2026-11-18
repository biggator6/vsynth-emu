// xt_fx control's check of a Matrix move through the queue: what it sends reaches the chip with no later message to carry
// it.  Beside a random run this doesn't depend on a move landing on a type that answers it.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
namespace {
constexpr long kMostFrames = 44100;    // far past the longest wait a type's load leaves
constexpr int kAnswering = 28;         // [FW] measured: the types whose words this move changes, of those it reaches
constexpr int kMoved = 20000;

// Has the chip all the driver has sent, word for word?
bool landed(XtFxControl& c, XtFxChip& chip) {
    playQueue(c, chip, kMostFrames);
    return c.drained() && wordsApart(c, chip) == 0;
}
} // namespace

void matrixThroughQueue(const char* rom) {
    const FxIds& fx = rom2Ids();
    auto chip = std::make_unique<XtFxChip>();
    XtFxControl c(rom);
    int tried = 1, different = landed(c, *chip) ? 0 : 1, moved = 0;
    for (int t = 0; t < fx.image.mfxTypes && !different; t++) {
        editOne(c, fx.mfxType, t);
        different += landed(c, *chip) ? 0 : 1;
        const auto before = std::make_unique<XtFxChip>(*chip);
        c.setMatrix(kMoved, kMoved, kMoved);
        different += landed(c, *chip) ? 0 : 1;
        moved += wordsApart(*before, *chip) != 0;
        c.setMatrix(0, 0, 0);
        different += landed(c, *chip) ? 0 : 1;
        tried += 3;
    }
    tried++;
    different += moved == kAnswering ? 0 : 1;
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  the Matrix through the queue: %d types moved, the chip behind by %d words\n", moved,
                    wordsApart(c, *chip));
    std::printf("control's Matrix through the queue: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck
#endif
