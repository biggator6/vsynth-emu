// The arpeggiator's timer, step and what it sends (XtArpeggio.h): it starts at the first key, steps on the OS's tempo
// timer through the pattern, and sends a note-on with its note-off in the list by time, or a CTRL line's controller.
// Each routine is the firmware's at the address beside it; HARDWARE_FIRMWARE.md, "The arpeggiator".
#include "XtArpeggio.h"
#include <algorithm>
#include <cstring>
#include <iterator>

namespace {
constexpr int kPairTicks = 480;          // the shuffle's span, a quarter: positions are taken mod 480 [FW 0x89302724]
constexpr int32_t kFirstDelay = 15;      // the first step, 15 ticks after the first key [FW 0x8930241e]
constexpr int kNoteOn = 0x90, kNoteOff = 0x80, kControl = 0xb0;
constexpr int kOffVelocity = 64;         // [FW 0x89302b2c]
constexpr int kTie = 1;                  // a cell of 1 [FW 0x89302700]
constexpr int kCtrlLine = 0x80;          // a line's State bit [FW 0x893026e2]

// What resetControllers sends a CTRL line's controller back to; any other, 0 [FW 0x89303b7a]
struct CcRest {
    uint8_t cc;
    uint8_t value;
};
constexpr CcRest kCcRest[] = { { 7, 127 }, { 10, 64 }, { 93, 127 }, { 91, 127 }, { 11, 127 } };

int ccRestValue(int cc) {
    const auto* it = std::ranges::find(kCcRest, cc, &CcRest::cc);
    return it == std::end(kCcRest) ? 0 : it->value;
}
} // namespace

int xtArpGridTicks(int grid) {
    static constexpr int kTicks[6] = { 480, 240, 160, 120, 80, 60 };   // [FW 0x89301e90]
    return grid >= 0 && grid < 6 ? kTicks[grid] : 120;
}

void XtArpeggio::send(int status, int d1, int d2) {
    if (nEvents >= kMaxEvents) return;
    eventBuf[nEvents++] = { (uint8_t)status, (uint8_t)d1, (uint8_t)d2 };
}

// 0x89302400: on the tempo group (0x89304650; alone there, its phase is the timer's now), the first step 15 ticks on
void XtArpeggio::start() {
    timerMode = 0;
    pos = 0;
    base = now;
    restart(kFirstDelay);
    lastNote = -1;
    octave = 0;
    direction = 1;
    step = 0;
    octavePending = 0;
    started = 0;
    soundingCount = 0;
}

// 0x893024e0: every note ends, the keys are let go, no step comes (0x8930ea90)
void XtArpeggio::halt() {
    while (offHead >= 0) endNote(offHead);
    std::memset(held, 0, sizeof held);
    soundingCount = 0;
    next = kNever;
    step = 0;
    due = kNever;
}

// 0x89303480: from the pattern's start, `delay` ticks late
void XtArpeggio::restart(int32_t delay) {
    pos = 0;
    nominal = base + shuffle[0] * 4;
    next = nominal + delay;
    schedule(next);
}

// 0x8930ea10: the timer only ever comes sooner
void XtArpeggio::schedule(int32_t t) {
    due = std::min(due, t);
}

// 0x8930eb00, and the task's flag 0x200 (0x8930d53c)
void XtArpeggio::tick(int32_t ticks) {
    now = (int32_t)((uint32_t)now + (uint32_t)ticks);
    if (due > now) return;
    fired = due;
    due = kFiring;
    if (next < kNever) runStep();
}

// 0x89302570: every step due by the time the timer fired, then the notes that end by it.  A step's place is its pair's
// start and the shuffle's place for where in the pair it falls.
void XtArpeggio::runStep() {
    while (next <= fired) {
        if (started) {
            step = (uint8_t)(step + 1);
            if (step >= endPoint + 1) {
                if (octavePending == 1) {
                    octave = (int8_t)(octave + 1);
                    octavePending = 0;
                }
                step = 0;
            }
        }
        for (; line < XtArpPattern::kLines; ++line) {
            // a step past the pattern's 32 (the constructor's End Point, which no patch gives) reads nothing here
            const int c = step < XtArpPattern::kSteps ? pattern.cell[step][line] : 0;
            if (c == 0) continue;
            if (pattern.line[line] & kCtrlLine)
                ccLine();
            else if (c != kTie)
                playLine();
        }
        pos = (int32_t)((uint32_t)pos + (uint32_t)xtArpGridTicks(grid));
        const int32_t frac = pos >= 0 ? pos % kPairTicks : kPairTicks - (int32_t)((0u - (uint32_t)pos) % kPairTicks);
        const uint32_t pairStart = (uint32_t)base + (uint32_t)pos - (uint32_t)frac;
        next = nominal = (int32_t)(pairStart + shuffle[(frac >> 2) % kShuffle] * 4u);
        line = 0;
        started = 1;
    }
    schedule(next);
    while (offHead >= 0 && offs[offHead].time <= fired) endNote(offHead);
    schedule(offHead >= 0 ? offs[offHead].time : kNever);
}

// 0x89302840: a note, its velocity the key's (or Keyboard Velocity) scaled by the cell, sounding to the end of its ties
// and Duration's share of a step
void XtArpeggio::playNote(int note, int velocity) {
    while (note < -12) note += 24;
    if (note < 0) note += 12;
    while (note > 139) note -= 24;
    if (note > 127) note -= 12;
    const int scale = (pattern.cell[step % XtArpPattern::kSteps][line] - 1) & 0xff;
    const int vel = keyVelocity > 0 ? keyVelocity : velocity;
    int out = scale * (vel & 0xff) / 127;
    if ((out & 0xff) == 0) out = 1;
    XtArpOffNode& o = offs[note];
    if (o.time != kNever) endNote(note);
    if (channel >= 0) send(kNoteOn | channel, note, out);
    o.ch = channel;
    lastNote = (int8_t)note;
    int ties = 1;
    for (int s = step + 1; pattern.cell[s % (endPoint + 1) % XtArpPattern::kSteps][line] == kTie; ++s) ++ties;
    const int g = xtArpGridTicks(grid);
    const int len = std::max(1, g * duration * 99 / 10000);
    xtArpSetBit(sounding, note);
    ++soundingCount;
    const int32_t end = nominal + (int32_t)(uint16_t)((uint16_t)(ties - 1) * (uint16_t)g + len);
    if (end == kNever) return;
    o.time = end;
    linkOff(note);
}

// 0x893040f0: a note's end into the list by time, after any that end at the same tick
void XtArpeggio::linkOff(int n) {
    XtArpOffNode& o = offs[n];
    if (offHead < 0 || o.time < offs[offHead].time) {
        o.next = offHead;
        offHead = (int16_t)n;
        return;
    }
    int16_t at = offHead;
    while (offs[at].next >= 0 && o.time >= offs[offs[at].next].time) at = offs[at].next;
    o.next = offs[at].next;
    offs[at].next = (int16_t)n;
}

// 0x89304140
void XtArpeggio::unlinkOff(int n) {
    if (offHead == n) {
        offHead = offs[n].next;
        return;
    }
    int16_t at = offHead;
    while (at >= 0 && offs[at].next >= 0 && offs[at].next != n) at = offs[at].next;
    if (at >= 0 && offs[at].next == n) offs[at].next = offs[n].next;
}

// 0x89302b00
void XtArpeggio::endNote(int n) {
    XtArpOffNode& o = offs[n];
    if (o.ch >= 0) send(kNoteOff | o.ch, n, kOffVelocity);
    xtArpClearBit(sounding, n);
    --soundingCount;
    o.time = kNever;
    unlinkOff(n);
}

// 0x89302ba0: a CTRL line's controller, at the cell less 1
void XtArpeggio::ccLine() {
    const int cc = pattern.line[line] & 0x7f;
    const int value = pattern.cell[step % XtArpPattern::kSteps][line] - 1;
    if (channel >= 0) send(kControl | channel, cc, value);
    lineCc[line] = (uint8_t)cc;
    if (cc == 64 && value > 0) cc64 = 1;
    if (cc == 66 && value > 0) cc66 = 1;
}

// 0x89303b40: every controller a CTRL line sent goes back (not 0 or 32), and the pedals up
void XtArpeggio::resetControllers() {
    for (const uint8_t cc : lineCc) {
        if (cc == 0 || cc == 32) continue;
        if (cc == 64) cc64 = 0;
        if (cc == 66) cc66 = 0;
        if (channel >= 0) send(kControl | channel, cc, ccRestValue(cc));
    }
    if (cc64 && channel >= 0) send(kControl | channel, 64, 0);
    if (cc66 && channel >= 0) send(kControl | channel, 66, 0);
    std::memset(lineCc, 0, sizeof lineCc);
    cc64 = cc66 = 0;
}
