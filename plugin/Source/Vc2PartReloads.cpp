// The VC-2 part's reload objects (v300): see Vc2PartReloads.h.  Each routine's stores and calls in rom.4's order; the
// request's and the message's switches on the state byte are their braf tables, a state past 2 going the way of 1 in
// the message and of 0 in the request.
#include "Vc2PartReloads.h"
#include "Vc2PartSettingsTables.h"
#include <iterator>

namespace {
// [FW] the object's class (the OS base's, 0x898c6ba8, written first and overwritten)
constexpr uint32_t kReloadClass = 0x898cd430;
constexpr uint8_t kIdle = 0, kAsked = 1, kReading = 2;   // [FW] the state byte
constexpr uint8_t kHeld = 1, kFree = 0;                  // [FW] the flags around the queue's remove
} // namespace

Vc2Reload& Vc2PartReloads::at(uint32_t object) { return objects[(object - kAt) / sizeof(Vc2Reload)]; }

void Vc2PartReloads::staticInit() {
    floats = vc2PartSettingsTables().reloadFloats;
    for (uint32_t k = 0; k < kObjects; ++k) construct(kAt + static_cast<uint32_t>(sizeof(Vc2Reload)) * k);
}

void Vc2PartReloads::construct(uint32_t object) {
    Vc2TimerNode& n = at(object).node;
    n = { object, object, n.word8, kReloadClass };   // linked to itself, off the queue; the queue's word as it was
}

void Vc2PartReloads::unpostHeld(uint32_t object) {
    held_.fill(kHeld);
    parts_.unpost(object);
    held_.fill(kFree);
}

void Vc2PartReloads::stopReading(uint32_t object) {
    if (at(object).state == kReading) unpostHeld(object);
}

void Vc2PartReloads::cancel(uint32_t object) { unpostHeld(object); }

bool Vc2PartReloads::run(uint32_t object) {
    stopReading(object);
    Vc2Reload& r = at(object);
    r.state = kIdle;
    return parts_.readPart(r.part);
}

void Vc2PartReloads::post(uint32_t object) {
    Vc2Reload& r = at(object);
    r.state = kAsked;
    if (r.posted != 0) return;
    r.posted = 1;
    parts_.post(object);
}

void Vc2PartReloads::repost(uint32_t object) {
    unpostHeld(object);
    post(object);
}

void Vc2PartReloads::ignore(uint32_t) {}

void Vc2PartReloads::request(uint32_t object) {
    static constexpr Way kWays[] = { &Vc2PartReloads::post, &Vc2PartReloads::ignore, &Vc2PartReloads::repost };
    Vc2Reload& r = at(object);
    if (r.posted != 0) {
        stopReading(object);
        r.state = kIdle;
    }
    (this->*(r.state < std::size(kWays) ? kWays[r.state] : kWays[kIdle]))(object);
}

int32_t Vc2PartReloads::fire(uint32_t object) {
    Vc2Reload& r = at(object);
    if (r.state != kIdle) {
        r.state = kReading;
        parts_.readPart(r.part);
        r.state = kIdle;
    }
    r.posted = 0;
    return 0;
}

void Vc2PartReloads::readNow(uint32_t object) {
    Vc2Reload& r = at(object);
    r.state = kIdle;
    parts_.readPart(r.part);
}

void Vc2PartReloads::unpostNow(uint32_t object) { parts_.unpost(object); }

void Vc2PartReloads::message(uint32_t object) {
    static constexpr Way kWays[] = { &Vc2PartReloads::ignore, &Vc2PartReloads::readNow, &Vc2PartReloads::unpostNow };
    const uint8_t s = at(object).state;
    (this->*(s < std::size(kWays) ? kWays[s] : kWays[kAsked]))(object);
}
