// The VC-2 part's assigner's second part, the part node's note start and off: see Vc2Assigner.h (v278).  Each
// routine's reads and calls in rom.4's order.
#include "Vc2Assigner.h"
#include "Vc2Keys.h"
#include <algorithm>

namespace {
constexpr uint32_t kSourceBytes = 0x480, kChannelBytes = 0x48, kGlideBytes = 0x24;   // [FW] a node's two glides
constexpr uint32_t kTranspose = 0, kMono = 1, kLegato = 2;                         // [FW] the part's settings bytes
constexpr uint32_t kCarrier = 0x16, kCarrierHeld = 0x18;
constexpr uint32_t kCarrierKey = 60, kCarrierPassed = 100, kCarrierKind = 1;          // [FW] the carrier's note
constexpr int32_t kKeyMost = 127;

// What the note start does after its carrier's: mono's retune when legato, a fresh note, or poly's note
enum class Then { tail, legato, fresh, poly };

int32_t movedKey(uint32_t part, int32_t by, Vc2AssignParts& parts) {
    return std::clamp((int32_t)(int8_t)parts.partSetting(part, kTranspose) + by, 0, kKeyMost);
}

// The node's first glide, by its key table's source and its own channel; the second is the next
uint32_t firstGlide(const Vc2NodeNote& n, Vc2Keys& keys, Vc2AssignParts& parts) {
    return Vc2Keys::kGlidesAt + kSourceBytes * keys.table(n.table).source + kChannelBytes * parts.firstByte(n.node);
}

int32_t glideOff(uint32_t glide, Vc2Keys& keys) {
    return keys.glide(glide).noteOff(keys.table(keys.glide(glide).table));
}
} // namespace

void Vc2Assigner::nodeNoteOn(const Vc2NodeNote& n, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t part = parts.firstByte(n.message);
    parts.readParameters(part);
    const int32_t key = movedKey(part, n.value, parts);
    const uint32_t first = firstGlide(n, keys, parts);
    Vc2KeyGlide& glide = keys.glide(first);
    glide.noteOn(parts.clock(), keys.table(glide.table));
    const Vc2AssignCall carrier { first + kGlideBytes, n.message, kCarrierKey, kCarrierPassed, kCarrierKind };
    const bool mono = parts.partSetting(part, kMono) != 0;
    Then then = Then::tail;
    if (parts.partSetting(part, kCarrier) != 0) {
        if (parts.partSetting(part, kCarrierHeld) != 0) {
            if (!matching(carrier, v, parts)) build(carrier, v, parts);
        } else if (glide.prevMonoHeld != 0) {
            then = mono ? Then::legato : Then::poly;
        } else if (glide.prevPolyHeld != 0) {
            then = mono ? Then::fresh : Then::poly;
        } else {
            truncateSame(carrier, v, parts);
            build(carrier, v, parts);
        }
    }
    if (then == Then::tail) then = !mono ? Then::poly : glide.prevMonoHeld != 0 ? Then::legato : Then::fresh;
    const Vc2AssignCall own { first, n.message, (uint32_t)key, n.passed, 0 };
    if (then == Then::legato && parts.partSetting(part, kLegato) != 0) {
        retune(own, (uint32_t)key, false, v, parts);
        return;
    }
    if (then != Then::poly) truncateSame(own, v, parts);
    build(own, v, parts);
}

void Vc2Assigner::nodeNoteOff(const Vc2NodeNote& n, Vc2Keys& keys, Vc2PartVoices& v, Vc2AssignParts& parts) {
    const uint32_t part = parts.firstByte(n.message);
    const uint32_t first = firstGlide(n, keys, parts);
    const int32_t found = glideOff(first, keys);
    if (found < 0) {
        if (parts.partSetting(part, kCarrierHeld) == 0) glideOff(first + kGlideBytes, keys);
        return;
    }
    if (parts.partSetting(part, kMono) == 0) return;
    parts.readParameters(part);
    retune({ first, n.message }, (uint32_t)movedKey(part, found, parts), false, v, parts);
}
