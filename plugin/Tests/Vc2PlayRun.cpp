// VSynthEmuVc2PlayTest's run (Vc2PlayRig.h; v455's moved here in v458, so the dry runs and the effects run are one): the
// host's MIDI and input through the processor's processBlock, beside the part driven directly, and what processBlock
// allocated, counted by this program's own operator new and delete.
#include "Vc2PlayDirect.h"
#include "Vc2PlayRig.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <new>

namespace {
// What the operators below count: the news and deletes made on a thread while it counts (processBlock's own calls)
struct Allocations {
    static std::atomic<long>& made() {
        static std::atomic<long> n { 0 };
        return n;
    }
    static bool& counting() {
        thread_local bool on = false;
        return on;
    }
    static void* taken(std::size_t n) {
        if (counting()) ++made();
        void* p = std::malloc(n != 0 ? n : 1);
        if (p == nullptr) std::abort();
        return p;
    }
    static void given(void* p) noexcept {
        if (counting() && p != nullptr) ++made();
        std::free(p);
    }
};
} // namespace

void* operator new(std::size_t n) { return Allocations::taken(n); }
void* operator new[](std::size_t n) { return Allocations::taken(n); }
void operator delete(void* p) noexcept { Allocations::given(p); }
void operator delete[](void* p) noexcept { Allocations::given(p); }
void operator delete(void* p, std::size_t) noexcept { Allocations::given(p); }
void operator delete[](void* p, std::size_t) noexcept { Allocations::given(p); }

namespace {
constexpr int kBlock = kVc2PlayBlock;
using Message = Vc2PlayDirect::Message;

// The host's MIDI: a chord, a controller, a bend, the chord let go, single notes, then a burst past the voices; v458
// CC 91 and 93, which the effects' driver keeps as well
std::vector<Message> hostMessages() {
    std::vector<Message> m = { { 700, 0x90, 60, 100 },  { 1500, 0x90, 64, 90 },  { 2300, 0x90, 67, 80 },
                               { 30001, 0xb0, 1, 64 },   { 45013, 0xe0, 0, 80 },  { 52003, 0xb0, 91, 40 },
                               { 60000, 0x80, 60, 0 },   { 60007, 0x80, 64, 0 },  { 60011, 0x80, 67, 0 },
                               { 61000, 0x91, 62, 110 }, { 66001, 0xb0, 93, 20 }, { 70100, 0x81, 62, 64 } };
    for (int k = 0; k < 40; ++k) m.push_back({ 72000 + 97 * k, 0x90, (uint8_t)(48 + k % 24), (uint8_t)(40 + k) });
    for (int k = 0; k < 40; ++k) m.push_back({ 80000 + 53 * k, 0x80, (uint8_t)(48 + k % 24), 0 });
    return m;
}
} // namespace

int vc2PlayMove(VSynthEmuProcessor& p, const juce::String& id) {
    auto* prm = p.apvts.getParameter(id);
    const int now = (int)std::lround(prm->convertFrom0to1(prm->getValue()));
    const int lo = (int)prm->getNormalisableRange().start, hi = (int)prm->getNormalisableRange().end;
    const int v = (now + lo) / 2 != now ? (now + lo) / 2 : hi;
    prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
    return v;
}

Vc2Played vc2PlayRun(VSynthEmuProcessor& p, Vc2PlayDirect* direct, const std::function<void(int)>& before) {
    Vc2Played r;
    const std::vector<Message> messages = hostMessages();
    juce::AudioBuffer<float> buf(2, kBlock);
    std::vector<float> wantL(kBlock), wantR(kBlock), inL(kBlock), inR(kBlock);
    for (int b = 0; b < kVc2PlayBlocks; ++b) {
        const int from = b * kBlock;
        before(b);
        juce::MidiBuffer midi;
        for (const Message& m : messages)
            if (m.frame >= from && m.frame < from + kBlock)
                midi.addEvent(juce::MidiMessage(m.status, m.data1, m.data2), m.frame - from);
        for (int i = 0; i < kBlock; ++i) {
            inL[(size_t)i] = vc2PlayInput(from + i, 0);
            inR[(size_t)i] = vc2PlayInput(from + i, 1);
        }
        buf.copyFrom(0, 0, inL.data(), kBlock);
        buf.copyFrom(1, 0, inR.data(), kBlock);
        const auto began = std::chrono::steady_clock::now();
        Allocations::counting() = true;
        p.processBlock(buf, midi);
        Allocations::counting() = false;
        r.seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count();
        if (direct != nullptr) {   // denormals flushed, as the SH-4 and processBlock have them
            const juce::ScopedNoDenormals flushed;
            direct->host(inL.data(), inR.data(), kBlock, messages, wantL.data(), wantR.data());
        }
        for (int i = 0; i < kBlock; ++i)
            for (int ch = 0; ch < 2; ++ch) {
                const float got = buf.getSample(ch, i), want = (ch ? wantR : wantL)[(size_t)i];
                r.finite = r.finite && std::isfinite(got);
                r.peak = std::max(r.peak, (double)std::fabs(got));
                r.sumSquares += (double)got * got;
                if (direct == nullptr || got == (std::isfinite(want) ? want : 0.0f)) continue;
                if (r.first < 0) r.first = from + i;
                ++r.differ;
            }
    }
    r.allocations = Allocations::made().exchange(0);
    return r;
}
