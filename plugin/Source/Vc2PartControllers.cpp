// The VC-2 part's controllers (v301): see Vc2PartControllers.h.  Each routine's stores and calls in rom.4's order.  The
// part's controller's three tests (Time Trip Assign X, Y, Reset All Controllers) are each made, so a number may match
// more than one: a table of the number to match and its way, in rom.4's order.  A trip object's fields are read as they
// stand at each test, as the trip's routines may move them.
#include "Vc2PartControllers.h"
#include <iterator>

namespace {
constexpr int32_t kResetAll = 121;          // [FW] Reset All Controllers
constexpr uint8_t kRest = 127;              // [FW] the part's byte with the trip at rest
constexpr int32_t kStop = 0, kStart = 1;    // [FW] 0x893b0610's second argument
} // namespace

void Vc2PartControllers::controller(int source, int32_t channel, int32_t number, int32_t value) {
    parts_.numberChanged(number, source, channel, value);
    const int32_t list = Vc2PartNodes::kChannels * (source - 1) + channel;
    for (uint32_t at = nodes_.lists[static_cast<size_t>(list)]; at != 0; at = nodes_.node(at).next)
        partController(parts_.firstByte(nodes_.node(at).word8), number, value);
}

void Vc2PartControllers::padX(uint8_t part, int32_t value) {
    const uint32_t at = parts_.tripAt(part);
    Vc2TripFront& t = parts_.trip(at);
    const auto v = static_cast<int8_t>(value);
    if (t.trip.moving == 0 || v > 0) {
        t.x = v;
        return;
    }
    parts_.tripSet(at, kStop, t.x, t.y);
    t.level = kRest;
}

void Vc2PartControllers::padY(uint8_t part, int32_t value) {
    const uint32_t at = parts_.tripAt(part);
    Vc2TripFront& t = parts_.trip(at);
    const auto v = static_cast<int8_t>(value);
    if (t.trip.moving == 0) {
        if (v <= 0) return;
        t.y = v;
        if (t.x > 0) parts_.tripSet(at, kStart, t.x, v);
        return;
    }
    if (v > 0) {
        t.y = v;
        t.level = parts_.tripMove(at, t.x, v);
        return;
    }
    parts_.tripSet(at, kStop, t.x, t.y);
    t.level = kRest;
}

void Vc2PartControllers::rest(uint8_t part, int32_t) {
    const uint32_t at = parts_.tripAt(part);
    parts_.tripSet(at, kStop, 0, 0);
    parts_.trip(at).level = kRest;
}

void Vc2PartControllers::partController(uint8_t part, int32_t number, int32_t value) {
    const int32_t matches[] = { rows_.tripAssigns[0], rows_.tripAssigns[1], kResetAll };
    constexpr Way kWays[] = { &Vc2PartControllers::padX, &Vc2PartControllers::padY, &Vc2PartControllers::rest };
    for (size_t k = 0; k < std::size(kWays); ++k)
        if (number == matches[k]) (this->*kWays[k])(part, value);
}
