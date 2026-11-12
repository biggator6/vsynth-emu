#pragma once
// xt_tone's firmware side (XT_TONE_MAIN only): the firmware's tone in the interpreter, from the parameter IDs on, its notes
// and its waves.  Moved from XtToneCheck.cpp as it stood (v193), for its MSM mode (XtToneMsm.cpp) to share.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <unordered_map>
#include <vector>
#include "Sh4Interp.h"
#include "XtFirmware.h"

const uint32_t kRam = 0x8C000000, kSp = kRam + 0xF0000, kN = kRam + 0x1000, kK = kRam + 0x1400, kMsg = kRam + 0x1500, kA1 = kRam + 0x1510,
               kNote = kRam + 0x1600, kCtl = kRam + 0x1800, kBuf = kRam + 0x2000, kOs = kRam + 0x3000;
constexpr uint32_t kBuf1 = 0x89d2c810, kBuf2 = 0x89d2c910, kBufMix = 0x89d2ca10;
constexpr uint32_t kFmLines = 0x89f91c5c;     // FM's line pool, XtModShared

constexpr uint32_t kDescTab = 0x897a167c, kRomBase = 0x89300000;
constexpr int kCosm2 = 0x10000;      // COSM 2's IDs in the patch table: COSM 1's plus this
constexpr uint32_t kPadCcX = 16, kPadCcY = 17;   // for the plugin's knob, which has no controller: the pad's handler on numbers of its own
constexpr uint32_t kClockObj = 0x89979bfc;       // the tempo clock's object

// A wave in the firmware's memory: its chunks (2 the header, 3 the steps, 4 the
// loop record, 5 the vpdt, 10 the cached loop events) and the bands the players read.
struct FwWave {
    uint32_t chunk[11] = {}, band[7] = {};
    long frames = 0;
    bool loaded = false;
};

// One note on the firmware's side: slot idx of the note objects, block and input pools.
struct FwNote {
    uint32_t note = 0, BL = 0, IN = 0, TN = 0;
    int raw = -1, vi = -1;              // the key as played; XtSynth's voice
    bool live = false, released = false, ended = false;
    bool off = false;                   // released (0x894dc270): off the allocator's list of notes, which a truncated note isn't
};

class Firmware {
public:
    Sh4 c;
    std::vector<uint8_t> rom;
    std::unordered_map<int, int> par;   // the patch, by parameter ID
    // Zones 2…16's own (zpar[z], empty for none: zone 1's then), what the getter answers for an ID of a zone's blocks when the read
    // object it is given was pointed at that zone (the routines at 0x894796d0…0x89479920, hooked to leave kZoneTag | zone there)
    std::vector<std::unordered_map<int, int>> zpar;
    long frames = 0;                    // the system clock
    FwWave wave, wave2;                 // the waves the Wave Numbers find: 1 OSC 1's, 2 OSC 2's
    std::unordered_map<uint32_t, FwWave> zoneWave;   // and zones 2…16's, by 2999 + the number
    int upper[16] = { 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };   // the zones' upper keys (IDs 0x124…0x133)
    int bender = 8192;
    int cosmSection = 0;
    uint32_t cosmBuf = 0;
    uint16_t held[8] = {};
    XtFwBandReads bandReads;
    // FR11 as the stereo Vari off player (0x89509f52) finds it, by its player object: what its backward Alternate hold compares the fade with
    // (XtLinearPlayer::setStaleFr11), left there by whatever ran before.  Taken each block, and given to the port's player before its block
    std::unordered_map<uint32_t, uint32_t> fr11;
    const FwWave& waveOf(uint32_t id) const {
        if (id == 2999 + 1) return wave;
        if (id == 2999 + 2) return wave2;
        static const FwWave none;
        const auto it = zoneWave.find(id);
        return it != zoneWave.end() ? it->second : id > 2999 + 2 ? none : wave;
    }
    // The IDs of a zone's blocks: the tone block, the envelopes, the LFOs and the COSMs (XtFiles' sections with a stride)
    static bool zoned(int id) { return (id >= 0x293 && id <= 0x3e8) || (id >= 0x8d0 && id <= 0x954); }
    static constexpr uint32_t kZoneTag = 0x7a5a0000;
    // The zone a key plays (the part's note-on, 0x894dbcb0): the first of the part's table (+6, from 0x894794f0) it doesn't pass
    int zoneOf(int key) {
        for (uint32_t z = 0; z < 16; ++z)
            if (!(key > (int)c.rd8(0x89d021d0 + 6 + z))) return (int)z;
        return 0;
    }

    float f32(uint32_t a) { const uint32_t v = c.rd32(a); float x; std::memcpy(&x, &v, 4); return x; }
    static uint32_t envAt(const FwNote& n, uint32_t e) { return n.TN + 0x1780 + 0x70 * e; }
    static uint32_t lfoAt(const FwNote& n, uint32_t l) { return n.TN + 0x1da0 + 0x98 * l; }
    static uint32_t voice(const FwNote& n, int o) { return n.TN + (o ? 0x9d4 : 0x20); }
    static uint32_t tva(const FwNote& n) { return n.TN + 0x1650; }

    // The value the getter returns for an ID with no setting: default − centre (the second
    // bank, from ID 0x55d: its default).
    int defaultOf(int id) const {
        if (id >= 0x55d) {
            const size_t o2 = (size_t)(0x897a56d8u + 8u * (uint32_t)(id - 0x55d) + 2 - kRomBase);
            if (o2 + 2 > rom.size()) return 0;
            int16_t d;
            std::memcpy(&d, rom.data() + o2, 2);
            return d;
        }
        const size_t o = (size_t)(kDescTab + 12u * (uint32_t)id + 4 - kRomBase);
        if (o + 4 > rom.size()) return 0;
        int16_t d[2];
        std::memcpy(d, rom.data() + o, 4);
        return d[1] - d[0];
    }

    bool open(const char* path);                     // XtToneFirmware.cpp

    bool updatePatch() {
        // Out Gain for TG and Output Mode, as the system updater (0x894eb2f0) stores them
        c.wr8(0x89d2a6d0 + 0x1600, (uint8_t)par.at(0x526));
        c.wr8(0x89d2a6d0 + 0x1604, (uint8_t)par.at(0x40));
        if (!c.call(0x894dfe10, { 0 }, kSp)) { std::fprintf(stderr, "updaters: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894e9db0, { 0x89d276b4u }, kSp)) { std::fprintf(stderr, "system assigns: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894d82d0, {}, kSp)) { std::fprintf(stderr, "pad's assigns: %s\n", c.error().c_str()); return false; }   // Time Trip Assign X, Y, Beam 1 Assign L, R
        // the task's flag 0x40 (0x89471d34): Clock Source and the Patch Tempo to the clock object
        if (!c.call(0x89471e30, {}, kSp) || !c.call(0x89471ef0, {}, kSp)) { std::fprintf(stderr, "patch tempo: %s\n", c.error().c_str()); return false; }
        return true;
    }
    // A clock (0x894f4da0), `blocks` the blocks rendered so far, whose count it reads (0x8987c7a8)
    bool clock(uint32_t blocks) {
        c.wr32(0x8987c7a8, blocks);
        if (!c.call(0x894f4da0, {}, kSp)) { std::fprintf(stderr, "clock: %s\n", c.error().c_str()); return false; }
        return true;
    }
    // The pad as its two controllers reach the part (0x894d8360); the next update sees a touch
    bool pad(int x, int y) {
        const uint8_t ax = c.rd8(0x89cf69a4), ay = c.rd8(0x89cf69a5);
        c.wr8(0x89cf69a4, kPadCcX); c.wr8(0x89cf69a5, kPadCcY);
        const bool ok = c.call(0x894d8360, { 0, kPadCcX, (uint32_t)x }, kSp) && c.call(0x894d8360, { 0, kPadCcY, (uint32_t)y }, kSp);
        c.wr8(0x89cf69a4, ax); c.wr8(0x89cf69a5, ay);
        if (!ok) std::fprintf(stderr, "pad: %s\n", c.error().c_str());
        return ok;
    }
    // A controller's last stop, whatever its number (0x894d6f10 → 0x894d8360)
    bool padCc(int number, int value) {
        if (!c.call(0x894d8360, { 0, (uint32_t)number, (uint32_t)value }, kSp)) { std::fprintf(stderr, "pad cc: %s\n", c.error().c_str()); return false; }
        return true;
    }
    bool padOn() { return c.rd32(c.rd32(0x8987ae78) + 8) != 0; }
    int padRadius() { return (int8_t)c.rd8(c.rd32(0x8987ae78) + 0x24); }
    // A millisecond of the timer routine (0x89471030, 0x894710e0)
    bool timer() {
        if (!c.call(0x89471030, {}, kSp)) { std::fprintf(stderr, "timer: %s\n", c.error().c_str()); return false; }
        return true;
    }
    // An F8 reaching the clock's object (0x894712f0)
    bool clockIn() {
        if (!c.call(0x894712f0, { kClockObj, 0xf8 }, kSp)) { std::fprintf(stderr, "f8: %s\n", c.error().c_str()); return false; }
        return true;
    }
    // Clock Source, from what it was: the object as it is after being sent there from the other
    bool clockSource(int source) {
        par[0x41] = source;
        if (!c.call(0x89471e30, {}, kSp)) { std::fprintf(stderr, "clock source: %s\n", c.error().c_str()); return false; }
        return true;
    }
    // The firmware's own timer routine `ms` times at this Patch Tempo: the clock object's ticks (+0x28),
    // of which 20 make a clock (0x894d4210)
    bool msTicks(int tempo, int ms, uint32_t& ticks) {
        par[0x11d] = tempo;
        if (!clockSource(0)) return false;
        if (!c.call(0x89471ef0, {}, kSp)) { std::fprintf(stderr, "patch tempo: %s\n", c.error().c_str()); return false; }
        const uint32_t from = c.rd32(kClockObj + 0x28);
        for (int i = 0; i < ms; ++i)
            if (!timer()) return false;
        ticks = c.rd32(kClockObj + 0x28) - from;
        return true;
    }
    bool partMono() { return c.rd8(0x89d021d0 + 1) != 0; }
    bool partLegato() { return c.rd8(0x89d021d0 + 2) != 0; }
    bool heldBefore() { return c.rd8(kK + 0x21) != 0; }

    // The key state's note-on (0x894d6330).
    bool keyOn(int raw, int vel) {
        held[raw >> 4] |= (uint16_t)(1u << (raw & 15));
        c.wr16(kN + 0xc + 2 * (uint32_t)(raw >> 4), held[raw >> 4]);
        c.wr8(kN + 7, (uint8_t)raw); c.wr8(kN + 8, (uint8_t)vel);
        c.wr16(kCtl, (uint16_t)bender);
        if (!c.call(0x894d6330, { kK }, kSp)) { std::fprintf(stderr, "key state: %s\n", c.error().c_str()); return false; }
        return true;
    }

    // The note's start and the tone's note-on.
    // `prev`: the allocator's earlier note of the part and zone (0x894d4c04), or none
    template <class Carry> bool start(FwNote& n, int idx, int key, int zone, Carry carry, const FwNote* prev = nullptr) {
        n.note = kNote + 0x100 * (uint32_t)idx;
        c.wr8(kMsg, 0);                                // the part
        c.write(n.note, std::vector<uint8_t>(0x100, 0).data(), 0x100);
        c.wr32(n.note + 0x10, 0x89833e44);
        c.wr8(n.note + 0x40, (uint8_t)idx);
        c.wr8(n.note + 0x52, 0xff);
        // as the allocation leaves a note (0x894d5b00): its state word all set, +0x1c at it.  The key's release clears bit 29 (0x894d5932)
        c.wr32(n.note + 0x20, 0xffffffff);
        c.wr32(n.note + 0x1c, n.note + 0x20);
        // the allocator's three bytes: an earlier note found, its key down (its state word's bit 29), and its chain key (+0x6c4), or this key
        const uint8_t chain = prev ? c.rd8(prev->BL + 0x6c4) : (uint8_t)key;
        c.wr8(kA1, prev ? 1 : 0); c.wr8(kA1 + 1, prev && (c.rd32(prev->note + 0x20) & 0x20000000) ? 1 : 0); c.wr8(kA1 + 2, chain);
        c.wr32(kSp, kA1); c.wr32(kSp + 4, (uint32_t)zone);
        if (!c.call(0x894dc130, { n.note, kK, kMsg, (uint32_t)key }, kSp)) { std::fprintf(stderr, "note: %s\n", c.error().c_str()); return false; }
        n.BL = c.rd32(n.note + 0x4c); n.IN = c.rd32(n.note + 0x5c);
        n.TN = c.rd32(n.BL + 0x6bc);
        n.raw = c.rd8(kK + 0xc);
        n.live = true; n.released = n.ended = n.off = false;
        carry(n);
        if (!c.call(0x894db830, { n.BL, prev ? prev->BL : 0, (uint32_t)(int8_t)chain }, kSp)) { std::fprintf(stderr, "tone note-on: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894f4f70, { n.BL }, kSp)) { std::fprintf(stderr, "tone start: %s\n", c.error().c_str()); return false; }
        return true;
    }

    // A sounding note takes a key (0x894dc1e0, as 0x894d4e00 calls it).
    bool legato(FwNote& n, int key) {
        c.wr16(kCtl, (uint16_t)bender);
        if (!c.call(0x894dc1e0, { n.note, kK, kMsg, (uint32_t)key }, kSp)) { std::fprintf(stderr, "legato: %s\n", c.error().c_str()); return false; }
        n.raw = c.rd8(kK + 0xc);
        return true;
    }

    bool truncate(FwNote& n) {
        if (!c.call(0x894d5ba0, { n.note }, kSp)) { std::fprintf(stderr, "truncate: %s\n", c.error().c_str()); return false; }
        n.released = true;
        return true;
    }

    // A knob moved, or the bender: the part's tables again (0x894dfe10), then the note's
    // update (0x894dc0b0): the input tables, the conversion pass, the blocks to the tone.
    bool liveUpdate(FwNote& n) {
        c.wr16(kCtl, (uint16_t)bender);
        if (!c.call(0x894dc0b0, { n.note }, kSp)) { std::fprintf(stderr, "note update: %s\n", c.error().c_str()); return false; }
        return true;
    }

    // The key state's note-off (0x894d63c0): the key a MONO part goes back to, or −1.
    bool keyOff(int raw, int relVel, int& back) {
        held[raw >> 4] &= (uint16_t)~(1u << (raw & 15));
        c.wr16(kN + 0xc + 2 * (uint32_t)(raw >> 4), held[raw >> 4]);
        c.wr8(kN + 9, (uint8_t)raw);
        c.wr8(kN + 0xa, (uint8_t)relVel);
        uint32_t r0 = 0;
        if (!c.call(0x894d63c0, { kK, 0, 0x7f }, kSp, &r0)) { std::fprintf(stderr, "key off: %s\n", c.error().c_str()); return false; }
        back = (int32_t)r0;
        return true;
    }

    // Whether the note has stopped: its three reasons to sound are gone (0x894dc270 has run).
    void released(FwNote& n) { if (!(c.rd8(n.note + 0x48) & 7)) n.released = n.off = true; }
    // A key let go (0x894d58b0), the release velocity at N + 0xa.
    bool keyRelease(FwNote& n) {
        if (!c.call(0x894d58b0, { n.note, kN }, kSp)) { std::fprintf(stderr, "key release: %s\n", c.error().c_str()); return false; }
        released(n);
        return true;
    }
    // Hold or Sostenuto as the handlers (0x894d77f0, 0x894d7880) set them; a pedal let go goes over the notes
    // (0x894d4ec0, 0x894d4f00, whose list of notes is ours here).
    bool pedal(int number, int value, FwNote* fn, int nNotes) {
        if (!c.call(number == 64 ? 0x894d5fc0 : 0x894d5fd0, { kN, (uint32_t)value }, kSp)) { std::fprintf(stderr, "pedal: %s\n", c.error().c_str()); return false; }
        if (value >= 64) return true;
        for (int i = 0; i < nNotes; ++i) {
            FwNote& n = fn[i];
            if (!n.live || n.released) continue;
            if (!c.call(number == 64 ? 0x894d5970 : 0x894d59d0, { n.note }, kSp)) { std::fprintf(stderr, "pedal up: %s\n", c.error().c_str()); return false; }
            released(n);
        }
        return true;
    }
    // A mode message's reset of the key state (0x894d6040, 0x894d6f80)
    bool keysCleared() {
        std::fill(std::begin(held), std::end(held), (uint16_t)0);
        if (!c.call(0x894d6040, { kN }, kSp)) { std::fprintf(stderr, "keys cleared: %s\n", c.error().c_str()); return false; }
        for (uint32_t o : { 0x1bu, 0x1du, 0x21u, 0x23u }) c.wr8(kK + o, 0);
        return true;
    }

    // The scheduler's step (0x894f4d10): the free-running LFOs, then the tones.
    bool tickFree() {
        if (!c.call(0x894f1540, { 0x89fa90b0u, 1 }, kSp)) { std::fprintf(stderr, "free lfos: %s\n", c.error().c_str()); return false; }
        return true;
    }
    bool tick(FwNote& n) {
        if (!c.call(0x894f75f0, { n.TN, 1 }, kSp)) { std::fprintf(stderr, "update: %s\n", c.error().c_str()); return false; }
        return true;
    }

    // False: the firmware read outside all memory.
    // FM's tap far past full scale reads below its line pool, among the lists of sounding
    // tones: on the XT whatever is there, in the port 0
    bool render(FwNote& n) {
        uint32_t below[8];
        for (uint32_t i = 0; i < 8; ++i) { below[i] = c.rd32(0x89f91c3c + 4 * i); c.wr32(0x89f91c3c + 4 * i, 0); }
        const bool ok = c.call(c.rd32(n.TN + 0x177c), { n.TN, kBuf }, kSp);
        for (uint32_t i = 0; i < 8; ++i) c.wr32(0x89f91c3c + 4 * i, below[i]);
        return ok;
    }
    void clearBus() { c.write(kBuf, std::vector<uint8_t>(0x400, 0).data(), 0x400); }
    // A controller's value, which the next conversion pass reads (0x894e9fe0)
    void setCc(int cc, int value) { c.wr8(kCtl + 0x84 + (uint32_t)cc, (uint8_t)value); }
    void setPressure(int channel) { c.wr8(kCtl + 2, (uint8_t)channel); }
    void setPoly(int key, int value) { c.wr8(kCtl + 4 + (uint32_t)key, (uint8_t)value); }

    bool stop(FwNote& n) {
        if (!c.call(0x894f5a70, { n.TN, 3 }, kSp)) { std::fprintf(stderr, "stop: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894eb810, { n.IN, 3 }, kSp)) { std::fprintf(stderr, "stop input: %s\n", c.error().c_str()); return false; }
        n.live = false;
        n.off = true;
        return true;
    }
};
