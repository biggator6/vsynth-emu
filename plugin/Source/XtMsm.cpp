// The Multi Step Modulator's object: its settings, start and stop, its timer, its clocks and its terms.  See XtMsm.h; the
// tracks' own routines are XtMsmTrack.cpp's.  Each routine is named by its address.
#include "XtMsm.h"

int* xtMsmField(XtMsmSettings& s, int id) {
    if (id == kXtMsmFirstId) return &s.master;
    if (id == kXtMsmFirstId + 1) return &s.keySync;
    const int k = id - (kXtMsmFirstId + 2);
    if (k < 0 || k >= kXtMsmTrackIds * XtMsmSettings::kTracks) return nullptr;
    XtMsmTrackSettings& t = s.track[k / kXtMsmTrackIds];
    const int f = k % kXtMsmTrackIds;
    int* const fields[] = { &t.on, &t.loop, &t.type, &t.grid, &t.endPoint, &t.direction, &t.zone, &t.dest };
    constexpr int kFields = sizeof fields / sizeof fields[0];
    return f < kFields ? fields[f] : &t.steps[f - kFields];
}

void XtMsm::reset() {
    *this = XtMsm();
}

// 0x894d9988… and 0x894d9b4c…: the Master Switch (a change to OFF stops it), Key Sync, then each track's by its IDs.  A
// track loops by its Loop Switch only under Key Sync.
void XtMsm::read(const XtMsmSettings& s) {
    const uint8_t m = (uint8_t)s.master;
    if (master != m) {
        master = m;
        if (!m) stop();
    }
    keySync = (uint8_t)s.keySync;
    for (int n = 0; n < kTracks; ++n) {
        const XtMsmTrackSettings& a = s.track[n];
        XtMsmTrack& t = track[n];
        t.on = (uint8_t)a.on;
        loops[n] = (uint8_t)a.loop;
        t.loop = keySync ? loops[n] : 1;
        t.type = (uint8_t)a.type;
        t.grid = (uint8_t)a.grid;
        t.end = (uint8_t)a.endPoint;
        t.dir = (uint8_t)a.direction;
        t.zone = (uint8_t)a.zone;
        t.dest = (uint8_t)a.dest;
        for (int k = 0; k < 16; ++k) t.steps[k] = (int8_t)a.steps[k];
    }
}

void XtMsm::load(const XtMsmSettings& s, int part) {
    if (part != 0) return;
    stop();
    read(s);
}

void XtMsm::edit(const XtMsmSettings& s, int part) {
    if (part != 0) return;
    read(s);
}

void XtMsm::start() {
    if (running == 1) return;
    pos = (int32_t)((uint32_t)now + kTicksPerClock);
    if (due > pos) due = pos;                   // 0x8930ea10: a timer is set only sooner
    running = 1;
    for (XtMsmTrack& t : track) t.start();
    ++updates;
}

void XtMsm::stop() {
    if (!running) return;
    due = kNever;                               // 0x8930ea90
    running = 0;
    for (XtMsmTrack& t : track) t.halt();
    ++updates;
}

void XtMsm::noteOn(int part, bool keyHeld) {
    if (!master || part != 0) return;
    switch (keySync) {
    case 0:                                     // OFF: started by the first note, then never again
        start();
        break;
    case 1:                                     // PART: again from the top when no key is down, unless the arpeggio plays
        if (arp) {
            start();
        } else if (!keyHeld) {
            stop();
            start();
        }
        break;
    case 2:                                     // VOICE: again from the top at every note
        stop();
        start();
        break;
    default:
        break;
    }
}

void XtMsm::tick(int32_t ticks) {
    now = (int32_t)((uint32_t)now + (uint32_t)ticks);
    if (due > now) return;
    fired = due;
    due = kFiring;
    ++pending;                                  // 0x894d9510: a clock, and the timer 20 ticks on from its last time
    pos = (int32_t)((uint32_t)pos + kTicksPerClock);
    if (due > pos) due = pos;
}

bool XtMsm::clockAll() {
    bool moved = false;
    for (XtMsmTrack& t : track)
        if (t.clock()) moved = true;
    return moved;
}

void XtMsm::clocks(bool sounding) {
    if (!sounding && keySync && !arp) {
        stop();
        return;
    }
    const int32_t n = pending;
    pending = 0;
    bool moved = false;
    for (int32_t i = n; i > 0; --i)
        if (clockAll()) moved = true;
    if (moved) ++updates;
}

int XtMsm::term(int dest, int part, int zone) const {
    if (!running || part != 0) return 0;
    int sum = 0;
    for (const XtMsmTrack& t : track)
        if (t.dest == dest && t.zone == zone && t.on) sum += t.out;
    return sum;
}

int XtMsm::termAll(int dest) const {
    if (!running) return 0;
    int sum = 0;
    for (const XtMsmTrack& t : track)
        if (t.dest == dest && t.on) sum += t.out;
    return sum;
}

int XtMsm::termAnyZone(int dest, int part) const {
    return part != 0 ? 0 : (int16_t)termAll(dest);
}

int XtMsm::shownStep(int n) const {
    const XtMsmTrack& t = track[n & 3];
    return t.stopped() ? -1 : t.cur;
}
