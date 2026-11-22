// The VC-2 tone's envelope: see Vc2ToneEnvelope.h (v268).
#include "Vc2ToneEnvelope.h"
#include "XtEnvelope.h"
#include <algorithm>
#include <bit>
#include <cstring>

namespace {
constexpr int32_t kBuilt = -1, kShaped = 2;              // [FW] the constructor's stage; the curve 0x893abd30 serves
constexpr float kStartLevel = 1.0f;                      // [FW] 0x893abe54: +0x48, +0x4c and +0x58
constexpr uint32_t kStartCoef = 0x3c199999;              // [FW] 0x8999965c, as 0x893ac4a0 sets it
constexpr uint32_t kBlockBytes = 14 * sizeof(uint32_t);  // [FW] 0x893ac1c0 copies the block to +0x08…+0x3f
constexpr uint32_t kNoVirtual = 0xffffffff;              // [FW] the member pointers' second word, at 0x8993d228
constexpr uint32_t kLinearOutput = 0x893ab860, kShapedOutput = 0x893abd30;   // [FW] curves 0 and 2
// [FW] 0x893ac118…0x893ac160: the settings' copies, by the block's words: the scale, the four times, the five levels
constexpr uint32_t kScaleWord = 1, kTimesWord = 5, kLevelsWord = 9;
} // namespace

void Vc2ToneEnvelope::load(XtEnvelope& e) const {
    e.p.curve = w.curve;
    e.p.scale = w.scale;
    std::ranges::copy(w.time, e.p.time);
    std::ranges::copy(w.level, e.p.level);
    e.st = w.stage;
    e.rate = w.rate;
    e.phase = w.phase;
    e.out = w.out;
    e.rel = w.release;
    e.coef = w.coef;
    e.onset = w.onset;
    e.stT = w.stageTime;
}

void Vc2ToneEnvelope::store(const XtEnvelope& e) {
    w.stage = e.st;
    w.rate = e.rate;
    w.phase = e.phase;
    w.out = e.out;
    w.release = e.rel;
    w.coef = e.coef;
    w.onset = e.onset;
    w.stageTime = e.stT;
}

void Vc2ToneEnvelope::construct() {
    w.stage = kBuilt;
    w.unread = w.out = w.onset = kStartLevel;
    w.coef = std::bit_cast<float>(kStartCoef);
    w.on = 0;
}

void Vc2ToneEnvelope::noteOn(const uint32_t* block) {
    std::memcpy(&w.curve, block, kBlockBytes);
    w.output = { 0, kNoVirtual, w.curve == kShaped ? kShapedOutput : kLinearOutput };
    XtEnvelope e;
    load(e);
    e.noteOn(e.p);
    store(e);
}

void Vc2ToneEnvelope::tick(int32_t n) {
    XtEnvelope e;
    load(e);
    e.tick(n);
    store(e);
}

void Vc2ToneEnvelope::settings(const uint32_t* block) {
    w.scale = std::bit_cast<float>(block[kScaleWord]);
    std::memcpy(w.time.data(), block + kTimesWord, sizeof w.time);
    std::memcpy(w.level.data(), block + kLevelsWord, sizeof w.level);
    XtEnvelope e;
    load(e);
    e.update(e.p);   // the words as they now stand
    store(e);
}

void Vc2ToneEnvelope::release() {
    XtEnvelope e;
    load(e);
    e.noteOff();
    store(e);
}
