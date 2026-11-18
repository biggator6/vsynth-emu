// xt_fx's check of XtFxControl's start: every byte on the chip's bus and every wait, in order, against the firmware's own
// effect task run from its entry to its first wait for an event, then the first message and the input's routing.  The
// words a start leaves can't show this: much of what the start task writes is written again by the first message.
// XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckHand.h"

namespace xtfxcheck {
namespace {
// XtFxControl's bus as the bytes and waits the hand driver logs
struct BusBytes : Recorder {
    std::vector<uint32_t> log;
    void write(uint8_t command, uint16_t index, uint32_t word) override {
        log.push_back(0x2003u << 8 | command);
        for (uint32_t k = 0; k < 4; k++) log.push_back(((uint32_t)index * 4 + k) << 8 | (uint8_t)(word >> (8 * k)));
        Recorder::write(command, index, word);
    }
    void wait(uint32_t samples) override { log.push_back(kWaitMark | samples); }
};

void startAgainstTask(const char* rom, const HandImage& hand, const FxIds& fx) {
    auto bus = std::make_unique<BusBytes>();
    const XtFxControl control(rom, bus.get(), fx.image);
    std::vector<uint32_t> own;
    const auto task = std::make_unique<HandDriver>(rom, hand, &own);
    const size_t both = std::min(own.size(), bus->log.size());
    size_t different = std::max(own.size(), bus->log.size()) - both;
    size_t first = both;
    for (size_t i = both; i-- > 0;) {
        if (own[i] == bus->log[i]) continue;
        different++;
        first = i;
    }
    if (!control.ok()) different++;
    different += (size_t)wordsApart(bus->chip, task->chip);
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  %s's start: %zu entries against the task's %zu, first apart at %zu\n", fx.label, bus->log.size(),
                    own.size(), first);
    std::printf("%s's start against the firmware's task: %zu samples, %zu different\n", fx.label, own.size(),
                different);
}
} // namespace

void rom2Start(const char* rom) { startAgainstTask(rom, handRom2(), rom2Ids()); }
void rom4Start(const char* rom) { startAgainstTask(rom, handRom4(), vc2Ids()); }
} // namespace xtfxcheck
#endif
