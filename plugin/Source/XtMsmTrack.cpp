// A Multi Step Modulator track: its start, stop and clock, its six Directions and SMOOTH's line, and the tables they read
// from rom.2.  See XtMsm.h.  Integer arithmetic throughout, as the firmware's; each routine is named by its address.
#include "XtMsm.h"

#include <cstdio>

namespace {

constexpr uint32_t kRomBase = 0x89300000;
// u8 ×64: a step's |value| as semitones, 0…24 (0x894d98a0).  It reads past them, into the bytes after (other tables), for
// a value past ±63: SMOOTH's line goes there when an edit of the Grid leaves more clocks in a step than its Grid has.
constexpr uint32_t kCoarseTab = 0x897c49bc;
constexpr int kCoarseReach = 129;             // −128 reads its 128th
constexpr uint32_t kGridTab = 0x897c4a30;     // u8 ×8: a step's clocks by the Grid, 24, 12, 8, 6, 4, 3, then 0, 0
constexpr int kDestCoarse[] = { 0, 3, 17 };   // OSC1+2-PIT-CRS, OSC1-PIT-CRS, OSC2-PIT-CRS: their terms in semitones

struct Tables {
    uint8_t coarse[kCoarseReach] = {};
    uint8_t grid[8] = {};
    bool ok = false;
} T;

int gridClocks(const XtMsmTrack& t) {
    return T.grid[t.grid & 7];
}

// 0x894da2b0's test, inline at each place a term is made: the coarse tunes' steps go through 0x894d98a0
int8_t termOf(const XtMsmTrack& t, int v) {
    for (const int d : kDestCoarse)
        if (t.dest == d) return (int8_t)xtMsmCoarse(v);
    return (int8_t)v;
}

// 0x894da270 (and inline at 0x894da570): SMOOTH's line from the step to the next, by the clocks left in it
int smooth(const XtMsmTrack& t) {
    const int clocks = gridClocks(t);
    if (clocks == 0) return 0;                  // Grid 6 and 7 aren't in its range
    const int c = t.count;
    return (t.stepAt(t.next) * (clocks - c) + t.stepAt(t.cur) * c) / clocks;
}

void hold(XtMsmTrack& t, int step, int next) {
    t.next = (int8_t)next;
    t.cur = (int8_t)step;
}

void finish(XtMsmTrack& t) {
    if (t.state != 1) t.state = 1;
}

// Past the End Point without a loop it holds there, its state 1
void forwardEnd(XtMsmTrack& t, int c) {
    const int e = t.end;
    if (c > e) {
        t.cur = (int8_t)e;
        finish(t);
    } else if (c >= e) {
        hold(t, e, e);
    } else {
        t.next = (int8_t)(c + 1);
    }
}

// The six Directions (0x894d9db0, 0x894d9e30, 0x894d9f10, 0x894da010, 0x894da090, 0x894da170): the step after this one,
// and the one after that.  An End Point of 0 holds step 1 under a loop.
void fwd1(XtMsmTrack& t) {
    const int c = (int8_t)(t.cur + 1);
    const int e = t.end;
    t.cur = (int8_t)c;
    if (!t.loop) forwardEnd(t, c);
    else if (e == 0) hold(t, 0, 0);
    else if (c > e) hold(t, 0, 1);
    else if (c >= e) t.next = 0;
    else t.next = (int8_t)(c + 1);
}

// Up to the End Point and turning there (FWD3's and BWD3's after two steps there), or ending there without a loop
void forward(XtMsmTrack& t, int c, bool loops, bool twice) {
    const int e = t.end;
    if (!loops) {
        forwardEnd(t, c);
    } else if (e == 0) {
        hold(t, 0, 0);
    } else if (twice ? c > e : c >= e) {
        t.cur = (int8_t)e;
        t.delta = -1;
        t.next = (int8_t)(t.cur - 1);
    } else if (c >= e) {
        hold(t, e, e);
    } else {
        t.next = (int8_t)(c + 1);
    }
}

// Down to step 1 and turning there (FWD3's and BWD3's after two steps there), or ending there without a loop
void backward(XtMsmTrack& t, int c, bool loops, bool twice) {
    if (loops && t.end == 0) {
        hold(t, 0, 0);
    } else if (c > 0) {
        t.next = (int8_t)(c - 1);
    } else if (c >= 0 && (twice || !loops)) {
        hold(t, 0, 0);
    } else if (!loops) {
        t.cur = 0;
        finish(t);
    } else {
        t.cur = 0;
        t.delta = 1;
        t.next = (int8_t)(t.cur + 1);
    }
}

// FWD2 and FWD3 always turn at the End Point and end at step 1 without a loop; BWD2 and BWD3 the other way about
void pingPong(XtMsmTrack& t, bool fwd, bool twice) {
    const int c = (int8_t)(t.cur + t.delta);
    t.cur = (int8_t)c;
    if (t.delta >= 0) forward(t, c, fwd || t.loop, twice);
    else backward(t, c, !fwd || t.loop, twice);
}

void bwd1(XtMsmTrack& t) {
    const int c = (int8_t)(t.cur - 1);
    t.cur = (int8_t)c;
    if (!t.loop) backward(t, c, false, false);
    else if (t.end == 0) hold(t, 0, 0);
    else if (c > 0) t.next = (int8_t)(c - 1);
    else if (c >= 0) t.next = (int8_t)t.end;
    else hold(t, t.end, t.end - 1);
}

// 0x894da440's switch on the Direction: 1…5 by its table, anything else FWD1
void advance(XtMsmTrack& t) {
    switch (t.dir) {
    case 1: pingPong(t, true, false); break;
    case 2: pingPong(t, true, true); break;
    case 3: bwd1(t); break;
    case 4: pingPong(t, false, false); break;
    case 5: pingPong(t, false, true); break;
    default: fwd1(t); break;
    }
}

} // namespace

void XtMsmTrack::start() {
    if (state == 2) return;
    state = 2;
    if ((uint8_t)(dir - 3) > 2) {
        delta = 1;
        cur = 0;
        next = 1;
        if (end < 1) next = (int8_t)end;
    } else {
        delta = -1;
        cur = (int8_t)end;
        const int n = (int8_t)(end - 1);
        next = (int8_t)(n < 0 ? 0 : n);
    }
    count = (int8_t)gridClocks(*this);
    out = termOf(*this, stepAt(cur));
}

void XtMsmTrack::halt() {
    if (stopped()) return;
    state = 0;
    out = 0;
}

bool XtMsmTrack::clock() {
    if (stopped()) return false;
    count = (int8_t)(count - 1);
    if (count > 0) {                            // within the step: SMOOTH's line moves on
        if (!type) return false;
        out = termOf(*this, (int8_t)smooth(*this));
        return true;
    }
    count = (int8_t)gridClocks(*this);
    advance(*this);
    if ((uint8_t)(state - 1) > 1) {
        out = 0;
        return false;
    }
    out = termOf(*this, type ? (int8_t)smooth(*this) : stepAt(cur));
    return true;
}

bool xtMsmLoadTables(const char* romPath) {
    T.ok = false;
    FILE* f = romPath ? std::fopen(romPath, "rb") : nullptr;
    if (!f) return false;
    const bool read = std::fseek(f, (long)(kCoarseTab - kRomBase), SEEK_SET) == 0
                      && std::fread(T.coarse, 1, sizeof T.coarse, f) == sizeof T.coarse
                      && std::fseek(f, (long)(kGridTab - kRomBase), SEEK_SET) == 0
                      && std::fread(T.grid, 1, sizeof T.grid, f) == sizeof T.grid;
    std::fclose(f);
    if (!read) return false;
    // Sanity: the curve rises from 0 to two octaves; a step is a quarter's 24 clocks down to a 32nd's 3
    for (int i = 1; i < 64; ++i)
        if (T.coarse[i] < T.coarse[i - 1]) return false;
    if (T.coarse[0] != 0 || T.coarse[63] != 24 || T.grid[0] != 24 || T.grid[5] != 3) return false;
    T.ok = true;
    return true;
}

bool xtMsmTablesLoaded() {
    return T.ok;
}

int xtMsmCoarse(int v) {
    const int i = v >= 0 ? v : -v;
    const int c = i < kCoarseReach ? T.coarse[i] : 0;   // a byte's value reaches 128 at most
    return v >= 0 ? c : -c;
}
