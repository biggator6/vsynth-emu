// The VC-2 part's static M (v295): see Vc2StaticMatrix.h.  Each initializer's stores and calls in rom.4's order.
#include "Vc2StaticMatrix.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2VoiceQueue.h"

namespace {
// [FW] the node's class (0x89394f30's 0x898cd340 written first and overwritten) and the object's
constexpr uint32_t kNodeClass = 0x898cd460, kMatrixClass = 0x898cd448;
constexpr uint8_t kNoVelocity = 0, kMiddleKey = 60;   // [FW] the head's velocity, key and poly key
constexpr int32_t kNoArg = 0;                         // [FW] the channel state's set-up's second argument
} // namespace

void Vc2StaticMatrix::staticInit(Vc2StaticMatrixParts& parts) {
    const auto& f = vc2PartSettingsTables().staticFloats;
    floats[0] = f[0];
    floats[1] = f[1];
    scale = f[2];
    node.prev = kNodeAt;
    node.next = kNodeAt;
    node.vtable = kNodeClass;
    vtable = kMatrixClass;
    parts.channelStateInit(kStateAt, kNoArg);
}

void Vc2StaticMatrix::partInit(Vc2StaticMatrixParts& parts) {
    links.part = 0;
    m.head[0] = kNoVelocity;
    m.head[2] = kMiddleKey;
    m.head[1] = kMiddleKey;
    links.channel = unreceived;
    parts.enqueue(Vc2UpdateQueue::kAt, kNodeAt);
}
