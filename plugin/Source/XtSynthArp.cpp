// XtSynth's arpeggiator (v189): the router in front of the part's notes, and the arpeggio stepping on the tempo clock's
// ticks each ms.  With its Switch on, played keys go to the arpeggio and not to the part (0x89307c80, the router's arpeggio
// mode), and what it sends reaches the part as played notes do (tgNoteOn, tgNoteOff, tgController).  The host's notes are
// the XT's own keys here, as with Remote Keyboard Switch on (ID 0x9b; the user's MIDI IN reaches the arpeggio only so).
// HARDWARE_FIRMWARE.md, "The arpeggiator in XtSynth".
#include "XtSynth.h"
#include <algorithm>
#include <cmath>
#include <iterator>

namespace {
constexpr int kHoldPedal = 64;
constexpr float kVelocityFull = 127.0f;
constexpr uint8_t kKind = 0xf0, kNoteOn = 0x90, kNoteOff = 0x80, kControl = 0xb0;
constexpr uint32_t kBlockMsByRate = 32 * 1000, kRate = 44100;   // a block's 32 frames as ms × 44100, as msDue counts them

int velocityOf(float v) {
    return (int)std::lround(std::clamp(v, 0.0f, 1.0f) * kVelocityFull);
}

void clearKeys(uint16_t (&map)[8]) {
    std::fill(std::begin(map), std::end(map), (uint16_t)0);
}
} // namespace

// 0x89307c80: a key its tracker (+0x80) already had (0x8930ad00 gives 0) goes to the arpeggio twice, a new one once;
// a velocity of 0 is the key's release
void XtSynth::noteOn(int note, float velocity) {
    arpSync();
    const int vel = velocityOf(velocity);
    if (!arp.on || vel == 0) {
        (arp.on ? noteOff(note, velocity) : tgNoteOn(note, velocity));
        return;
    }
    const bool known = xtArpBit(arpKeys, note);
    xtArpSetBit(arpKeys, note);
    if (known) arp.noteOn(note, vel);
    arp.noteOn(note, vel);
    arpApply();
}

// A key down at the part when the Switch went on has its note-off there (the choice here, not read in the firmware); a key
// of the arpeggio's goes to it if its tracker had it (0x8930ad40 gives 1), at velocity 64
void XtSynth::noteOff(int note, float velocity) {
    arpSync();
    if (xtArpBit(tgKeysAtSwitch, note) || !arp.on) {
        xtArpClearBit(tgKeysAtSwitch, note);
        tgNoteOff(note, velocity);
        return;
    }
    if (!xtArpBit(arpKeys, note)) return;
    xtArpClearBit(arpKeys, note);
    arp.noteOff(note);
    arpApply();
}

// The Hold pedal holds the arpeggio's keys while it's on (0x8930a360 into 0x89303a80; the manual's "hold pedal", the router's
// condition on it, this[0x179], not read); every other controller is the part's, as played
void XtSynth::controller(int number, int value) {
    arpSync();
    if (!arp.on || number != kHoldPedal) {
        tgController(number, value);
        return;
    }
    arp.pedal(std::clamp(value, 0, 127));
    arpApply();
}

// The panic: the arpeggio as built, given the patch's settings again at the next block
void XtSynth::allNotesOff() {
    arp.reset();
    arpGiven = {};
    patternGiven = {};
    clearKeys(arpKeys);
    clearKeys(tgKeysAtSwitch);
    tgAllNotesOff();
}

// An F8 from outside: the ticks go on to the next 20 (0x894712f0), then the count of six (clockCount)
void XtSynth::clockIn() {
    arpTicks.f8();
    clockCount();
}

// The patch's settings and pattern as 0x8930c190 and 0x8930ca30 give them, when they have changed.  A Switch going off stops
// it (setSettings) and its tracker lets go; going on, the part's held keys keep their own note-offs.
void XtSynth::arpSync() {
    if (!(ctl.arpPattern == patternGiven)) {
        arp.setPattern(ctl.arpPattern);
        patternGiven = ctl.arpPattern;
    }
    if (ctl.arp == arpGiven) return;
    const bool was = arp.on;
    arp.setSettings(ctl.arp);
    arpGiven = ctl.arp;
    if (arp.on && !was) std::copy(std::begin(keys.held), std::end(keys.held), tgKeysAtSwitch);
    if (!arp.on) clearKeys(arpKeys);
    arpApply();
}

// Each block: the ms it holds, each the clock object's ticks (0x894710e0) given to the tempo timer (0x894710b0), then the
// ms timer moves on.  After the clock's own ms and clocks in the block, before the tone update.
void XtSynth::arpBlock() {
    arpSync();
    for (int n = msDue(); n > 0; --n) arpMs();
    clockMsAcc = (clockMsAcc + kBlockMsByRate) % kRate;
}

void XtSynth::arpMs() {
    if (clockExt) arpTicks.msExternal(clockSix, clockSince);
    else arpTicks.msInternal(clockPeriod());
    const int32_t d = arpTicks.take();
    if (d != 0) arp.tick(d);
    arpApply();
}

// What it sent, to the part as the tone generator's input takes it (0x894d42e0): notes and controllers
void XtSynth::arpApply() {
    for (int i = 0; i < arp.eventCount(); ++i) {
        const XtArpEvent& e = arp.events()[i];
        switch (e.status & kKind) {
            case kNoteOn:
                tgNoteOn(e.d1, (float)e.d2 / kVelocityFull);
                break;
            case kNoteOff:
                tgNoteOff(e.d1, (float)e.d2 / kVelocityFull);
                break;
            case kControl:
                tgController(e.d1, e.d2);
                break;
            default:
                break;
        }
    }
    arp.clearEvents();
}
