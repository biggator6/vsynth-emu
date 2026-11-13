// vc2run hold: which controller reaches the vocoder's hold (an engine's slot 6), through rom.4's own controller path.
// The vocoder is built and set to an engine (0x893f0600), the controllers' handler table built by its own routine
// (0x89393370: a handler object per controller number, its pointer at 0x899891e0 + 4 cc), then every controller sent
// through the dispatcher 0x89392f90(port, channel, cc, value), as MIDI IN's is.  Hooks on the glue (0x893b1c10) and the
// engine's slot 6 record what reaches them, and let the call go on.
//   hold [engine]    engine 1 LEGACY, 2 HIRESO (default): every cc at 127 and at 0 on port 1 channel 1, then cc 64's
//                    values 0…127, then cc 64 on every port and channel
#include "vc2runs.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace vc2 {

namespace {

constexpr uint32_t kSetEngine = 0x893f0600;      // [FW] (vocoder, (Type + 1) × Sw): 0 off, 1 LEGACY, 2 HIRESO
constexpr uint32_t kBuildHandlers = 0x89393370;  // [FW] the controllers' handler objects, one per number
constexpr uint32_t kHandlers = 0x899891e0;       // [FW] their pointers, by controller number
constexpr uint32_t kDispatch = 0x89392f90;       // [FW] (port 1-based, channel 0-based, cc, value)
constexpr uint32_t kHoldGlue = 0x893b1c10;       // [FW] (value): the vocoder's hold, from CC 64's handler
constexpr uint32_t kListenerHead = 0x899826e0;   // [FW] the controllers' listener list (.bss, set by the OS)
constexpr uint32_t kHoldCc = 64;                 // the controller the table builder gives the hold's class
constexpr int kSlotHold = 6;
constexpr uint32_t kControllers = 128, kPorts = 2, kChannels = 16;

struct Message {
    uint32_t port, channel, cc, value;   // port 1-based, channel 0-based
};

// The machine with the hooks that count what reaches the glue and slot 6
struct Probe {
    Machine& m;
    std::vector<int> glue, slot;   // the values each was called with, in order
    // 1 when the message reached slot 6, 0 when not, −1 when the call failed
    int hits(Message msg) {
        const size_t before = slot.size();
        if (!m.run(kDispatch, {msg.port, msg.channel, msg.cc, msg.value})) return -1;
        return slot.size() != before ? 1 : 0;
    }
};

} // namespace

int hold(Machine& m, int argc, char** argv) {
    const uint32_t engine = argc >= 2 ? (uint32_t)std::strtoul(argv[1], nullptr, 0) : 2;
    if (!m.run(kBuild, {kVocoder}) || !m.run(kSetEngine, {kVocoder, engine})) return 1;
    const uint32_t active = m.cpu.rd32(kVocoder + 0x20);
    std::printf("engine %u: the vocoder's +0x20 is %08x (HIRESO %08x, LEGACY %08x)\n", engine, active, kHireso,
                kLegacy);
    if (!m.run(kBuildHandlers, {})) return 1;
    // The base handler (0x893938d0) tells the listeners on 0x899826e0's circular list (walked by 0x89390ea0 through
    // +0x18) the controller's new value; the OS that fills it hasn't run, so it gets an empty one: a node to itself.
    const uint32_t listeners = m.alloc(0x40);
    m.cpu.wr32(listeners + 0x18, listeners);
    m.cpu.wr32(kListenerHead, listeners);
    const uint32_t handler = m.cpu.rd32(kHandlers + 4 * kHoldCc);
    std::printf("handler table built in %llu steps; cc %u's handler %08x, its vtable %08x\n",
                (unsigned long long)m.cpu.steps(), kHoldCc, handler, m.cpu.rd32(handler + 4));
    Probe p{m, {}, {}};
    m.cpu.hook(kHoldGlue, [&p](Sh4& c) { p.glue.push_back((int)c.r[4]); });
    m.cpu.hook(slotOf(m, active, kSlotHold), [&p](Sh4& c) { p.slot.push_back((int)c.r[5]); });
    int hit = 0;
    for (const uint32_t value : {127u, 0u}) {
        std::printf("value %3u, port 1 channel 1: cc reaching slot 6:", value);
        for (uint32_t cc = 0; cc < kControllers; ++cc) {
            if ((hit = p.hits({1, 0, cc, value})) < 0) return 1;
            if (hit) std::printf(" %u (with %d)", cc, p.slot.back());
        }
        std::printf("\n");
    }
    std::printf("cc %u at 0…127: slot 6 called at", kHoldCc);
    for (uint32_t value = 0; value < kControllers; ++value) {
        if ((hit = p.hits({1, 0, kHoldCc, value})) < 0) return 1;
        if (hit) std::printf(" %u", value);
    }
    std::printf("\ncc %u at 127 by port and channel: slot 6 called on", kHoldCc);
    for (uint32_t port = 1; port <= kPorts; ++port) {
        for (uint32_t channel = 0; channel < kChannels; ++channel) {
            if ((hit = p.hits({port, channel, kHoldCc, 127})) < 0) return 1;
            if (hit) std::printf(" %u/%u", port, channel + 1);
        }
    }
    std::printf("\nglue calls %zu, slot 6 calls %zu\n", p.glue.size(), p.slot.size());
    return 0;
}

} // namespace vc2
