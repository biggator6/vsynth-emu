// The VC-2 part's start-up (v304): see Vc2PartStartUp.h.  Its stores and calls in rom.4's order.  acos 0 is acosf's
// π/2 less asin 0, which is 0, so its constant itself; × 2 and ÷ 2 are exact.  0x8939b200's eight calls return at once
// and are left out.
#include "Vc2PartStartUp.h"
#include "Vc2PartSettingsTables.h"

namespace {
constexpr int8_t kOff = -1;                   // [FW] Time Trip's assigns and the bytes after them
constexpr uint8_t kRoutingFirst = 0, kRoutingSecond = 127;
constexpr float kTwo = 2.0f;
} // namespace

void vc2PartStartUp(Vc2PartStartWorld& w) {
    Vc2PartRows& t = w.init.t;
    Vc2PartStartWords& s = w.words;
    t.tripAssigns.fill(kOff);
    s.after.fill(kOff);
    const float pi = vc2PartSettingsTables().halfPi * kTwo;
    s.routing[1] = kRoutingSecond;
    s.floats[0] = pi;
    s.floats[1] = pi / kTwo;
    s.routing[0] = kRoutingFirst;
    s.remain = Vc2PartStartWords::kSettingsObjectAt + 1;
    vc2PartInit(w.init);
    w.voices.initVoices();
    w.voices.initLists();
    Vc2Reload& r = w.reloads.objects[0];
    r.part = 0;
    r.state = 0;
    r.posted = 0;
    w.staticM.partInit(w.staticParts);
}
