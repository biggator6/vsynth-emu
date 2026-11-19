// The VC-2 tone's LFO: see Vc2ToneLfo.h (v268).
#include "Vc2ToneLfo.h"
#include "Vc2FreeLfos.h"
#include "XtFloat.h"
#include "XtLfo.h"
#include <cmath>
#include <cstring>

namespace {
constexpr uint32_t kVtable = 0x898cd490;                 // [FW] 0x893acc1e
constexpr float kFull = 131072.0f;                       // [FW] 0x893ac6a8: the fade's full amplitude
constexpr uint32_t kBlockBytes = 8 * sizeof(uint32_t);   // [FW] 0x893acda0 copies the block to +0x4c…+0x6b
constexpr int32_t kFirstOffMode = 2;                     // [FW] 0x893ac928: modes 2 and 3 start the fade at note-off
constexpr float kFewestClocks = 3.0f, kMostClocks = 192.0f;   // [FW] 0x893acfd4, 0x893acfd8
constexpr float kPhaseSpan = 65535.0f, kMinute = 60.0f, kClocksABeat = 24.0f;   // [FW] 0x893acfdc…0x893acfe4
} // namespace

void Vc2ToneLfo::load(XtLfo& l) const {
    l.p = { w.wave, w.rate, w.delayMs, w.fadeMs, w.keySync, w.offset, w.fadeMode, w.sync };
    l.state = w.state;
    l.dir = w.direction;
    l.count = w.count;
    l.tIn = w.fadeIn;
    l.tOut = w.fadeOut;
    l.delay = w.delay;
    l.rIn = w.rateIn;
    l.rOut = w.rateOut;
    l.rest = w.rest;
    l.target = w.target;
    l.amp = w.amplitude;
    l.active = w.active != 0;
    l.o = w.out;
    l.ph = w.phase;
    l.prev = w.previous;
    l.hold = w.held;
    l.x = w.chaos;
    l.s84 = w.clockStep;
    l.t88 = w.clockTarget;
    l.d8c = w.clockTime;
    l.w90 = w.clockWrap;
    l.c94 = w.clockCount;
}

void Vc2ToneLfo::store(const XtLfo& l) {
    w.state = (uint8_t)l.state;
    w.direction = (int8_t)l.dir;
    w.count = l.count;
    w.fadeIn = l.tIn;
    w.fadeOut = l.tOut;
    w.delay = l.delay;
    w.rateIn = l.rIn;
    w.rateOut = l.rOut;
    w.rest = l.rest;
    w.target = l.target;
    w.amplitude = l.amp;
    w.active = l.active ? 1 : 0;
    w.out = l.o;
    w.phase = l.ph;
    w.previous = l.prev;
    w.held = l.hold;
    w.chaos = l.x;
    w.clockTarget = l.t88;
    w.clockTime = l.d8c;
    w.clockWrap = l.w90;
    w.clockCount = l.c94;
}

void Vc2ToneLfo::storeTicked(const XtLfo& l) {
    store(l);
    w.fadeLevel = xtf::div(l.amp, kFull);
}

void Vc2ToneLfo::construct(float& generator) {
    XtLfo seed;
    seed.x = generator;
    const float next = seed.chaos();
    generator = seed.x;
    w.vtable = kVtable;
    w.state = 0;                           // the delay and fade's base, 0x893ac550
    w.direction = 1;
    w.count = 0;
    w.fadeIn = w.fadeOut = w.delay = w.rest = 0.0f;
    w.target = w.amplitude = kFull;
    w.active = 0;
    w.out = w.unread3c = w.phase = w.previous = w.held = 0.0f;
    w.chaos = std::fabs(next);
    w.waveCase = w.follow = w.on = 0;
}

void Vc2ToneLfo::destroy() { w.vtable = kVtable; }

void Vc2ToneLfo::takeBlock(const uint32_t* block) { std::memcpy(&w.wave, block, kBlockBytes); }

void Vc2ToneLfo::noteOn(const uint32_t* block, float tempo) {
    using namespace xtf;
    takeBlock(block);
    w.tempo = tempo;
    w.phase = 0.0f;
    if (w.sync != 0) {
        w.clockTarget = 0.0f;
        w.clockCount = 0;
        w.rate = std::floor(w.rate);       // [FW] 0x893acdea: a note value's whole clocks
        w.rate = w.rate > kMostClocks ? kMostClocks : (kFewestClocks > w.rate ? kFewestClocks : w.rate);
        w.clockStep = div(kPhaseSpan, w.rate);
        w.clockTime = div(div(kMinute, tempo), kClocksABeat);
    }
    trigger();
}

void Vc2ToneLfo::follow(const Vc2ToneLfo& other, const uint32_t* block, Vc2FreeLfos& frees) {
    const Vc2LfoWords& o = other.w;
    w.index = o.index;
    w.out = o.out;
    w.unread3c = o.unread3c;
    w.phase = o.phase;
    w.previous = o.previous;
    w.held = o.held;
    w.chaos = o.chaos;
    w.unread70 = o.unread70;
    w.tempo = o.tempo;
    w.waveCase = o.waveCase;
    w.follow = o.follow;
    w.on = o.on;
    w.clockStep = o.clockStep;
    w.clockTarget = o.clockTarget;
    w.clockTime = o.clockTime;
    w.clockWrap = o.clockWrap;
    w.clockCount = o.clockCount;
    takeBlock(block);
    if (w.follow != 0) ++frees.object(w.follow).refs;
    trigger();
}

void Vc2ToneLfo::attach(uint32_t free, Vc2FreeLfos& frees) {
    w.follow = free;
    ++frees.object(free).refs;
}

void Vc2ToneLfo::startFree(const uint32_t* block, float tempo, Vc2FreeLfos& frees) {
    takeBlock(block);
    if (w.follow != 0) {
        frees.setRate(w.follow, w.sync, w.rate, tempo);
        const Vc2FreeLfoWords& f = frees.object(w.follow);
        w.phase = f.phase;
        w.clockStep = f.clockStep;
        w.clockTarget = f.clockTarget;
        w.clockTime = f.clockTime;
        w.clockWrap = f.wrapped;
        w.clockCount = f.clockCount;
        w.tempo = f.tempo;
    }
    trigger();
}

void Vc2ToneLfo::trigger() {
    w.mode = (uint32_t)w.fadeMode;
    w.waveCase = 0;                        // [FW] 0x893acd70: for the eight waves a converter gives
    XtLfo l;
    load(l);
    l.setDir(w.fadeMode % 2);              // 0x893ac830: modes 1 and 3 fade down
    l.setTimes();
    if (w.fadeMode < kFirstOffMode) l.fadeStart();
    else l.state = 0;
    l.active = true;
    l.tick(0);
    storeTicked(l);
}

void Vc2ToneLfo::tick(int32_t n) {
    XtLfo l;
    load(l);
    l.tick(n);
    storeTicked(l);
}

void Vc2ToneLfo::clock(float seconds) {
    XtLfo l;
    load(l);
    l.clock(seconds);
    store(l);
}
