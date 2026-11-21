// xt_fx's check of a Matrix move through the queue: what it sends reaches the chip with no later message to carry it.
// Beside a random run this doesn't depend on a move landing on a type that answers it.  rom.2's under xt_fx control;
// rom.4's under vc2 (v401), where no type answers, as rom.4 sends its Matrix's MFX-PRMs to the part mixer, and the
// driver's object is held to what it was.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
namespace {
constexpr long kMostFrames = 44100;    // far past the longest wait a type's load leaves
constexpr int kMoved = 20000;
// [FW] measured: the types whose words this move changes, of those it reaches.  rom.4's none (v401)
constexpr int kRom2Answering = 28;
constexpr int kVc2Answering = 0;

// Has the chip all the driver has sent, word for word?
bool landed(XtFxControl& c, XtFxChip& chip) {
    playQueue(c, chip, kMostFrames);
    return c.drained() && wordsApart(c, chip) == 0;
}

// The driver's object, from its address to its message's flags, as its interpreter holds it
std::vector<uint8_t> objectOf(const XtFxControl& c, const XtFxImage& im) {
    std::vector<uint8_t> bytes;
    for (uint32_t a = im.self; a < im.message; ++a) bytes.push_back(c.driverByte(a));
    return bytes;
}

// All three moved on each MFX type and back; `answering` of the types must move the chip.  An image where none should
// keeps the driver's object as it was, too
void matrixMoves(const char* rom, const FxIds& fx, int answering) {
    auto chip = std::make_unique<XtFxChip>();
    XtFxControl c(rom, nullptr, fx.image);
    int tried = 1, different = landed(c, *chip) ? 0 : 1, moved = 0;
    for (int t = 0; t < fx.image.mfxTypes && !different; t++) {
        editOne(c, fx.mfxType, t);
        different += landed(c, *chip) ? 0 : 1;
        const auto before = std::make_unique<XtFxChip>(*chip);
        const std::vector<uint8_t> object = objectOf(c, fx.image);
        c.setMatrix(kMoved, kMoved, kMoved);
        different += landed(c, *chip) ? 0 : 1;
        moved += wordsApart(*before, *chip) != 0;
        different += answering == 0 && objectOf(c, fx.image) != object ? 1 : 0;
        c.setMatrix(0, 0, 0);
        different += landed(c, *chip) ? 0 : 1;
        tried += 3;
    }
    tried++;
    different += moved == answering ? 0 : 1;
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  the Matrix through the queue: %d types moved, the chip behind by %d words\n", moved,
                    wordsApart(c, *chip));
    std::printf("%s's Matrix through the queue: %d samples, %d different\n", fx.label, tried, different);
}
} // namespace

void matrixThroughQueue(const char* rom) { matrixMoves(rom, rom2Ids(), kRom2Answering); }
void vc2MatrixThroughQueue(const char* rom) { matrixMoves(rom, vc2Ids(), kVc2Answering); }
} // namespace xtfxcheck
#endif
