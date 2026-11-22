// The VC-2 part's keys: see Vc2Keys.h (v276).  Each routine's writes as rom.4's; P's laws are XtKeyState's.
#include "Vc2Keys.h"
#include "Vc2KeysTables.h"
#include "XtFloat.h"
#include "XtKeys.h"
#include <algorithm>
#include <utility>

namespace {
constexpr uint8_t kMost = 0xff, kWasReset = 0xff;   // [FW] a count's most, the reset's mark
constexpr uint32_t kTableBytes = sizeof(Vc2KeyTable), kGlideBytes = sizeof(Vc2KeyGlide);
constexpr uint32_t kSourceBytes = kTableBytes * Vc2Keys::kChannels;
constexpr uint32_t kChannelOffset = 2;          // [FW] a channel's state, past its table's word
constexpr uint32_t kListOffset = 4;             // [FW] the constructor's +8: its own +4
constexpr int8_t kMiddle = 60;                  // [FW] the reset's keys
constexpr uint8_t kWindow = 0x20;               // [FW] and its chord window
constexpr uint32_t kSostenutoBit = 0x40;        // [FW] the bit the pedal's moves are told by
constexpr int32_t kSostenutoDown = 64;
constexpr uint32_t kSinceMost = 0xffff;         // [FW] the time since the note-on before, held
constexpr int32_t kNone = -1;

const Vc2KeysTables& keysTab() { return vc2KeysTables(); }

// One more of a count, held at its most
void countUp(uint8_t& count) {
    if (++count == 0) count = kMost;
}

uint16_t& keyWord(Vc2KeyBits& bits, uint32_t key) { return bits[key >> 4]; }
uint16_t keyBit(uint32_t key) { return keysTab().bits[key & 15]; }

// The highest bit set in `w`, by the bits' table from bit 15 down (w not 0)
int32_t highestBit(uint32_t w) {
    int32_t b = 15;
    while ((w & keysTab().bits[(size_t)b]) == 0) --b;
    return b;
}

XtKeyState glideState(const Vc2KeyGlide& g, const Vc2KeyTable& keys) {
    XtKeyState s;
    s.cur = g.key;
    s.lastMs = g.lastMs;
    s.window = g.window;
    s.prevPoly = g.prevPoly;
    s.lastPoly = g.lastPoly;
    s.prevMono = g.prevMono;
    s.lastMono = g.lastMono;
    s.prevPolyHeld = g.prevPolyHeld != 0;
    s.lastPolyHeld = g.lastPolyHeld != 0;
    s.prevMonoHeld = g.prevMonoHeld != 0;
    s.lastMonoHeld = g.lastMonoHeld != 0;
    std::ranges::copy(keys.held, s.held);
    return s;
}

void glideKeep(Vc2KeyGlide& g, const XtKeyState& s) {
    g.key = (int8_t)s.cur;
    g.lastMs = s.lastMs;
    g.window = (uint8_t)s.window;
    g.prevPoly = (int8_t)s.prevPoly;
    g.lastPoly = (int8_t)s.lastPoly;
    g.prevMono = (int8_t)s.prevMono;
    g.lastMono = (int8_t)s.lastMono;
    g.prevPolyHeld = s.prevPolyHeld;
    g.lastPolyHeld = s.lastPolyHeld;
    g.prevMonoHeld = s.prevMonoHeld;
    g.lastMonoHeld = s.lastMonoHeld;
}
} // namespace

void Vc2KeyTable::init(uint32_t src, uint32_t ch) {
    source = (uint8_t)src;
    number = (uint8_t)ch;
    channel = keysTab().states[keysTab().sources[src] * Vc2Keys::kChannels + ch] + kChannelOffset;
    curve = 0;
    reset();
}

void Vc2KeyTable::reset() {
    wasReset = kWasReset;
    sostenuto = 0;
    latched = keysTab().zero;
    clear();
}

void Vc2KeyTable::noteOn(uint32_t key, uint32_t velocity) {
    onKey = (int8_t)key;
    onVelocity = keysTab().curves[curve * 128u + velocity];
    lastOn = 1;
    countUp(ons[key]);
    keyWord(held, key) |= keyBit(key);
}

void Vc2KeyTable::noteOff(uint32_t key, uint32_t velocity) {
    offKey = (int8_t)key;
    offVelocity = (uint8_t)velocity;
    lastOn = 0;
    if (ons[key] == 0) return;
    countUp(offs[key]);
    if (offs[key] == ons[key]) keyWord(held, key) &= (uint16_t)~keyBit(key);
}

void Vc2KeyTable::setHold(uint32_t value) { hold = (uint8_t)value; }

void Vc2KeyTable::setSostenuto(int32_t value) {
    if (((sostenuto ^ (uint32_t)value) & kSostenutoBit) != 0) latched = value >= kSostenutoDown ? held : keysTab().zero;
    sostenuto = (uint8_t)value;
}

void Vc2KeyTable::clear() {
    held = keysTab().zero;
    ons.fill(0);
    offs.fill(0);
}

float Vc2KeyTable::velocityFactor(uint32_t velocity) const {
    const int32_t v = keysTab().curves[curve * 128u + velocity];
    return xtf::div(xtf::flt(v * v), keysTab().divisor);
}

int32_t vc2KeysHighestIn(const Vc2KeyBits& bits, int32_t lo, int32_t hi) {
    int32_t word = hi >> 4;
    int32_t at = hi - (hi & 15);
    const uint32_t first = bits[(size_t)word] & keysTab().masks[(size_t)(hi & 15)];
    if (first != 0) return at + highestBit(first);
    for (at -= 1; at >= lo; at -= 16) {
        const uint32_t w = bits[(size_t)--word];
        if (w == 0) continue;
        const int32_t found = at - 15 + highestBit(w);
        return found >= lo ? found : kNone;
    }
    return kNone;
}

int32_t vc2KeysHighest(const Vc2KeyBits& bits) { return vc2KeysHighestIn(bits, 0, 127); }

void vc2KeysSet(Vc2KeyBits& bits, int32_t lo, int32_t hi) {
    for (int32_t k = lo; k <= hi; ++k) keyWord(bits, (uint32_t)k) |= keyBit((uint32_t)k);
}

void vc2KeysFill(Vc2KeyBits& bits) { bits.fill(0xffff); }

bool vc2KeysAny(const Vc2KeyBits& bits) {
    return std::ranges::any_of(bits, [](uint16_t w) { return w != 0; });
}

void Vc2KeyGlide::construct(uint32_t self) { last = self + kListOffset; }

void Vc2KeyGlide::reset() {
    prevPoly = kMiddle;
    lastPoly = kMiddle;
    prevPolyHeld = 0;
    lastPolyHeld = 0;
    prevMono = kMiddle;
    lastMono = kMiddle;
    prevMonoHeld = 0;
    lastMonoHeld = 0;
    window = kWindow;
}

void Vc2KeyGlide::noteOn(int32_t ms, const Vc2KeyTable& keys) {
    const uint32_t dt = (uint32_t)ms - (uint32_t)lastMs;
    XtKeyState s = glideState(*this, keys);
    s.noteOn(keys.onKey, ms);
    glideKeep(*this, s);
    since = (uint16_t)std::min(dt, kSinceMost);
}

int32_t Vc2KeyGlide::noteOff(const Vc2KeyTable& keys) {
    XtKeyState s = glideState(*this, keys);
    const int32_t found = s.noteOff(keys.offKey);
    glideKeep(*this, s);
    return found;
}

uint32_t Vc2Keys::tableAt(uint32_t source, uint32_t channel) {
    return kTablesAt + kSourceBytes * source + kTableBytes * channel;
}

Vc2KeyTable& Vc2Keys::table(uint32_t address) {
    const std::pair<uint32_t, Vc2KeyTable*> owned[2] = { { kSpareAt, &spare }, { kOwnAt, &own } };
    for (const auto& [at, keys] : owned)
        if (at == address) return *keys;
    return tables[(address - kTablesAt) / kTableBytes];
}

Vc2KeyGlide& Vc2Keys::glide(uint32_t address) {
    return address == kOwnGlideAt ? ownGlide : glides[(address - kGlidesAt) / kGlideBytes];
}

void Vc2Keys::resetObjectGlides(uint8_t index) {
    constexpr int kPerKey = 2;   // [FW]
    for (int s = 0; s < kSources; ++s)
        for (int n = 0; n < kPerKey; ++n) glides[(size_t)(kPerKey * (kChannels * s + index) + n)].reset();
}

void Vc2Keys::staticInit() {
    for (uint32_t c = 0; c < kChannels; ++c)
        for (uint32_t s = 0; s < kSources; ++s) table(tableAt(s, c)).init(s, c);
    own.reset();
}

void Vc2Keys::glidesInit() {
    const uint32_t perSource = kGlides / kSources;
    for (uint32_t n = 0; n < kGlides; ++n) {
        glides[n].reset();
        glides[n].table = tableAt(n / perSource, 0);
    }
    ownGlide.reset();
    ownGlide.table = kOwnAt;
}

void Vc2Keys::constructGlides() {
    for (uint32_t n = 0; n < kGlides; ++n) glides[n].construct(kGlidesAt + kGlideBytes * n);
    ownGlide.construct(kOwnGlideAt);
}
