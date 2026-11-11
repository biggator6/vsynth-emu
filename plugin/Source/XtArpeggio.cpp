// The arpeggiator's keys and settings (XtArpeggio.h): the keys it holds, in the order played and by key, Hold and the
// pedal, the shuffle's table.  The timer, the step and what it sends are XtArpeggioStep.cpp's; which key a step plays,
// XtArpeggioMotifs.cpp's.  Each routine is the firmware's at the address beside it; HARDWARE_FIRMWARE.md, "The arpeggiator".
#include "XtArpeggio.h"
#include <algorithm>
#include <cstring>

namespace {
constexpr uint8_t kNoKey = 0x80;         // +0x718 with no key [FW 0x893038d4]
constexpr int kPedalDown = 64;           // the Hold pedal at or past it holds [FW 0x89303866]
} // namespace

// The fields as declared, each key's node holding its key (0x89301d20), the shuffle at 50 (0x89304200)
XtArpeggio::XtArpeggio() {
    for (int k = 0; k < kKeys; ++k) keys[k].key = (uint8_t)k;
    makeShuffle(rate, res);
}

void XtArpeggio::reset() {
    *this = XtArpeggio();
}

// 0x8930c190's part for one object: a change of Switch stops it first, then the settings and their shuffle
void XtArpeggio::setSettings(const XtArpSettings& s) {
    if (s.on != on) {
        on = s.on;
        stop();
    }
    hold = (uint8_t)s.hold;
    motif = (uint8_t)s.motif;
    rate = (uint8_t)s.shuffleRate;
    res = (uint8_t)s.shuffleRes;
    duration = (uint8_t)s.duration;
    grid = (uint8_t)s.grid;
    endPoint = (uint8_t)s.endPoint;
    octaveRange = (int8_t)s.octave;
    keyVelocity = (uint8_t)s.keyVelocity;
    makeShuffle(rate, res);
    pedalCheck();
    // 0x893033d0: running off the tempo group, it joins it and starts its phase again.  Once started it is on it.
    if (running() && timerMode != 0) {
        timerMode = 0;
        restart(0);
    }
}

// 0x89304430: where each 4-tick place of a 480-tick pair falls, by the Shuffle Rate (1…99) over a pair of 8ths, or of
// 16ths twice.  At 50 each place falls where it is.
void XtArpeggio::makeShuffle(int r, int s) {
    r = std::clamp(r, 1, 99);
    if (shuffleRate == r && shuffleRes == s) return;
    shuffleRate = (uint8_t)r;
    shuffleRes = (uint8_t)s;
    if ((s & 0xff) == 1) {
        const int span = 120 * r / 100;   // the first 8th's end, /4
        for (int k = 0; k < 60; ++k) shuffle[k] = (uint8_t)(k * span / 60);
        for (int k = 60; k < 120; ++k) shuffle[k] = (uint8_t)(2 * span + 2 * k - k * span / 60 - 120);
        return;
    }
    const int span = 60 * r / 100;        // the first 16th's end, /4
    for (int k = 0; k < 30; ++k) shuffle[k] = (uint8_t)(k * span / 30);
    for (int k = 30; k < 60; ++k) shuffle[k] = (uint8_t)(2 * span + 2 * k - k * span / 30 - 60);
    for (int k = 0; k < 30; ++k) shuffle[60 + k] = (uint8_t)(k * span / 30 + 60);
    for (int k = 30; k < 60; ++k) shuffle[60 + k] = (uint8_t)(2 * span + 2 * k - k * span / 30);
}

bool XtArpeggio::anyHeld() const {
    return std::ranges::any_of(held, [](uint16_t w) { return w != 0; });
}

// 0x893040a0: a key at the end of the order played
void XtArpeggio::linkKey(int k) {
    XtArpKeyNode& n = keys[k];
    if (keyHead < 0) {
        n.prev = n.next = keyHead = (int16_t)k;
        return;
    }
    const int16_t tail = keys[keyHead].prev;
    n.prev = tail;
    n.next = keyHead;
    keys[keyHead].prev = (int16_t)k;
    keys[tail].next = (int16_t)k;
}

// 0x893040c0: a key out of it.  Taking out the first makes the one before it (the last played) the first.
void XtArpeggio::unlinkKey(int k) {
    const int16_t prev = keys[k].prev;
    if (keyHead == k) {
        if (prev == k) {
            keyHead = -1;
            return;
        }
        keyHead = prev;
    }
    keys[prev].next = keys[k].next;
    keys[keys[k].next].prev = prev;
}

// Every key out of it (0x893038a8, 0x89303af6)
void XtArpeggio::dropKeys() {
    while (keyHead >= 0) {
        keys[keyHead].vel = 0;
        unlinkKey(keyHead);
    }
    curKey = kNoKey;
    curNode = -1;
}

// 0x89303830
void XtArpeggio::noteOn(int key, int velocity) {
    key &= 127;
    lastVelocity = (uint8_t)velocity;
    if (keyHead < 0)
        start();
    else if ((hold != 0 || pedalValue >= kPedalDown) && !anyHeld())
        dropKeys();   // held by Hold or the pedal, a new chord replaces the last
    xtArpSetBit(held, key);
    XtArpKeyNode& n = keys[key];
    if (n.vel != 0) {
        if (curNode == key) curNode = n.prev;
        n.vel = 0;
        unlinkKey(key);
        if (keyHead < 0) {
            curKey = kNoKey;
            curNode = -1;
        }
    }
    if (velocity <= 0) return;
    n.vel = (uint8_t)velocity;
    linkKey(key);
}

// 0x893039c0: under Hold or the pedal the key stays
void XtArpeggio::noteOff(int key) {
    key &= 127;
    xtArpClearBit(held, key);
    XtArpKeyNode& n = keys[key];
    if (hold != 0 || pedalValue >= kPedalDown || n.vel == 0) return;
    if (curNode == key) curNode = n.prev;
    n.vel = 0;
    unlinkKey(key);
    if (keyHead >= 0) return;
    curKey = kNoKey;
    curNode = -1;
    halt();
    resetControllers();
}

// 0x89303a80
void XtArpeggio::pedal(int value) {
    pedalValue = (uint8_t)value;
    pedalCheck();
}

// 0x89303380: with neither Hold nor the pedal, the keys let go leave (0x89301dd0, which leaves +0x71c as it is), and
// with none left it stops
void XtArpeggio::pedalCheck() {
    if (hold != 0 || pedalValue >= kPedalDown) return;
    for (int k = 0; k < kKeys; ++k) {
        if (keys[k].vel == 0 || xtArpBit(held, keys[k].key)) continue;
        keys[k].vel = 0;
        unlinkKey(k);
    }
    if (keyHead < 0) halt();
}

// 0x89303ab0
void XtArpeggio::stop() {
    std::memset(held, 0, sizeof held);
    pedalValue = 0;
    dropKeys();
    soundingCount = 0;
    halt();
    resetControllers();
}
