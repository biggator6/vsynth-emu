// The VC-2's free LFOs' manager: see Vc2FreeLfos.h (v271).
#include "Vc2FreeLfos.h"
#include "XtLfo.h"
#include <cmath>

namespace {
constexpr int32_t kNone = -1;   // [FW] 0x893aca1a: a free object's LFO and id, and a node's +4, before any use
} // namespace

// The clock's list alone reaches an object's clock here, so every object rom.2's class is given is clocked
XtLfoFree Vc2FreeLfos::lawOf(const Vc2FreeLfoWords& o) {
    XtLfoFree f;
    f.rateHz = o.rate;
    f.ph = o.phase;
    f.sync = o.sync;
    f.wrap = o.wrapped;
    f.count = o.clockCount;
    f.step = o.clockStep;
    f.target = o.clockTarget;
    f.dt = o.clockTime;
    f.tempo = o.tempo;
    f.clocked = true;
    return f;
}

void Vc2FreeLfos::keep(const XtLfoFree& f, Vc2FreeLfoWords& o) {
    o.rate = f.rateHz;
    o.phase = f.ph;
    o.sync = f.sync;
    o.wrapped = f.wrap;
    o.clockCount = f.count;
    o.clockStep = f.step;
    o.clockTarget = f.target;
    o.clockTime = f.dt;
    o.tempo = f.tempo;
}

// [FW] 0x893ac9a0: the phase a tick moves, as one tick from 0 shows it, with no clock's target to hold it back
float Vc2FreeLfos::stepOf(XtLfoFree f) {
    f.ph = 0.0f;
    f.wrap = 1;
    f.tick(1);
    return f.ph;
}

Vc2FreeLfoNode& Vc2FreeLfos::node(uint32_t address) {
    return *reinterpret_cast<Vc2FreeLfoNode*>(reinterpret_cast<char*>(&w) + (address - kAt));
}

uint32_t Vc2FreeLfos::addressOf(const void* word) const {
    return kAt + (uint32_t)(static_cast<const char*>(word) - reinterpret_cast<const char*>(&w));
}

Vc2FreeLfoWords& Vc2FreeLfos::object(uint32_t free) {
    return free == 0 ? none_ : w.objects[(free - kObjectsAt) / sizeof(Vc2FreeLfoWords)];
}

// [FW] each list is walked to its end: nothing keeps its tail
void Vc2FreeLfos::append(Vc2FreeLfoNode& list, uint32_t at) {
    uint32_t tail = addressOf(&list);
    while (node(tail).next != 0) tail = node(tail).next;
    node(tail).next = at;
    node(at).next = 0;
    node(at).prev = tail;
}

void Vc2FreeLfos::moveTo(Vc2FreeLfoNode& list, uint32_t at) {
    const Vc2FreeLfoNode& n = node(at);
    if (n.next != 0) node(n.next).prev = n.prev;
    node(n.prev).next = n.next;
    append(list, at);
}

void Vc2FreeLfos::construct() {
    const auto clearNode = [](Vc2FreeLfoNode& n) {   // [FW] 0x893ad8a0 leaves the +0
        n.unread = kNone;
        n.next = n.prev = 0;
    };
    for (Vc2FreeLfoNode* head : { &w.taken, &w.spare, &w.clocked, &w.unclocked }) clearNode(*head);
    w.spareCount = w.takenCount = 0;
    for (size_t k = 0; k < Vc2FreeLfoBank::kObjects; ++k) {
        clearNode(w.nodes[k]);
        clearNode(w.clockNodes[k]);
        Vc2FreeLfoWords& o = w.objects[k];
        XtLfoFree fresh;
        fresh.step = o.clockStep;          // [FW] 0x893aca00 leaves the clock's step
        keep(fresh, o);
        o.step = stepOf(fresh);
        o.lfo = kNone;
        o.id = (uint32_t)kNone;
        o.refs = 0;
        o.node = addressOf(&w.nodes[k]);
        w.nodes[k].object = addressOf(&o);
        append(w.spare, o.node);
        ++w.spareCount;
        append(w.unclocked, addressOf(&w.clockNodes[k]));
    }
}

uint32_t Vc2FreeLfos::take(uint32_t id, int32_t lfo) {
    uint32_t at = w.spare.next;
    while (at != 0) {
        const Vc2FreeLfoWords& o = object(node(at).object);
        if (o.id == id && o.lfo == lfo) break;
        at = node(at).next;
    }
    if (at == 0) at = w.spare.next;
    if (at == 0) return 0;
    moveTo(w.taken, at);
    --w.spareCount;
    ++w.takenCount;
    return node(at).object;
}

void Vc2FreeLfos::release(uint32_t free) {
    Vc2FreeLfoWords& o = object(free);
    if (--o.refs != 0) return;
    moveTo(w.spare, o.node);
    --w.takenCount;
    ++w.spareCount;
}

// The clock's list's node of a tone id and LFO number, 0 none
uint32_t Vc2FreeLfos::onClock(uint32_t id, int32_t lfo) {
    uint32_t at = w.clocked.next;
    while (at != 0) {
        const Vc2FreeLfoWords& o = object(node(at).object);
        if (o.id == id && o.lfo == lfo) return at;
        at = node(at).next;
    }
    return 0;
}

void Vc2FreeLfos::addClock(uint32_t free, uint32_t id, int32_t lfo) {
    const uint32_t at = w.unclocked.next;
    if (onClock(id, lfo) != 0 || at == 0) return;
    node(at).object = free;
    moveTo(w.clocked, at);
}

void Vc2FreeLfos::removeClock(uint32_t id, int32_t lfo) {
    const uint32_t at = onClock(id, lfo);
    if (at == 0) return;
    object(node(at).object).sync = 0;
    moveTo(w.unclocked, at);
}

void Vc2FreeLfos::setRate(uint32_t free, int32_t sync, float rate, float tempo) {
    Vc2FreeLfoWords& o = object(free);
    o.sync = sync;
    o.rate = rate;
    XtLfoFree f = lawOf(o);
    o.step = stepOf(f);                    // [FW] 0x893acae0: from the rate as given, before a note value's own
    // [FW] 0x893acafa: a note value's whole clocks
    f.setRate({ .rate = sync != 0 ? std::floor(rate) : rate, .sync = sync }, tempo);
    keep(f, o);
}

void Vc2FreeLfos::tick(int32_t n) {
    const auto ticks = static_cast<int32_t>(static_cast<int16_t>(n));   // [FW] 0x893ad60a: exts.w
    for (Vc2FreeLfoWords& o : w.objects) {
        XtLfoFree f = lawOf(o);
        f.tick(ticks);
        keep(f, o);
    }
}

void Vc2FreeLfos::clock(float seconds) {
    for (uint32_t at = w.clocked.next; at != 0; at = node(at).next) {
        Vc2FreeLfoWords& o = object(node(at).object);
        XtLfoFree f = lawOf(o);
        f.clock(seconds);
        keep(f, o);
    }
}
