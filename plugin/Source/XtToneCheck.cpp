// A whole firmware tone against XtSynth on the same settings: the wiring check.
//
// xt_tone <rom.2.bin> [cases] [seed] [inputs dir]
//   XT_TONE_FIRST=1: only each case's first difference; XT_TONE_DUMP=<case>|a: its settings
//   XT_TONE_STOP=1: end the run at its first difference ("tone: 1 different, at case ..."); mutate_tone.py
//   XT_TONE_HOST=1: in the host's clock's section, the count of six and the tempo read at each tempo
//
// The firmware runs in the SH-4 interpreter from the parameter IDs on, nothing
// of its path set by hand:
//   the parameter getter (0x8947d310) answers from a table made from
//     XtSynth::Controls, the XT's defaults for every other ID;
//   the patch → part table updaters (0x894dfe10);
//   the note's start (0x894dc130): the input tables (0x894eb4d0) and the tone
//     block through each converter, the tone from its constructor (0x894f5860);
//   the tone's note-on (0x894db830 → 0x894f5e60, then 0x894f4f70);
//   the free-running LFOs (0x894f1540) and the tone update (0x894f75f0) on
//     XtSynth's control clock, the render (0x894f6d40) every block;
//   the note-off (0x894dc270), a sounding tone taking a note (0x894dc1e0) and
//     truncation (0x894d5ba0), chosen as the part's note-on and note-off do;
//   a knob moved during a note: the updaters and the note's update (0x894dc0b0);
//   the Matrix's table (0x894e9330, among the updaters) and the system's assigns (0x894e9db0);
//   the bender, the pressures and the controllers written where the channel keeps them, and that update;
//   a key's release (0x894d58b0), Hold and Sostenuto (0x894d5fc0, 0x894d5fd0, 0x894d5970, 0x894d59d0)
//   and the mode messages (0x894d6040, then the key's release or truncation) on the note objects;
//   the tempo clock's tick (0x894f4da0) on XtSynth's clock, the Patch Tempo through its
//     object (0x89471ef0), Clock Source through it too (0x89471e30), an outside clock as its F8 routine
//     (0x894712f0) and timer routine (0x89471030), and the Time Trip pad through its controllers (0x894d8360).
//   Then the clock's rate alone, against the firmware's timer routine; the outside clock alone, on what
//   no transport gives; and the host's transport to the clocks, against where it says it is.
// Only the OS is hooked: its locks and clock, the voice allocator's lists, and
// the wave store (0x8949e5c0…): the waves lie in RAM as the loader leaves them,
// from the same encodes XtSynth plays (their vpdt and load-time bands).
//
// XtSynth runs on the same Controls and notes.  Compared, every block: both OSC
// buffers, the mix, the output of the COSM stage, the TVA and the output; at every
// update, the envelopes' and LFOs' outputs and stages, the pitch terms and ramps,
// and each voice's fields; at each case's end, FM's lines; all bit for bit in the
// XT's units, with no tolerance.  Past the wave's data a player
// reads whatever lies there: the waves lie among zeros, which is what the port reads;
// a band player's read further off is given zeros too (xtFwHookBandReads), and a note
// whose read leaves memory by another path is left off and counted.
//
// The random cases are followed by a sweep: each field XtSynth keeps in a list of what
// has changed moved alone during a note, in each stage of the envelopes; then by cases
// with a wave on OSC 2, its own, over OSC 1 off, analog or on another wave; then by
// cases with a second update inside one tick, after a Sustain moved in the sustain stage.
#ifdef XT_TONE_MAIN
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "Sh4Interp.h"
#include "XtFirmware.h"
#include "XtSoloEncoder.h"
#include "XtSynth.h"
#include "XtFiles.h"
#include "XtPatch.h"

using OscC = XtSynth::OscControls;
using Ctl = XtSynth::Controls;

// The peek at XtSynth's voices (a friend of the class).
struct XtToneProbe {
    static void bend(XtSynth& s, int b) { s.bender = b; }
    static void ccs(XtSynth& s, const int* v, const int* number, int n) { for (int i = 0; i < n; ++i) s.cc[number[i]] = v[i]; }
    static void sources(XtSynth& s, int number, int value) { s.cc[number] = value; }
    static void press(XtSynth& s, int channel, int key, int poly) { s.pressure = channel; if (key >= 0) s.polyPress[key] = poly; }
    static int run(int argc, char** argv);
    static XtSynth::OscVoice& osc(XtSynth::Voice& v, int n) { return n ? v.osc2 : static_cast<XtSynth::OscVoice&>(v); }
    static XtEnvelope& env(XtSynth::Voice& v, int e) {
        if (e <= 8) {
            XtSynth::OscVoice& o = osc(v, (e - 1) / 4);
            switch ((e - 1) % 4) { case 0: return o.penv; case 1: return o.tenv; case 2: return o.fenv; default: return o.oenv; }
        }
        if (e == 13) return v.env;
        XtSynth::Voice::CosmVoice& c = v.cs[(e - 9) / 2];
        return (e - 9) % 2 ? c.envB : c.envA;
    }
};

namespace {

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

    bool open(const char* path) {
        FILE* f = std::fopen(path, "rb");
        if (!f) return false;
        std::fseek(f, 0, SEEK_END);
        rom.resize((size_t)std::ftell(f));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(rom.data(), 1, rom.size(), f) != rom.size()) { std::fclose(f); return false; }
        std::fclose(f);
        if (!c.loadRom(path)) return false;
        c.mapRam(kRam, 0x03000000);
        // A VariPhrase note-on starts its formant and walk ramps (0x894f2440), off, from the
        // log of a formant or walk at or below 0: exp2 (0x894ec090) then reads far outside
        // its tables.  The hardware reads whatever is there, into a ramp that is off.
        c.mapRam(0x877d0000, 0x10000);
        c.mapRam(0x8b7d0000, 0x10000);
        // A key-unsynced LFO's search for the newest tone of its part and zone (0x894f5f70) walks back from the sounding list's
        // last node, and with none of its zone off the first into the list's head (0x89fa9078), whose tone pointer (+0x14) is 0:
        // it then reads +0x1c of address 0, on the XT the boot ROM in area 0.  Equal to the key or not, the search ends there with
        // no tone (0x894f5f8a), on the free-running object.  Zeros stand in for it.
        c.mapRam(0x00000000, 0x1000);
        for (uint32_t a : { 0x89301920u, 0x89301930u, 0x89301950u, 0x89301960u, 0x89301860u }) c.hook(a, [](Sh4& m) { m.ret(0); });
        c.hook(0x89301940, [this](Sh4& m) { m.ret((uint32_t)(int32_t)std::floor((double)frames * 1000.0 / 44100.0)); });
        auto getter = [this](Sh4& m) {
            const int id = (int16_t)m.r[5];
            const bool cosm = id == 0x3e7 || id == 0x3e8 || (id >= 0x8d0 && id <= 0x954);
            if (id >= 0x124 && id <= 0x133) { m.ret((uint32_t)upper[id - 0x124]); return; }
            const uint32_t zone = (m.r[4] & 0xffffff00u) == kZoneTag ? m.r[4] & 0xffu : 0;
            const auto& P = zone && zoned(id) && zone < zpar.size() && !zpar[zone].empty() ? zpar[zone] : par;
            auto it = P.find(cosm && cosmSection ? id + kCosm2 : id);
            m.ret((uint32_t)(it != P.end() ? it->second : defaultOf(id)));
        };
        c.hook(0x89509f52, [this](Sh4& m) { fr11[m.r[5]] = m.fb[(m.fpscr >> 21) & 1][11]; });
        c.hook(0x8947d310, getter);
        c.hook(0x8947d330, getter);
        c.hook(0x894798a0, [this](Sh4& m) { cosmSection = (int)m.r[7]; m.wr32(m.r[4] + 4, kZoneTag | (m.r[6] & 0xff)); m.ret(0); });   // which COSM's parameters are read next, and the zone's
        // the read objects of a zone's blocks (obj, part, zone): its tone block, envelopes, LFOs, COSMs, each from one of the part's two stores
        for (uint32_t a : { 0x89479720u, 0x89479760u, 0x894797c0u, 0x89479810u, 0x89479860u, 0x89479920u })
            c.hook(a, [](Sh4& m) { m.wr32(m.r[4] + 4, kZoneTag | (m.r[6] & 0xff)); m.ret(0); });
        // the COSMs' buffers (the pool's slot finder), which the allocator clears
        c.hook(0x894ec2b0, [this](Sh4& m) { for (uint32_t i = 0; i < 4; ++i) { m.wr32(m.r[5] + 4 * i, cosmBuf); cosmBuf += 0x10000; } m.ret(0); });
        c.wr32(0x898585dc, kOs);                       // the OS's lock owner
        if (!c.call(0x894e5010, {}, kSp)) { std::fprintf(stderr, "seed: %s\n", c.error().c_str()); return false; }   // the random pitch's power-on seed
        // the wave store, by 2999 + the Wave Number in r4
        c.hook(0x8949e5c0, [this](Sh4& m) { m.ret(waveOf(m.r[4]).loaded ? 1 : 0); });
        c.hook(0x8949e850, [this](Sh4& m) { m.ret(m.r[5] < 11 ? waveOf(m.r[4]).chunk[m.r[5]] : 0); });
        c.hook(0x8949ed70, [this](Sh4& m) { m.ret(m.r[5] < 7 ? waveOf(m.r[4]).band[m.r[5]] : 0); });
        c.hook(0x8949e790, [this](Sh4& m) { m.ret((uint32_t)waveOf(m.r[4]).frames); });
        c.hook(0x894d9230, [](Sh4& m) { m.ret(0); });  // the note lists
        c.hook(0x894d91a0, [](Sh4& m) { m.ret(0); });
        c.hook(0x894db720, [](Sh4& m) { m.ret(0); });  // a tone's end (+0x1760)
        c.hook(0x894db730, [](Sh4& m) { m.ret(0); });  // (+0x1764)
        // The channel's key state (0x894d6420), the LFOs' global seed (0x894f1820),
        c.wr32(kK, kN);
        c.wr32(kN, kCtl);                              // the channel's controllers: the bender first
        c.wr8(kCtl + 0x84 + 7, 127); c.wr8(kCtl + 0x84 + 11, 127); c.wr8(kCtl + 0x84 + 10, 64);   // CC 7, 11 and 10 at rest
        if (!c.call(0x894d6420, { kK }, kSp) || !c.call(0x894f1820, {}, kSp)) { std::fprintf(stderr, "power-on: %s\n", c.error().c_str()); return false; }
        // The free-running LFOs' pool (from the tones' manager, 0x894f5060), and
        // the system's calibration table, as at power-on
        if (!c.call(0x894f1400, { 0x89fa90b0u }, kSp)) { std::fprintf(stderr, "lfo pool: %s\n", c.error().c_str()); return false; }
        c.wr16(0x89d2a6d0 + 0x1602, (uint16_t)defaultOf(0x51f));   // the Truncate Time, as the system updater stores it
        // the TB Filter's constants (run at start-up)
        if (!c.call(0x894fc4c0, {}, kSp)) { std::fprintf(stderr, "tb constants: %s\n", c.error().c_str()); return false; }
        c.hook(0x894796d0, [](Sh4& m) { m.wr32(m.r[4] + 4, kZoneTag | (m.r[6] & 0xff)); m.ret(0); });
        if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, kSp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return false; }
        xtFwHookBandReads(c, bandReads);
        // the tempo clock's object and the parts' Time Trip pads, as at power-on
        if (!c.call(0x894714b0, {}, kSp) || !c.call(0x894d8740, {}, kSp)) { std::fprintf(stderr, "clock, pads: %s\n", c.error().c_str()); return false; }
        c.hook(0x893018b0, [](Sh4& m) { m.ret(0); });     // the signal to the clock's task
        for (uint32_t a : { 0x8946ebb0u, 0x8946ec00u }) c.hook(a, [](Sh4& m) { m.ret(0); });   // its object on the MIDI input's list, or off it
        return true;
    }

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

// The vpdt of a SOLO wave as the XT lays out a one-segment one (as xtFwSoloPlay does).
std::vector<uint8_t> soloVpdt(const std::vector<XtRecord>& recs, long frames, int channels) {
    const uint32_t endPos = (uint32_t)frames * 16;
    const int st = channels == 2 ? 16 : 10;
    const uint32_t ev0 = 16, ev1 = (uint32_t)(st + 8) * 2, seg0 = (uint32_t)(2 * st + 8) * 2, seg1 = seg0 + 12;
    const uint32_t cnt = seg1 + 12, rec0 = cnt + 4;
    std::vector<uint16_t> stream;
    for (const XtRecord& r : recs) {
        int len = r.len16;
        while (len >= 0xffff) { stream.push_back(0xffff); stream.push_back((uint16_t)r.info); len -= 0xffff; }
        stream.push_back((uint16_t)len); stream.push_back((uint16_t)r.info);
    }
    const uint32_t term = rec0 + (uint32_t)stream.size() * 2;
    stream.push_back(256); stream.push_back(1);
    std::vector<uint8_t> v(term + 4, 0);
    auto w32 = [&](uint32_t o, uint32_t x) { std::memcpy(&v[o], &x, 4); };
    auto w16 = [&](uint32_t o, uint16_t x) { std::memcpy(&v[o], &x, 2); };
    w32(8, 2u << 16);
    w32(ev0, 0x0d); w32(ev0 + 4, 0); w32(ev0 + 8, seg0); w32(ev0 + 12, rec0); w16(ev0 + 16, stream[0]);
    w32(ev1, 0x0e); w32(ev1 + 4, endPos); w32(ev1 + 8, seg1); w32(ev1 + 12, term); w16(ev1 + 16, 256);
    w32(seg0, rec0); w32(seg0 + 4, 1); w32(seg0 + 8, 0);
    w32(seg1, term); w32(seg1 + 4, 1); w32(seg1 + 8, endPos);
    w32(cnt, (uint32_t)(stream.size() / 2));
    std::memcpy(&v[rec0], stream.data(), stream.size() * 2);
    return v;
}

// One of the test waves, encoded for XtSynth and laid out for the firmware.
struct TestWave {
    std::shared_ptr<XtEncoded> enc;
    FwWave fw;
    int type = 0, channels = 1;
    std::vector<XtRecord> recs;         // SOLO
};

// A band wave's `vpno` (chunk 3), as XtEncoded has it: +0 8, +4 the count, +8 0 and the count less one, then the vpdt's event numbers.
// The chunk is 0x40 bytes: room for 26.
void writeVpno(Firmware& f, const FwWave& w, const XtEncoded& e) {
    std::vector<uint8_t> v(std::max<size_t>(0x40, 12 + 2 * e.vpno.size()), 0);   // the chunk has a page to itself
    const auto le = [&](size_t at, uint32_t x, int n) { for (int k = 0; k < n; ++k) v[at + (size_t)k] = (uint8_t)(x >> (8 * k)); };
    le(0, 8, 4);
    le(4, (uint32_t)e.vpnoCount, 4);
    le(10, (uint32_t)(e.vpnoCount - 1), 2);
    for (size_t k = 0; k < e.vpno.size(); ++k) le(12 + 2 * k, e.vpno[k], 2);
    f.c.write(w.chunk[3], v.data(), v.size());
}

bool makeWave(Firmware& f, uint32_t& next, const std::vector<int16_t>& raw, int channels, int type, TestWave& out);
bool loadWave(Firmware& f, uint32_t& next, const std::string& path, int channels, int type, long maxFrames, TestWave& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::vector<int16_t> raw;
    int16_t x;
    while (raw.size() < (size_t)(maxFrames * channels) && std::fread(&x, 2, 1, fp) == 1) raw.push_back(x);
    std::fclose(fp);
    return makeWave(f, next, raw, channels, type, out);
}

// A wave of separate hits, decaying noise and tone, which the encoder gives an attack event each: hits + 1 events
std::vector<int16_t> hitsWave(int hits, int channels, long frames, uint32_t seed) {
    std::vector<int16_t> raw((size_t)(frames * channels), 0);
    for (int h = 0; h < hits; ++h) {
        const long at = h * frames / hits;
        for (long i = 0; i < frames / hits - 200; ++i)
            for (int c = 0; c < channels; ++c) {
                seed = seed * 1664525u + 1013904223u;
                const double noise = (double)(seed >> 9) / 8388608.0 - 1.0, tone = std::sin(2.0 * 3.141592653589793 * (220.0 + 110.0 * h + 3.0 * c) * (double)i / 44100.0);
                raw[(size_t)((at + i) * channels + c)] = (int16_t)std::lround(26000.0 * std::exp(-(double)i / 1500.0) * (0.5 * noise + 0.5 * tone));
            }
    }
    return raw;
}

bool makeWave(Firmware& f, uint32_t& next, const std::vector<int16_t>& raw, int channels, int type, TestWave& out) {
    const long frames = (long)raw.size() / channels;
    std::vector<std::vector<float>> chans((size_t)channels);
    for (long i = 0; i < frames; ++i)
        for (int k = 0; k < channels; ++k) chans[(size_t)k].push_back((float)raw[(size_t)(i * channels + k)] / 32768.0f);
    std::string err;
    out.enc = xtSynthEncode(chans, 44100.0, type, 64, &err);
    out.type = type;
    out.channels = channels;
    if (!out.enc) { std::fprintf(stderr, "encode: %s\n", err.c_str()); return false; }
    FwWave& w = out.fw;
    w.frames = out.enc->frames;
    auto put = [&](const void* data, size_t bytes) {
        next += kXtFwWavePad;                              // zeros around each, as a band's reads outside it get from the port
        const uint32_t a = next;
        f.c.write(a, data, bytes);
        next = (next + (uint32_t)bytes + 0x1fff) & ~0xfffu;
        return a;
    };
    std::vector<uint8_t> zero(0x400, 0);
    w.chunk[2] = put(zero.data(), 0x40);                  // the header
    w.chunk[3] = put(zero.data(), 0x40);                  // no steps
    w.chunk[4] = put(zero.data(), 0x40);                  // the loop record
    w.chunk[10] = put(zero.data(), 0x400);                // the cached loop events
    f.c.wr8(w.chunk[2] + 1, (uint8_t)type);
    f.c.wr8(w.chunk[2] + 2, channels == 2 ? 1 : 0);
    f.c.wr16(w.chunk[2] + 8, 44000);
    f.c.wr8(w.chunk[4] + 0x1a, 1);                        // the Loop switch decides
    if (type == 0)                                         // LITE: the samples alone, no vpdt (a project's LITE waves have none)
        w.band[0] = put(raw.data(), raw.size() * 2);
    else if (type == 1) {
        out.recs = xtSoloEncode(raw.data(), (int)frames, channels);
        const std::vector<uint8_t> v = soloVpdt(out.recs, frames, channels);
        w.chunk[5] = put(v.data(), v.size());
        w.band[0] = put(raw.data(), raw.size() * 2);
    } else {
        w.chunk[5] = put(out.enc->vpdt.data(), out.enc->vpdt.size());
        writeVpno(f, w, *out.enc);
        static const int slot[6] = { 0, 2, 3, 4, 5, 6 };
        for (int k = 0; k < 6; ++k) {
            const size_t bytes = out.enc->bands.buf[k].size() * 2;
            w.band[slot[k]] = put(out.enc->bands.buf[k].data(), bytes);
            f.bandReads.add(w.band[slot[k]], bytes);
        }
    }
    w.loaded = true;
    return true;
}

// XtSynth's own SOLO records against the firmware's encoder.  The cases give both sides the port's
// records, loadWave's, so nothing in them sees which mode XtSynth encodes in, on a wave where the modes differ.
void checkSoloRecords(const std::string& dir, const char* file, int channels) {
    std::vector<int16_t> raw;
    if (FILE* fp = std::fopen((dir + file).c_str(), "rb")) {
        int16_t x;
        while (std::fread(&x, 2, 1, fp) == 1) raw.push_back(x);
        std::fclose(fp);
    }
    const long frames = (long)raw.size() / channels;
    std::vector<std::vector<float>> chans((size_t)channels);
    for (long i = 0; i < frames; ++i)
        for (int k = 0; k < channels; ++k) chans[(size_t)k].push_back((float)raw[(size_t)(i * channels + k)] / 32768.0f);
    std::string err;
    const std::shared_ptr<XtEncoded> e = xtSynthEncode(chans, 44100.0, 1, 64, &err);
    std::vector<XtRecord> fw;
    if (!e || !xtFwSoloEncode(raw.data(), (int)frames, fw, channels)) { std::printf("tone, the SOLO records of %s: cannot encode\n", file); return; }
    const XtSoloPlan& P = e->solo;
    const size_t both = std::min(P.segStart.size(), fw.size());
    size_t diff = std::max(P.segStart.size(), fw.size()) - both;
    for (size_t i = 0; i < both; ++i)
        if (P.segStart[i] != fw[i].start16 / 16.0 || P.info[i] != fw[i].info) ++diff;
    std::printf("tone, the SOLO records of %s: %zu different of %zu\n", file, diff, fw.size());
}

// The cached loop events a wave's player reads, for the loop XtSynth computes: the
// firmware's own builder of them is not found.  As xtFwSoloPlay and xtFwBandPlay lay them.
void writeLoopEvents(Firmware& f, const TestWave& t, const OscC& c, const FwWave* copy = nullptr) {
    const FwWave& W = copy ? *copy : t.fw;          // a zone's own copy of its header and loop chunks, or the wave's
    // the region from the fractions, at least 16 long; One Shot's modes are 0
    XtLoop l;
    const int last = (int)t.fw.frames - 1;
    int a = (int)std::lround((double)c.loopStart * last), b = (int)std::lround((double)std::min(c.loopEnd, 1.0f) * last);
    if (b - a < 16) { b = std::min(a + 16, last); a = b - 16; }
    l.on = c.loop;
    l.fwdMode = l.bwdMode = c.loopMode;
    l.fwdStart = l.bwdStart = a;
    l.fwdEnd = l.bwdEnd = b;
    l.fwdRate = t.enc->loopRate[0]; l.bwdRate = t.enc->loopRate[1];
    if (t.enc->bwdMode >= 0) { l.bwdMode = t.enc->bwdMode; l.bwdStart = t.enc->bwdStart; l.bwdEnd = t.enc->bwdEnd; }
    const uint32_t W2 = W.chunk[10];
    f.c.write(W2, std::vector<uint8_t>(0x400, 0).data(), 0x400);
    f.c.wr32(W2 + 0x2d0, 0x3f800000);                     // a rate factor no code seen writes: what the note-on takes for the backward loop, 1 as the port has it
    // The loop record, which only Vari off reads (0x894f3460)
    const uint32_t R4 = W.chunk[4];
    f.c.wr32(R4 + 4, (uint32_t)l.fwdStart); f.c.wr32(R4 + 8, (uint32_t)l.fwdEnd); f.c.wr32(R4 + 0xc, (uint32_t)l.bwdEnd); f.c.wr32(R4 + 0x10, (uint32_t)l.bwdStart);
    f.c.wr8(R4 + 0x18, (uint8_t)l.fwdMode); f.c.wr8(R4 + 0x19, (uint8_t)l.bwdMode);
    if (!l.on) { f.c.wr32(W2 + 0xb8, 0x7fffffff); f.c.wr32(W2 + 0x16c, 0x7fffffff); return; }
    if (t.type == 0) {                                     // a LITE wave's player reads the record (0x8950b712); the beat, the events' positions
        const int pts[4] = { l.fwdStart, l.fwdEnd, l.bwdEnd, l.bwdStart };
        for (uint32_t k = 0; k < 4; ++k) f.c.wr32(W2 + 0xb4 * k + 4, (uint32_t)(pts[k] * 16));
        return;
    }
    if (t.type == 1) {
        const int st = t.channels == 2 ? 16 : 10;
        const uint32_t seg0 = (uint32_t)(2 * st + 8) * 2, rec0 = seg0 + 12 + 12 + 4, endPos = (uint32_t)t.fw.frames * 16;
        uint32_t term = rec0;
        for (const XtRecord& r : t.recs) { int len = r.len16; while (len >= 0xffff) { term += 4; len -= 0xffff; } term += 4; }
        const int pts[4] = { l.fwdStart, l.fwdEnd, l.bwdEnd, l.bwdStart };
        const int modes[4] = { 0, l.fwdMode, 0, l.bwdMode };
        for (uint32_t k = 0; k < 4; ++k) {
            const int pos = pts[k] * 16;
            uint32_t off = term, walk = rec0;
            int start = (int)endPos, first = 256;
            size_t s0 = 0;
            for (const XtRecord& r : t.recs) {
                if ((int)s0 + r.len16 > pos) { off = walk; start = (int)s0; first = std::min(r.len16, 0xffff); break; }
                int len = r.len16;
                while (len >= 0xffff) { walk += 4; len -= 0xffff; }
                walk += 4;
                s0 += (size_t)r.len16;
            }
            const uint32_t R = W2 + 0xb4 * k;
            f.c.wr16(R, 0); f.c.wr16(R + 2, (uint16_t)modes[k]); f.c.wr32(R + 4, (uint32_t)pos);
            f.c.wr32(R + 8, seg0); f.c.wr32(R + 12, off); f.c.wr16(R + 16, (uint16_t)(start + first - pos));
        }
    } else {
        uint8_t ev[4][kXtMbLoopEventBytes];
        if (xtMbLoopEvents(t.enc->wave, l, ev))
            for (uint32_t k = 0; k < 4; ++k) f.c.write(W2 + (uint32_t)kXtMbLoopEventBytes * k, ev[k], kXtMbLoopEventBytes);
    }
}

// An analog voice's fields (as xt_analog compares them), port side.
struct VField { const char* name; uint32_t off; float v; };
std::vector<VField> anaFields(const XtAnalogOsc& o) {
    std::vector<VField> f = { { "ratio", 0x144, o.ratio() }, { "gain target", 0x154, o.gainTarget() }, { "gain", 0x158, o.gain() }, { "fat target", 0x1c0, o.fatTarget() }, { "fat", 0x1c4, o.fat() },
        { "pw target", 0x1c8, o.pwTarget() }, { "pw", 0x1cc, o.pw() }, { "decay", 0x1d0, o.decay() }, { "phase", 0x1d4, o.phase() }, { "impact", 0x1d8, o.impact() }, { "edge", 0x1e0, o.edge() },
        { "hq level", 0x1e4, o.hqLevel() }, { "sub pw target", 0x1ec, o.subPwTarget() }, { "sub pw", 0x1f0, o.subPw() }, { "la step", 0x1f4, o.laStep() }, { "sub step", 0x1f8, o.subStep() },
        { "sub phase", 0x1fc, o.subPhase() }, { "sub level target", 0x200, o.subLevelTarget() }, { "sub level", 0x204, o.subLevel() }, { "ramp", 0x208, o.rampEnd() }, { "ramp 2", 0x210, o.rampEnd2() },
        { "la phase 2", 0x218, o.laPhase2() }, { "la phase 3", 0x21c, o.laPhase3() }, { "la steep target", 0x224, o.laSteepTarget() }, { "la steep", 0x228, o.laSteep() },
        { "la steep 2 target", 0x22c, o.laSteep2Target() }, { "la steep 2", 0x230, o.laSteep2() }, { "steep target", 0x234, o.steepTarget() }, { "steep", 0x238, o.steep() },
        { "steep 2 target", 0x23c, o.steep2Target() }, { "steep 2", 0x240, o.steep2() }, { "detune target", 0x24c, o.detuneTarget() }, { "detune", 0x250, o.detune() },
        { "mix target", 0x254, o.mixTarget() }, { "mix", 0x258, o.mix() }, { "fb state", 0x274, o.fbState() } };
    static const char* side[6] = { "side 0", "side 1", "side 2", "side 3", "side 4", "side 5" };
    for (int i = 0; i < 6; ++i) f.push_back({ side[i], 0x25c + 4 * (uint32_t)i, o.sidePhase(i) });
    return f;
}

// ── The patch, by ID, from XtSynth's controls ───────────────────────────────
void envParams(std::unordered_map<int, int>& p, int e, int a, int d, int s, int r, int curve, int sens, int aS, int dS, int rS, int kf) {
    const int b = 0x315 + 13 * e;
    p[b] = a; p[b + 2] = d; p[b + 3] = s; p[b + 5] = r; p[b + 6] = curve; p[b + 7] = sens; p[b + 8] = aS; p[b + 9] = dS; p[b + 10] = rS; p[b + 11] = kf;
}

void oscParams(std::unordered_map<int, int>& p, int n, const OscC& o) {
    const int b = 0x2a4 + 0x34 * n;
    p[b] = o.on; p[b + 1] = o.oscType; p[b + 2] = o.waveGain; p[b + 3] = o.oscPitch; p[b + 4] = o.pitchKF / 10;
    p[b + 5] = o.lfoPitchDepth; p[b + 6] = o.pDepth; p[b + 7] = o.oscCoarse; p[b + 8] = o.oscFine; p[b + 9] = o.randomPitch;
    p[b + 0xa] = o.oscLevel; p[b + 0xb] = o.oscLevelKF / 10; p[b + 0xc] = o.oscLevelLfoDepth;
    p[b + 0xd] = o.ana.wave; p[b + 0xe] = o.ana.impact; p[b + 0xf] = o.ana.pw; p[b + 0x10] = o.ana.pwKf; p[b + 0x11] = o.ana.pwLfo; p[b + 0x12] = o.ana.pwEnv;
    p[b + 0x13] = o.ana.fat; p[b + 0x14] = o.ana.fatKf; p[b + 0x15] = o.ana.fatLfo; p[b + 0x16] = o.ana.fatEnv;
    p[b + 0x1a] = 1 + n;                              // the Wave Number
    p[b + 0x22] = o.startOffset;
    p[b + 0x2e] = o.playbackMode;
    p[b + 0x23] = o.oscTime; p[b + 0x24] = o.oscTimeKF / 10; p[b + 0x25] = o.lfoTimeDepth; p[b + 0x26] = o.tDepth; p[b + 0x27] = o.oscTimeOffset;
    p[b + 0x28] = o.oscFormant; p[b + 0x29] = o.oscFormantKF / 10; p[b + 0x2a] = o.lfoFormantDepth; p[b + 0x2b] = o.fDepth;
    p[b + 0x30] = o.vari;
    p[b + 0x2c] = o.tempoSync; p[b + 0x2d] = o.robot; p[b + 0x2f] = o.loop; p[b + 0x31] = o.energy; p[b + 0x32] = o.timeTrip; p[b + 0x33] = o.beatKeep;
    p[0x30d + 3 * n] = o.ana.detune; p[0x30e + 3 * n] = o.ana.subOctave; p[0x30f + 3 * n] = o.ana.subLevel;
    const int e = 4 * n;
    envParams(p, e, o.pAttack, o.pDecay, o.pSustain, o.pRelease, o.pVel.curve, o.pVel.sens, o.pVel.aSens, o.pVel.dSens, o.pVel.rSens, o.pVel.timeKF / 10);
    envParams(p, e + 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease, o.tVel.curve, o.tVel.sens, o.tVel.aSens, o.tVel.dSens, o.tVel.rSens, o.tVel.timeKF / 10);
    envParams(p, e + 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease, o.fVel.curve, o.fVel.sens, o.fVel.aSens, o.fVel.dSens, o.fVel.rSens, o.fVel.timeKF / 10);
    envParams(p, e + 3, o.oAttack, o.oDecay, o.oSustain, o.oRelease, o.oscVelCurve, o.oscVelSens, o.oASens, o.oDSens, o.oRSens, o.oTimeKF / 10);
    const int l = 0x3be + 8 * n;
    p[l] = o.lfoWave; p[l + 2] = o.lfoRate; p[l + 3] = o.lfoOffset; p[l + 4] = o.lfoDelay; p[l + 5] = o.lfoFadeMode; p[l + 6] = o.lfoFade; p[l + 7] = o.lfoKeySync;
}

void cosmParams(std::unordered_map<int, int>& p, int k, const Ctl::Cosm& c) {
    const int add = k ? kCosm2 : 0;
    static const int fwType[16] = { 10, 11, 9, 16, 8, 6, 7, 5, 14, 15, 12, 13, 2, 1, 3, 4 };   // COSM Type by XtSynth's number
    p[0x3e7 + add] = c.on; p[0x3e8 + add] = fwType[c.type];
    auto set = [&](int id, std::initializer_list<int> v) { for (int x : v) p[add + id++] = x; };
    set(0x920, { c.filter, c.reso, c.resoLfo, c.resoEnv, c.cutoff, c.kf / 10, c.cutoffLfo, c.cutoffEnv });
    set(0x928, { c.filter, c.reso, c.resoLfo, c.resoOwn, c.resoEnv, c.cutoff, c.kf / 10, c.cutoffLfo, c.cutoffOwn, c.cutoffEnv });
    set(0x915, { c.dualType, c.freq1, c.kf1 / 10, c.q1, c.lfo1, c.env1, c.freq2, c.kf2 / 10, c.q2, c.lfo2, c.env2 });
    set(0x94a, { c.tbCutoff, c.tbKf / 10, c.tbLfo, c.tbEnv, c.tbQ, c.tbQFollow, c.tbHpf, c.tbHpfKf / 10, c.tbHpfLfo, c.tbHpfEnv, c.tbGain });
    set(0x90e, { c.combOctave, c.combDetune, c.combDetuneLfo, c.combDetuneEnv, c.combTone, c.combToneLfo, c.combToneEnv });
    const Ctl::Cosm::SideBand& a = c.sb[0];
    set(0x8f3, { a.hpf, a.lpf, a.lpfKf / 10, a.subDetune, a.octave, a.detune, a.detuneLfo, a.detuneOwn, a.detuneEnv, a.width, a.widthKf / 10, a.widthLfo, a.widthOwn, a.widthEnv });
    const Ctl::Cosm::SideBand& b = c.sb[1];
    set(0x901, { b.q, b.lpf, b.lpfKf / 10, b.octave, b.detune, b.detuneLfo, b.detuneOwn, b.detuneEnv, b.width, b.widthKf / 10, b.widthLfo, b.widthOwn, b.widthEnv });
    set(0x8eb, { c.resType, c.resSize, c.resSizeKf / 10, c.resSizeLfo, c.resSizeEnv, c.resBalance, c.resBalanceLfo, c.resBalanceEnv });
    set(0x93a, { c.fsDepth, c.fsDepthKf / 10, c.fsDepthLfo, c.fsDepthEnv, c.fsBalance, c.fsBalanceLfo, c.fsBalanceEnv });
    set(0x941, { c.lofiBit, c.lofiRate, c.lofiFilter, c.lofiFilterKf / 10, c.lofiFilterLfo, c.lofiFilterEnv, c.lofiBalance, c.lofiBalanceLfo, c.lofiBalanceEnv });
    set(0x932, { c.compAttack, c.compSustain, c.compLevel });
    set(0x935, { c.limThreshold, c.limRatio, c.limAttack, c.limRelease, c.limLevel });
    set(0x8d9, { c.wsType, c.wsLevel, c.wsDrive, c.wsDriveEnv, c.wsDriveLfo, c.wsTone, c.wsToneKf / 10, c.wsToneLfo, c.wsToneEnv });   // Drive's Env depth before its LFO depth
    set(0x8d0, { c.odType, c.odLevel, c.odDrive, c.odDriveLfo, c.odDriveEnv, c.odTone, c.odToneKf / 10, c.odToneLfo, c.odToneEnv });
    set(0x8e2, { c.ampType, c.ampBass, c.ampMiddle, c.ampTreble, c.ampLevel, c.ampGain, c.ampGainLfo, c.ampGainEnv });
    set(0x8ea, { c.spkType });
    const Ctl::CosmEnv* e[2] = { &c.envA, &c.envB };
    for (int i = 0; i < 2; ++i)
        envParams(p, 8 + 2 * k + i, e[i]->attack, e[i]->decay, e[i]->sustain, e[i]->release, e[i]->velCurve, e[i]->velSens, e[i]->aSens, e[i]->dSens, e[i]->rSens, e[i]->timeKF / 10);
    const int l = 0x3be + 8 * (2 + k);
    p[l] = c.lfoWave; p[l + 2] = c.lfoRate; p[l + 3] = c.lfoOffset; p[l + 4] = c.lfoDelay; p[l + 5] = c.lfoFadeMode; p[l + 6] = c.lfoFade; p[l + 7] = c.lfoKeySync;
}

std::unordered_map<int, int> patchOf(const Ctl& c) {
    std::unordered_map<int, int> p;
    oscParams(p, 0, c); oscParams(p, 1, c.osc2);
    cosmParams(p, 0, c.cosm[0]); cosmParams(p, 1, c.cosm[1]);
    p[0x11d] = c.patchTempo;
    p[0x41] = c.clockSource;
    p[0x110] = c.patchCoarse; p[0x111] = c.patchFine; p[0x112] = c.octave; p[0x113] = !c.mono; p[0x114] = c.legato;   // Mono/Poly: 0 MONO
    p[0x115] = c.portamento; p[0x116] = c.portaMode; p[0x117] = c.portaType; p[0x118] = c.portaTime; p[0x119] = c.portaVeloSens;
    p[0x11e] = c.bendUp; p[0x11f] = c.bendDown;
    p[0x134] = c.scaleTune;
    for (int i = 0; i < 12; ++i) p[0x136 + 2 * i] = c.scale[i];
    p[0x293] = c.structure; p[0x294] = c.mod.on; p[0x295] = c.mod.type; p[0x296] = c.mod.level; p[0x297] = c.mod.attack; p[0x298] = c.mod.release;
    p[0x299] = c.tvaOn;
    p[0x29a] = c.tvaLevel; p[0x29b] = c.tvaLevelKF / 10; p[0x29c] = c.tvaLevelLfo; p[0x29d] = c.tvaPan; p[0x29e] = c.tvaPanKF / 10; p[0x29f] = c.tvaPanLfo; p[0x2a0] = c.mfxSend;
    p[0x2a1] = c.chorusSend; p[0x2a2] = c.reverbSend; p[0x2a3] = c.outputAssign; p[0x526] = c.outGainTg; p[0x40] = c.parallel;
    for (int i = 0; i < 8; ++i) {
        const int b = 0x14d + 5 * i;
        p[b] = c.matrix[i].source; p[b + 1] = c.matrix[i].dest[0]; p[b + 2] = c.matrix[i].sens[0]; p[b + 3] = c.matrix[i].dest[1]; p[b + 4] = c.matrix[i].sens[1];
    }
    static const int kAssignId[12] = { 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x93, 0x94, 0x91, 0x92 };
    for (int i = 0; i < 12; ++i) p[kAssignId[i]] = c.assign[i];
    p[0x3de] = c.tvaLfoWave; p[0x3e0] = c.tvaLfoRate; p[0x3e1] = c.tvaLfoOffset; p[0x3e2] = c.tvaLfoDelay; p[0x3e3] = c.tvaLfoFadeMode; p[0x3e4] = c.tvaLfoFade; p[0x3e5] = c.tvaLfoKeySync;
    envParams(p, 12, c.attack, c.decay, c.sustain, c.release, c.ampVelCurve, c.ampVelSens, c.aSens, c.dSens, c.rSens, c.timeKF / 10);
    return p;
}

// ── A real patch ────────────────────────────────────────────────────────────
// xtPatchControls against patchOf: every ID the firmware's converters read from a patch, what it would have read from the store (xt_files holds xtPatchValue to the
// firmware's own read) against what it reads in the whole-tone check for those controls.  The system's IDs and the Wave Number aren't a patch's to give here.
struct PatchTally { long values = 0, different = 0; std::map<int, long> byId; };
void patchRoundTrip(const std::vector<uint8_t>& store, int zone, const Ctl& c, PatchTally& t) {
    const std::unordered_map<int, int> p = patchOf(c);
    const auto own = [&](int id) { return id == 0x40 || id == 0x41 || id == 0x526 || (id >= 0x89 && id <= 0x94) || id == 0x2be || id == 0x2f2; };
    for (const auto& kv : p) {
        const int id = kv.first & 0xffff, k = kv.first >> 16;
        if (own(id)) continue;
        // OSC Type 2, the external input: here an OSC off and on analog.  The firmware's tone gets the same, so only this can see it
        const int typeId = id == 0x2a4 || id == 0x2a5 ? 0x2a5 : id == 0x2d8 || id == 0x2d9 ? 0x2d9 : 0;
        if (typeId && xtPatchValue(store, typeId, zone) == 2) {
            ++t.values;
            if (kv.second != 0) { ++t.different; ++t.byId[kv.first]; }
            continue;
        }
        int fw;
        if (id >= 0x55d) {
            // A type's parameter: a patch holds only its own type's, and a COSM on Through none
            const int type = xtPatchValue(store, 0x3e8, zone, k);
            int first = 0, last = -1;
            xtEffectTypeIds(3, type, first, last);
            if (type == 0 || id < first || id > last) continue;
            fw = xtPatchTyped(store, kXtCosmSlots, id, zone, k);
        } else if (id == 0x3e7 || id == 0x3e8) {
            if (xtPatchValue(store, 0x3e8, zone, k) == 0) continue;   // Through: off here
            fw = xtPatchValue(store, id, zone, k);
        } else fw = xtPatchValue(store, id, zone);
        ++t.values;
        if (fw != kv.second) { ++t.different; ++t.byId[kv.first]; }
    }
}

// ── Random settings ─────────────────────────────────────────────────────────
struct Rng {
    uint32_t s;
    uint32_t next() { s = s * 1103515245u + 12345u; return (s >> 8) & 0xffff; }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
    bool one(int n) { return next() % (uint32_t)n == 0; }
};

// An envelope's time: in one case of eight a note value; an LFO's rate, in one of four
int drawTime(Rng& r) { return r.one(8) ? r.range(128, 144) : r.range(0, 90); }
int drawRate(Rng& r) { return r.one(4) ? r.range(128, 144) : r.range(0, 127); }

void drawOsc(Rng& r, OscC& o) {
    o.on = true;
    o.tempoSync = r.one(3); o.timeTrip = !r.one(4); o.beatKeep = !r.one(4);
    o.origTempo = r.one(3) ? 1200000 : r.range(40, 240) * 10000 + r.range(0, 9999);
    o.oscType = 0;
    o.oscTime = r.one(2) ? 0 : r.range(-63, 63); o.oscTimeOffset = r.range(0, 2); o.oscTimeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    o.oscFormant = r.one(2) ? 0 : r.range(-63, 63); o.oscFormantKF = r.one(2) ? 0 : 10 * r.range(-20, 20); o.energy = r.one(2) ? 0 : r.range(0, 127);
    o.tDepth = r.one(2) ? 0 : r.range(-63, 63); o.fDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.lfoTimeDepth = r.one(2) ? 0 : r.range(-63, 63); o.lfoFormantDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.ana.wave = r.range(0, 13);
    o.ana.impact = r.range(0, 40);
    o.ana.pw = r.range(-63, 63); o.ana.pwLfo = r.range(-63, 63); o.ana.pwEnv = r.range(-63, 63);
    o.ana.fat = r.range(0, 127); o.ana.fatLfo = r.range(-63, 63); o.ana.fatEnv = r.range(-63, 63);
    if (!r.one(3)) { o.ana.pwKf = r.range(-20, 20); o.ana.fatKf = r.range(-20, 20); }
    if (r.one(3)) o.ana.detune = r.range(-63, 63);
    if (r.one(3)) { o.ana.subOctave = r.range(0, 3); o.ana.subLevel = r.range(0, 127); }
    o.oscCoarse = r.one(2) ? 0 : r.range(-24, 24);
    o.oscFine = r.one(2) ? 0 : r.range(-50, 50);
    o.oscPitch = r.one(2) ? 0 : r.range(-63, 63);
    o.pitchKF = r.one(2) ? 100 : 10 * r.range(-20, 20);
    o.randomPitch = r.one(3) ? r.range(0, 30) : 0;
    o.pAttack = drawTime(r); o.pDecay = drawTime(r); o.pSustain = r.range(0, 127); o.pRelease = drawTime(r); o.pDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.tAttack = drawTime(r); o.tDecay = drawTime(r); o.tSustain = r.range(0, 127); o.tRelease = drawTime(r);
    o.fAttack = drawTime(r); o.fDecay = drawTime(r); o.fSustain = r.range(0, 127); o.fRelease = drawTime(r);
    for (OscC::EnvVelocity* v : { &o.pVel, &o.tVel, &o.fVel })
        if (r.one(3)) { v->curve = r.range(0, 7); v->sens = r.range(-63, 63); v->aSens = r.one(2) ? 0 : r.range(-63, 63); v->dSens = r.one(2) ? 0 : r.range(-63, 63); v->rSens = r.one(2) ? 0 : r.range(-63, 63); v->timeKF = r.one(2) ? 0 : 10 * r.range(-20, 20); }
    o.lfoWave = r.range(0, 7); o.lfoRate = drawRate(r); o.lfoOffset = r.range(0, 4); o.lfoDelay = r.one(2) ? 0 : r.range(0, 60);
    o.lfoFadeMode = r.range(0, 3); o.lfoFade = r.one(2) ? 0 : r.range(0, 60); o.lfoKeySync = !r.one(4);
    o.lfoPitchDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.oscLevel = r.range(0, 127); o.oscLevelKF = 10 * r.range(-20, 20); o.oscLevelLfoDepth = r.one(2) ? 0 : r.range(-63, 63); o.waveGain = r.range(-12, 12);
    o.oAttack = drawTime(r); o.oDecay = drawTime(r); o.oSustain = r.range(0, 127); o.oRelease = drawTime(r);
    o.oASens = r.one(2) ? 0 : r.range(-63, 63); o.oDSens = r.one(2) ? 0 : r.range(-63, 63); o.oRSens = r.one(2) ? 0 : r.range(-63, 63);
    o.oTimeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    o.oscVelSens = r.range(-63, 63); o.oscVelCurve = r.range(0, 7);
}

void drawCosm(Rng& r, Ctl::Cosm& c) {
    auto d = [&] { return r.one(2) ? 0 : r.range(-63, 63); };     // a depth
    auto u = [&] { return r.range(0, 127); };
    auto kf = [&] { return r.one(2) ? 0 : 10 * r.range(-20, 20); };
    c.type = r.range(0, 15);
    c.filter = r.range(0, 11); c.cutoff = u(); c.reso = u(); c.kf = kf(); c.cutoffEnv = d(); c.cutoffLfo = d(); c.resoEnv = d(); c.resoLfo = d(); c.resoOwn = d(); c.cutoffOwn = d();
    c.dualType = r.range(0, 2); c.freq1 = u(); c.kf1 = kf(); c.q1 = u(); c.lfo1 = d(); c.env1 = d(); c.freq2 = u(); c.kf2 = kf(); c.q2 = u(); c.lfo2 = d(); c.env2 = d();
    c.tbCutoff = u(); c.tbKf = kf(); c.tbLfo = d(); c.tbEnv = d(); c.tbQ = u(); c.tbQFollow = d(); c.tbHpf = u(); c.tbHpfKf = kf(); c.tbHpfLfo = d(); c.tbHpfEnv = d(); c.tbGain = u();
    c.combOctave = r.range(0, 4); c.combDetune = r.range(-100, 100); c.combDetuneLfo = d(); c.combDetuneEnv = d(); c.combTone = u(); c.combToneLfo = d(); c.combToneEnv = d();
    for (Ctl::Cosm::SideBand& s : c.sb) {
        s.hpf = u(); s.q = u(); s.lpf = u(); s.lpfKf = kf(); s.subDetune = r.range(-100, 100); s.octave = r.range(0, 2); s.detune = r.range(-100, 100); s.detuneLfo = d(); s.detuneOwn = d(); s.detuneEnv = d();
        s.width = u(); s.widthKf = kf(); s.widthLfo = d(); s.widthOwn = d(); s.widthEnv = d();
    }
    c.resType = r.range(0, 2); c.resSize = u(); c.resSizeKf = kf(); c.resSizeLfo = d(); c.resSizeEnv = d(); c.resBalance = r.range(0, 100); c.resBalanceLfo = d(); c.resBalanceEnv = d();
    c.fsDepth = u(); c.fsDepthKf = kf(); c.fsDepthLfo = d(); c.fsDepthEnv = d(); c.fsBalance = r.range(0, 100); c.fsBalanceLfo = d(); c.fsBalanceEnv = d();
    c.lofiBit = r.range(0, 15); c.lofiRate = r.range(0, 5); c.lofiFilter = u(); c.lofiFilterKf = kf(); c.lofiFilterLfo = d(); c.lofiFilterEnv = d(); c.lofiBalance = r.range(0, 100); c.lofiBalanceLfo = d(); c.lofiBalanceEnv = d();
    c.compAttack = u(); c.compSustain = u(); c.compLevel = u();
    c.limThreshold = u(); c.limRatio = r.range(0, 3); c.limAttack = u(); c.limRelease = u(); c.limLevel = u();
    c.wsType = r.range(0, 5); c.wsLevel = u(); c.wsDrive = u(); c.wsDriveLfo = d(); c.wsDriveEnv = d(); c.wsTone = u(); c.wsToneKf = kf(); c.wsToneLfo = d(); c.wsToneEnv = d();
    c.odType = r.range(0, 1); c.odLevel = u(); c.odDrive = u(); c.odDriveLfo = d(); c.odDriveEnv = d(); c.odTone = u(); c.odToneKf = kf(); c.odToneLfo = d(); c.odToneEnv = d();
    c.ampType = r.range(0, 2); c.ampBass = u(); c.ampMiddle = u(); c.ampTreble = u(); c.ampLevel = u(); c.ampGain = u(); c.ampGainLfo = d(); c.ampGainEnv = d();
    c.spkType = r.range(0, 11);
    for (Ctl::CosmEnv* e : { &c.envA, &c.envB }) {
        e->attack = drawTime(r); e->decay = drawTime(r); e->sustain = u(); e->release = drawTime(r); e->velCurve = r.range(0, 7); e->velSens = r.range(-63, 63);
        e->aSens = d(); e->dSens = d(); e->rSens = d(); e->timeKF = kf();
    }
    c.lfoWave = r.range(0, 7); c.lfoRate = drawRate(r); c.lfoOffset = r.range(0, 4); c.lfoDelay = r.one(2) ? 0 : r.range(0, 60); c.lfoFadeMode = r.range(0, 3); c.lfoFade = r.one(2) ? 0 : r.range(0, 60); c.lfoKeySync = !r.one(4);
}

// The Matrix in half the cases, one to eight controls, on the sources whose values the case draws
void drawMatrix(Rng& r, Ctl& c) {
    for (int i = 0; i < 10; ++i) c.assign[i] = r.one(2) ? 0 : r.one(8) ? 32 : r.range(0, 95);   // 32 means none, and it is a controller's number
    if (r.one(3)) { c.assign[6] = 80; c.assign[7] = 81; }              // Pad Assign X, Y, the chart's
    // Time Trip Assign X, Y: the chart's, none, Hold or Sostenuto as well, or another.  Not those with draws of their own (7, 10, 11, 1)
    const auto tripAssign = [&r](int chart) {
        if (r.one(3)) return chart;
        if (r.one(5)) return r.one(2) ? 0 : 32;
        if (r.one(8)) return r.one(2) ? 64 : 66;
        const int n = r.range(1, 95);
        return n == 7 || n == 10 || n == 11 || n == 1 ? 16 : n;
    };
    c.assign[10] = tripAssign(82);
    c.assign[11] = r.one(16) ? c.assign[10] : tripAssign(83);          // one controller can be both
    if (r.one(2)) return;
    static const int kOther[] = { 96, 96, 96, 97, 97, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 107, 108, 108 };
    const auto dest = [&] { return r.range(0, 78); };
    for (int i = r.range(0, 7); i < 8; ++i) {
        XtMatrixControl& m = c.matrix[i];
        m.source = r.one(2) ? (r.one(8) ? 32 : r.range(0, 95)) : kOther[r.range(0, (int)(sizeof kOther / sizeof kOther[0]) - 1)];
        for (int j = 0; j < 2; ++j) {
            m.dest[j] = r.one(4) ? 0 : dest();
            m.sens[j] = r.one(6) ? 0 : r.one(4) ? (r.one(2) ? 63 : -63) : r.range(-63, 63);
        }
    }
}

Ctl drawControls(Rng& r, int g, bool pcm, bool pcm2 = false) {
    Ctl c;
    drawMatrix(r, c);
    drawOsc(r, c);
    drawOsc(r, c.osc2);
    const auto drawPcm = [&r](OscC& o) {
        o.oscType = 1; o.rootKey = r.range(36, 84);
        o.robot = r.one(4);
        o.vari = !r.one(3);
        // the Start Offset in one case in three, now and then at or past the waves' 30000 frames, where the converter takes the last sample
        if (r.one(3)) { o.startOffset = r.one(4) ? r.range(0, 40000) : r.range(1, 4000); if (o.startOffset > 35000) o.startOffset = 30000; }
        if (r.one(3)) { o.loop = true; o.loopMode = r.range(0, 2); o.loopStart = (float)r.range(0, 120) / 1000.0f; o.loopEnd = o.loopStart + (float)r.range(5, 40) / 1000.0f; }   // near the start: notes are short
    };
    if (pcm) drawPcm(c);
    if (pcm2) drawPcm(c.osc2);
    drawCosm(r, c.cosm[0]); drawCosm(r, c.cosm[1]);
    c.structure = r.range(0, 2);
    c.tvaOn = !r.one(4);
    c.mod.on = r.one(3); c.mod.type = r.range(0, 4); c.mod.level = r.range(0, 127); c.mod.attack = r.range(0, 127); c.mod.release = r.range(0, 127);
    c.cosm[0].on = g % 3 == 0;
    c.cosm[1].on = g % 6 == 3 || g % 4 == 1;
    if (g % 10 == 4) c.ana.wave = c.osc2.ana.wave = 11;   // both on SUPER-SAW: its counter goes OSC 1 then OSC 2
    if (r.one(8)) c.on = false;
    if (r.one(3)) c.osc2.on = false;
    if (!c.on || pcm2) c.osc2.on = true;               // both off: XtSynth starts no voice at all (see the report)
    c.patchCoarse = r.one(2) ? 0 : r.range(-24, 24);
    c.patchFine = r.one(2) ? 0 : r.range(-50, 50);
    c.bendUp = r.range(0, 48); c.bendDown = r.range(0, 48);
    // the ends too, where the time between clocks reaches its limits (0x894f0fd0)
    c.patchTempo = r.one(3) ? 1200 : r.one(6) ? (r.one(2) ? r.range(200, 209) : r.range(2450, 2500)) : r.range(200, 2500);
    c.clockSource = r.one(3);
    if (r.one(3)) { c.scaleTune = true; for (int& s : c.scale) s = r.range(-100, 100); }
    c.attack = drawTime(r); c.decay = drawTime(r); c.sustain = r.range(0, 127); c.release = drawTime(r);
    c.aSens = r.one(2) ? 0 : r.range(-63, 63); c.dSens = r.one(2) ? 0 : r.range(-63, 63); c.rSens = r.one(2) ? 0 : r.range(-63, 63);
    c.timeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    c.ampVelSens = r.range(-63, 63); c.ampVelCurve = r.range(0, 7);
    c.tvaLevel = r.one(3) ? 127 : r.range(0, 127); c.tvaLevelKF = r.one(2) ? 0 : 10 * r.range(-20, 20); c.tvaLevelLfo = r.one(2) ? 0 : r.range(-63, 63);
    c.tvaPan = r.one(3) ? 64 : r.range(0, 127); c.tvaPanKF = r.one(2) ? 0 : 10 * r.range(-20, 20); c.tvaPanLfo = r.one(2) ? 0 : r.range(-63, 63);
    c.mfxSend = r.one(2) ? 127 : r.range(0, 127);
    c.chorusSend = r.one(2) ? 0 : r.range(0, 127); c.reverbSend = r.one(2) ? 0 : r.range(0, 127);
    c.outputAssign = r.one(2) ? 0 : r.range(0, 2); c.parallel = !r.one(3); c.outGainTg = r.one(2) ? 0 : r.range(-48, 48);
    c.tvaLfoWave = r.range(0, 7); c.tvaLfoRate = drawRate(r); c.tvaLfoOffset = r.range(0, 4); c.tvaLfoDelay = r.one(2) ? 0 : r.range(0, 60);
    c.tvaLfoFadeMode = r.range(0, 3); c.tvaLfoFade = r.one(2) ? 0 : r.range(0, 60); c.tvaLfoKeySync = !r.one(4);
    // now and then the pan LFO at full depth on a slow square from the note's start, one channel's gain 0 all through, and
    // no release, so that the other's is still up as the envelope ends: a voice's end must look at both
    if (r.one(16)) { c.tvaPanLfo = r.one(2) ? 63 : -63; c.tvaLfoWave = 3; c.tvaLfoFade = c.tvaLfoDelay = 0; c.tvaLfoOffset = 2; c.release = 0; c.rSens = 0; c.tvaLevel = 127; c.tvaOn = true; c.tvaLfoRate = 30; c.tvaLfoKeySync = true; c.tvaLfoFadeMode = 0; }
    // LFO 5 a square held at full Pan Depth, and the release whose last step before its end is largest: as the envelope
    // ends one channel's gain is at 0 and the other's is not, and the tone goes on until both are (0x894f54b0)
    if (r.one(16)) {
        c.tvaOn = true;
        c.tvaLfoWave = 3; c.tvaPanLfo = 63; c.tvaLfoRate = r.range(0, 10); c.tvaLfoOffset = 2; c.tvaLfoDelay = c.tvaLfoFade = 0; c.tvaLfoKeySync = true;
        c.sustain = r.range(70, 127); c.release = 16; c.rSens = 0; c.timeKF = 0;
    }
    c.octave = r.one(3) ? r.range(-3, 3) : 0;
    if (r.one(2)) { c.portamento = true; c.portaMode = r.range(0, 1); c.portaType = r.range(0, 1); c.portaTime = r.range(0, 127); c.portaVeloSens = r.range(-63, 63); }
    return c;
}

// ── Compare ─────────────────────────────────────────────────────────────────
constexpr size_t kBenderOnly = (size_t)-1;     // in the sweep: the bender moved, no knob
constexpr int kCcNum[] = { 7, 11, 10, 1, 91, 93 };
constexpr int kCcCount = (int)(sizeof kCcNum / sizeof *kCcNum);
constexpr size_t kCcOnly = (size_t)-1 - kCcCount;   // and these: CC 7, 11, 10, 1, 91 or 93 moved, no knob
constexpr size_t kModeOnly = kCcOnly - 1;      // and an All Notes Off, which is no update
constexpr size_t kDestOnly = (size_t)-100;     // and these 79: one Dest of the Matrix on CC 2, which moves alone
constexpr size_t kDestKnob = (size_t)-200;     // and these 79: the same with CC 2 still, and its Sens moved
constexpr size_t kCosmPrm = (size_t)-300;      // and these 64: each COSM type's PRM1, PRM2, LFO-PRM1, LFO-PRM2 on CC 2, by a COSM in the middle of its knobs
constexpr int kDestCc = 2;
// Every field XtSynth keeps in a list of what has changed (setControls): the sweep moves
// these one at a time, where nothing else moving can hide one left out of a list.
std::vector<size_t> listedFields() {
    static const Ctl c;
    std::vector<size_t> f;
    const auto add = [&](const auto& x) { f.push_back((size_t)(reinterpret_cast<const char*>(&x) - reinterpret_cast<const char*>(&c))); };
    for (const auto* x : { &c.attack, &c.decay, &c.sustain, &c.release, &c.aSens, &c.dSens, &c.rSens, &c.timeKF, &c.patchCoarse, &c.portaTime, &c.portaVeloSens, &c.portaType }) add(*x);
    add(c.portamento); add(c.tvaOn);
    for (const OscC* o : { static_cast<const OscC*>(&c), static_cast<const OscC*>(&c.osc2) })
        for (const int* x : { &o->pVel.aSens, &o->pVel.dSens, &o->pVel.rSens, &o->pVel.timeKF, &o->pVel.sens, &o->tVel.aSens, &o->tVel.dSens, &o->tVel.rSens, &o->tVel.timeKF, &o->tVel.sens, &o->fVel.aSens, &o->fVel.dSens, &o->fVel.rSens, &o->fVel.timeKF, &o->fVel.sens,
                              &o->pAttack, &o->pDecay, &o->pSustain, &o->pRelease, &o->tAttack, &o->tDecay, &o->tSustain, &o->tRelease, &o->fAttack, &o->fDecay, &o->fSustain, &o->fRelease,
                              &o->oAttack, &o->oDecay, &o->oSustain, &o->oRelease, &o->oASens, &o->oDSens, &o->oRSens, &o->oTimeKF,
                              &o->lfoWave, &o->lfoRate, &o->lfoOffset, &o->lfoDelay, &o->lfoFadeMode, &o->lfoFade, &o->oscCoarse, &o->pitchKF }) add(*x);
    for (const Ctl::Cosm& k : c.cosm) {
        for (const Ctl::CosmEnv* e : { &k.envA, &k.envB })
            for (const int* x : { &e->attack, &e->decay, &e->sustain, &e->release, &e->aSens, &e->dSens, &e->rSens, &e->timeKF }) add(*x);
        for (const int* x : { &k.lfoWave, &k.lfoRate, &k.lfoOffset, &k.lfoDelay, &k.lfoFadeMode, &k.lfoFade }) add(*x);
    }
    add(c.mod.on);
    for (const int* x : { &c.mod.type, &c.mod.level, &c.mod.attack, &c.mod.release }) add(*x);
    for (const int* x : { &c.tvaLfoWave, &c.tvaLfoRate, &c.tvaLfoOffset, &c.tvaLfoDelay, &c.tvaLfoFadeMode, &c.tvaLfoFade,
                          &c.tvaLevel, &c.tvaLevelKF, &c.tvaLevelLfo, &c.tvaPan, &c.tvaPanKF, &c.tvaPanLfo, &c.mfxSend,
                          &c.chorusSend, &c.reverbSend, &c.outputAssign, &c.outGainTg }) add(*x);
    add(c.parallel);
    // the tempo's, which sweepWave gives what a move of them needs
    add(c.patchTempo); add(c.clockSource); add(c.assign[10]); add(c.assign[11]);
    for (const OscC* o : { static_cast<const OscC*>(&c), static_cast<const OscC*>(&c.osc2) }) { add(o->tempoSync); add(o->timeTrip); add(o->beatKeep); add(o->vari); add(o->startOffset); add(o->playbackMode); }
    // a stray or missing update shows only when a COSM's block has moved since its last update, so several goes at those
    for (int rep = 0; rep < 3; ++rep) {
        for (size_t i = 0; i < (size_t)kCcCount; ++i) f.push_back(kCcOnly + i);
        f.push_back(kModeOnly);
        f.push_back(kBenderOnly);
    }
    for (int d = 1; d < 79; ++d) { f.push_back(kDestOnly + (size_t)d); f.push_back(kDestKnob + (size_t)d); }
    for (size_t i = 0; i < 64; ++i) f.push_back(kCosmPrm + i);
    return f;
}

// In the sweep: the tempo's fields, seen only on a wave with the pad held, Vari Sw, the Start Offset and Playback Mode.  Which OSC has the wave, or 0.
int sweepWave(size_t at) {
    static const Ctl c;
    const auto is = [&](const auto& x) { return at == (size_t)(reinterpret_cast<const char*>(&x) - reinterpret_cast<const char*>(&c)); };
    if (is(c.patchTempo) || is(c.clockSource) || is(c.assign[10]) || is(c.assign[11]) || is(c.tempoSync) || is(c.timeTrip) || is(c.beatKeep) || is(c.vari) || is(c.startOffset) || is(c.playbackMode)) return 1;
    return is(c.osc2.tempoSync) || is(c.osc2.timeTrip) || is(c.osc2.beatKeep) || is(c.osc2.vari) || is(c.osc2.startOffset) || is(c.osc2.playbackMode) ? 2 : 0;
}

struct Report {
    std::map<std::string, long> kinds;
    long checks = 0;
    int caseNo = 0, firstCase = -1;
    bool first = std::getenv("XT_TONE_FIRST") != nullptr;      // only each case's first difference
    bool stop = std::getenv("XT_TONE_STOP") != nullptr;        // the run ends at its first difference
    void bad(const std::string& kind, const char* fmt, double a, double b, int blk) {
        if (stop) { std::printf("tone: 1 different, at case %d: %s\n", caseNo, kind.c_str()); std::exit(0); }
        long& n = kinds[kind];
        const bool show = first ? firstCase != caseNo : n < 3;
        ++n;
        firstCase = caseNo;
        if (show) { std::printf("  case %d block %d %s: ", caseNo, blk, kind.c_str()); std::printf(fmt, a, b); std::printf("\n"); }
    }
};

// Equal, the two zeros included: a sign on a zero has no effect on anything after it.
bool bits(float a, float b) { return a == b || std::memcmp(&a, &b, 4) == 0; }

}  // namespace

int main(int argc, char** argv) { return XtToneProbe::run(argc, argv); }

int XtToneProbe::run(int argc, char** argv) {
    if (argc < 2 || !xtSynthSetRom(argv[1])) { std::fprintf(stderr, "usage: xt_tone rom.2.bin [cases] [seed] [inputs dir]\n"); return 2; }
#if defined(__aarch64__)
    {   // denormals flushed, as the SH-4 does (FPSCR.DN = 1) and the plugin's audio thread does
        uint64_t fpcr;
        __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
        __asm__ volatile("msr fpcr, %0" : : "r"(fpcr | (1ull << 24)));
    }
#endif
    const int nCases = argc > 2 ? std::atoi(argv[2]) : 300;
    Rng r{ argc > 3 ? (uint32_t)std::atoi(argv[3]) : 2024u };
    Firmware fw;
    if (!fw.open(argv[1])) { std::fprintf(stderr, "firmware\n"); return 2; }
    // The PCM cases' waves: SOLO mono and stereo, BACKING, ENSEMBLE
    std::vector<TestWave> waves, liteWaves, eventWaves;
    // The COSMs' buffers each case (the pool's slot finder, hooked): past the waves, which they'd write over, and below the zones' chunks
    uint32_t cosmBase = kRam + 0x1000000;
    if (argc > 4) {
        const std::string dir = std::string(argv[4]) + "/";
        struct W { const char* file; int ch, type; } list[] = { { "vocal.bin", 1, 1 }, { "w358.bin", 2, 1 }, { "drum.bin", 2, 3 }, { "chord.bin", 1, 5 } };
        uint32_t next = kRam + 0x200000;
        for (const W& w : list) {
            TestWave t;
            if (!loadWave(fw, next, dir + w.file, w.ch, w.type, 30000, t)) { std::fprintf(stderr, "wave %s\n", w.file); return 2; }
            waves.push_back(std::move(t));
        }
        // Band waves of several events, for Playback Mode STEP and EVENT: 4, 6, 8 and 12 hits, BACKING and ENSEMBLE, mono and stereo.  12 hits
        // make 13 events, a list whose last index is a multiple of 12, where EVENT's rounding up to octaves (0x894e1200) has nothing to add.  The
        // encoder merges hits closer than about 3000 frames, so those are 48000 frames long
        struct H { int hits, ch, type; long frames; };
        for (const H& w : { H{ 4, 1, 3, 30000 }, H{ 6, 2, 5, 30000 }, H{ 8, 1, 5, 30000 }, H{ 4, 2, 3, 30000 }, H{ 12, 1, 3, 48000 }, H{ 12, 2, 5, 48000 } }) {
            TestWave t;
            if (!makeWave(fw, next, hitsWave(w.hits, w.ch, w.frames, 777u + (uint32_t)w.hits), w.ch, w.type, t)) { std::fprintf(stderr, "hits wave\n"); return 2; }
            eventWaves.push_back(std::move(t));
        }
        checkSoloRecords(dir, "vocal.bin", 1);
        checkSoloRecords(dir, "drum.bin", 2);
        // LITE waves (type 0), mono and stereo: the samples alone, for the last cases
        for (const W& w : { W{ "vocal.bin", 1, 0 }, W{ "w358.bin", 2, 0 } }) {
            TestWave t;
            if (!loadWave(fw, next, dir + w.file, w.ch, w.type, 30000, t)) { std::fprintf(stderr, "LITE wave %s\n", w.file); return 2; }
            liteWaves.push_back(std::move(t));
        }
        cosmBase = std::max(cosmBase, (next + 0xfffffu) & ~0xfffffu);
    }
    // Playback Mode's waves: the hits, the others, and LITE
    std::vector<TestWave> playWaves = eventWaves;
    playWaves.insert(playWaves.end(), waves.begin(), waves.end());
    playWaves.insert(playWaves.end(), liteWaves.begin(), liteWaves.end());
    XtSynth syn;
    syn.prepare(44100.0);
    syn.setSample(nullptr, 0, 1);                       // the Wave Numbers patchOf gives, from the first patch on, as the firmware's
    syn.setSample(nullptr, 1, 2);
    Report rep;
    long blocks = 0, notes = 0, wild = 0, legatos = 0, truncates = 0, zoneNotes = 0, zonesSeen = 0, playerTakes = 0, endSkips = 0, eventNotes = 0;
    float L[32], R[32];
    uint32_t fwFree[16][3] = {};            // the free-running objects of LFO 1, 2 and 5, by zone
    static constexpr uint32_t kFreeLfo[3] = { 1, 2, 5 };
    FwNote fn[2];
    const char* dumpWhich = std::getenv("XT_TONE_DUMP");
    // After the random cases, the sweep: each listed field moved during a note, once in
    // each stage of the envelopes (attack, decay, sustain, release).
    const std::vector<size_t> listed = listedFields();
    const int nSweep = (int)listed.size() * 4;
    // The results so far, and the counts started again for the cases that follow
    long total = 0;
    const auto summary = [&](const char* name) {
        std::printf("%s: %ld notes, %ld taken by a sounding tone, %ld truncated, %ld blocks, %ld checks\n", name, notes, legatos, truncates, blocks, rep.checks);
        std::printf("%s: %ld notes left off, a read outside memory; %ld band reads far from their wave\n", name, wild, fw.bandReads.far);
        long sub = 0;
        for (const auto& k : rep.kinds) { std::printf("  %-28s %ld different\n", k.first.c_str(), k.second); sub += k.second; }
        std::printf("%s: %ld different\n", name, sub);
        total += sub;
        rep.kinds.clear();
        rep.checks = notes = legatos = truncates = blocks = wild = fw.bandReads.far = 0;
    };
    // A block's clocks on the firmware's side: an outside clock has the timer's ms in it first, from the harness's
    // own count, and each clock is an F8 before it reaches the tones
    const auto fwClocks = [&]() {
        const long at = syn.hostFrames;
        if (syn.clockExt)
            for (long n = (at + 32) * 1000 / 44100 - at * 1000 / 44100; n > 0; --n)
                if (!fw.timer()) return false;
        for (int k = syn.clocksDue(); k > 0; --k) {
            if (syn.clockExt && !fw.clockIn()) return false;
            if (!fw.clock((uint32_t)(at / 32))) return false;
        }
        return true;
    };
    // The clock's object on both sides
    const auto clockFields = [&](XtSynth& s, int blk) {
        rep.checks += 3;
        const int seen = (int8_t)fw.c.rd8(kClockObj + 0x41), since = fw.c.rd16(kClockObj + 0x46), six = fw.c.rd16(kClockObj + 0x48);
        if (seen != s.clockSeen) rep.bad("clock's count of six", "fw %.0f port %.0f", seen, s.clockSeen, blk);
        if (since != s.clockSince) rep.bad("clock's ms", "fw %.0f port %.0f", since, s.clockSince, blk);
        if (six != s.clockSix) rep.bad("clock's six", "fw %.0f port %.0f", six, s.clockSix, blk);
    };
    const int nOsc2 = waves.empty() ? 0 : nCases / 2;
    const int tickFrom = nCases + nSweep + nOsc2, nTick = 16 * 3;     // each COSM type, each kind of second update
    // Then the firmware's own patches, each a zone of it at random, with the case's waves on its PCM OSCs and a tuning of their own in half of them; and
    // random settings on waves with their own tuning and a backward loop of their own, which is what a V-Synth wave brings.  XT_TONE_PATCHES: a folder of .p00 instead of the image's.
    std::vector<std::vector<uint8_t>> patchFiles;
    {
        std::string image = argv[1];
        const auto slash = image.find_last_of('/');
        image = (slash == std::string::npos ? std::string() : image.substr(0, slash + 1)) + "V-SynthXTDiskImage.bin.0.bin";
        XtDiskImage disk;
        if (disk.open(image))
            for (const auto& e : disk.list("Internal.prj/Patch")) patchFiles.push_back(disk.read(e));
        if (const char* dir = std::getenv("XT_TONE_PATCHES"))
            for (const auto& e : std::filesystem::directory_iterator(dir)) {
                if (e.path().extension() != ".p00") continue;
                std::ifstream f(e.path(), std::ios::binary);
                patchFiles.emplace_back(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
            }
    }
    const int patchFrom = tickFrom + nTick, nPatch = waves.empty() ? 0 : std::min((int)patchFiles.size(), nCases / 2);
    const int tuneFrom = patchFrom + nPatch, nTune = waves.empty() ? 0 : nCases / 4;
    // Then LITE waves (kind 1), on OSC 1 or OSC 2, with a tuning of their own in half of them: last, so that the cases before them draw as they did
    const int liteFrom = tuneFrom + nTune, nLite = waves.empty() ? 0 : nCases / 4;
    // The firmware's own patches, every zone of each that has one, as xtPatchControls reads them.  What's patchOf's for these controls must be what's in the store:
    // the whole-tone cases above hold patchOf's IDs to XtSynth's fields, so this holds a real patch to them.  XT_TONE_PATCHES: a folder of .p00 besides, not in fcheck.
    {
        // The effects' too: each block's slots count from the firmware's own table (0x897a100c), and a type the patch isn't on goes back to its defaults
        const auto slotsOf = [&](int b) { int16_t v; std::memcpy(&v, fw.rom.data() + (0x897a100c - kRomBase) + 2 * b, 2); return (int)v; };
        PatchTally t;
        long fxChecks = 0, fxDifferent = 0;
        int nPatches = 0, nZones = 0;
        for (const auto& file : patchFiles) {
            XtPatchFile p;
            if (!xtPatchRead(file.data(), file.size(), p)) continue;
            ++nPatches;
            for (int z = 0; z < xtPatchZones(p.store); ++z) {
                ++nZones;
                patchRoundTrip(p.store, z, xtPatchControls(p.store, z, Ctl{}), t);
            }
            std::unordered_map<int, int> fx;
            for (const int id : { 0x120, 0x121, 0x122, 0x177, 0x178, 0x179, 0x17a, 0x1fc, 0x1fd, 0x1fe, 0x230, 0x231 }) fx[id] = xtPatchValue(p.store, id);
            for (int b = 0; b < 3; ++b) {
                static constexpr int kTypeIds[3] = { 0x177, 0x1fc, 0x230 };
                for (int ty = 0; ty < xtEffectTypes(b); ++ty) {
                    int lo = 0, hi = -1;
                    xtEffectTypeIds(b, ty, lo, hi);
                    for (int id = lo; id <= hi; ++id) fx[id] = ty == xtPatchValue(p.store, kTypeIds[b]) ? xtPatchTyped(p.store, slotsOf(b), id) : xtTypedDesc(id)->def;
                }
            }
            const auto list = xtPatchEffects(p.store);
            ++fxChecks;
            if (list.size() != fx.size()) ++fxDifferent;
            for (const XtPatchFx& e : list) {
                ++fxChecks;
                const auto it = fx.find(e.id);
                if (it == fx.end() || it->second != e.value) ++fxDifferent;
            }
        }
        std::printf("patches: %d, %d zones, %ld values, %ld different; %ld of their effects' values, %ld different\n", nPatches, nZones, t.values, t.different, fxChecks, fxDifferent);
        for (const auto& kv : t.byId) std::printf("  id %03x%s %ld different\n", kv.first & 0xffff, kv.first >> 16 ? " (COSM 2)" : "", kv.second);
        total += t.different + fxDifferent;
        if (rep.stop && t.different + fxDifferent) { std::printf("tone: 1 different, in the patches\n"); return 0; }
    }
    // Then a patch's zones, last of all: zones 2…16 each its own settings and waves, the upper keys over the keyboard (XtSynth::Zones)
    const int zoneFrom = liteFrom + nLite, nZone = waves.empty() ? 0 : nCases / 4;
    // Then Playback Mode, last: two notes of a part, each OSC on a mode of its own, the second while the first is held or after it
    const int playFrom = zoneFrom + nZone, nPlay = waves.empty() ? 0 : nCases / 4;
    const auto zoneLine = [&] {
        int n = 0;
        for (int z = 1; z < 16; ++z) n += (zonesSeen >> z) & 1;
        std::printf("tone, zones: %ld notes on zones 2 to 16, %d of those zones\n", zoneNotes, n);
    };
    // Each zone's OSC's own copy of its wave's header and loop chunks, as a zone's settings of the wave's own are its own here
    constexpr uint32_t kZoneChunks = kRam + 0x2800000;
    for (int g = 0; g < playFrom + nPlay; ++g) {
        rep.caseNo = g;
        const bool playCase = g >= playFrom, zoneCase = g >= zoneFrom && !playCase;
        const bool patchCase = g >= patchFrom && g < tuneFrom, tuneCase = g >= tuneFrom && g < liteFrom, liteCase = g >= liteFrom && !zoneCase && !playCase, extra = patchCase || tuneCase || liteCase || zoneCase || playCase;
        const bool twoInTick = g >= tickFrom && !extra;
        if (g == nCases + nSweep) summary("tone");
        if (nOsc2 && g == tickFrom) summary("tone, a wave on OSC 2");
        if (nPatch + nTune && g == patchFrom) summary("tone, a second update in a tick");
        if (nPatch && g == tuneFrom) summary("tone, the firmware's patches");
        if (nLite && g == liteFrom) summary("tone, a wave's own tuning");
        if (nZone && g == zoneFrom) summary("tone, LITE waves");
        if (nPlay && g == playFrom) { zoneLine(); summary("tone, zones"); syn.setZones(nullptr); }
        const bool sweep = g >= nCases && g < nCases + nSweep;
        const size_t sweepAt = sweep ? listed[(size_t)(g - nCases) / 4] : 0;
        const bool sweepCosm = sweep && sweepAt >= kCosmPrm && sweepAt < kCosmPrm + 64;
        const bool sweepDest = (sweep && sweepAt >= kDestOnly && sweepAt < kDestOnly + 79) || sweepCosm, sweepSens = sweep && sweepAt >= kDestKnob && sweepAt < kDestKnob + 79;
        const int sweepStage = sweep ? (g - nCases) % 4 : 0;
        const int sweepOn = sweep && !waves.empty() ? sweepWave(sweepAt) : 0;
        Ctl patchCtl;
        if (patchCase) {
            XtPatchFile pf;
            const auto& file = patchFiles[(size_t)((long)(g - patchFrom) * (long)patchFiles.size() / nPatch)];
            if (!xtPatchRead(file.data(), file.size(), pf)) { std::fprintf(stderr, "patch\n"); return 1; }
            patchCtl = xtPatchControls(pf.store, r.range(0, xtPatchZones(pf.store) - 1), Ctl{});
            if (!patchCtl.on && !patchCtl.osc2.on) patchCtl.on = true;   // both off: XtSynth starts no voice at all (see the report)
        }
        const bool osc2Case = patchCase ? patchCtl.osc2.oscType == 1 : (g >= nCases + nSweep && !twoInTick && !tuneCase && !liteCase && !zoneCase && !playCase) || sweepOn == 2 || ((tuneCase || liteCase || zoneCase || playCase) && g % 2 == 0);
        // one case in two plays a wave on OSC 1 (with the analog OSC 2 or alone); the last
        // ones a wave on OSC 2, over OSC 1 off, analog or on another wave
        std::vector<TestWave>& pool = liteCase ? liteWaves : playCase ? playWaves : waves;
        const TestWave* wv2 = osc2Case ? &pool[(size_t)r.range(0, (int)pool.size() - 1)] : nullptr;
        const TestWave* wv = (patchCase ? patchCtl.oscType == 1 : sweepOn == 1 || (!sweep && !twoInTick && g % 2 == 1)) && !pool.empty() ? &pool[(size_t)r.range(0, (int)pool.size() - 1)] : nullptr;
        // A patch's STEP or EVENT on a SOLO wave needs the XT's segments by events (not done): that OSC plays the next band wave instead
        const auto band = [&](const TestWave* w) {
            for (size_t i = 0; w && w->type == 1 && i < pool.size(); ++i) {
                const TestWave* x = &pool[((size_t)(w - pool.data()) + i) % pool.size()];
                if (x->type == 3 || x->type == 5) return x;
            }
            return w;
        };
        if (patchCase && patchCtl.osc2.playbackMode >= 2) wv2 = band(wv2);
        if (patchCase && patchCtl.playbackMode >= 2) wv = band(wv);
        if (wv && wv == wv2) wv = &pool[(size_t)((wv - pool.data() + 1) % (long)pool.size())];   // a wave's loop is its own
        if (patchCase && patchCtl.playbackMode >= 2 && wv && wv->type == 1) {
            wv = band(wv);
            for (size_t i = 0; wv == wv2 && i < pool.size(); ++i) wv = band(&pool[((size_t)(wv - pool.data()) + 1) % pool.size()]);
        }
        Ctl ctl = drawControls(r, g, wv != nullptr, wv2 != nullptr);
        if (patchCase) {   // what a patch has, on what the wave has of its own
            const Ctl d = ctl;
            ctl = patchCtl;
            ctl.rootKey = d.rootKey; ctl.loopMode = d.loopMode; ctl.loopStart = d.loopStart; ctl.loopEnd = d.loopEnd; ctl.origTempo = d.origTempo;
            ctl.osc2.rootKey = d.osc2.rootKey; ctl.osc2.loopMode = d.osc2.loopMode; ctl.osc2.loopStart = d.osc2.loopStart; ctl.osc2.loopEnd = d.osc2.loopEnd; ctl.osc2.origTempo = d.osc2.origTempo;
        }
        // A wave's fine tuning and backward loop, in the firmware's memory and on its encoded form; no tuning in all the other cases
        const auto waveOwn = [&](const TestWave& t) {
            int headerCents = 0, fine = 0, tune = 0, rateF = 0, rateB = 0;
            t.enc->bwdMode = -1;
            if (extra && r.one(2)) {
                headerCents = r.one(2) ? 0 : r.range(-60, 60);
                fine = r.one(2) ? 0 : r.range(-100, 100);
                tune = r.one(4) ? r.range(-1500, 1500) : r.range(-120000, 172956);   // as far as 250 real waves go
                const int last = (int)t.fw.frames - 1, a = r.range(0, std::max(last - 32, 0));
                if (r.one(2)) { t.enc->bwdMode = r.range(0, 2); t.enc->bwdStart = a; t.enc->bwdEnd = std::min(a + r.range(16, 4000), last); }
                if (!r.one(4)) { rateF = r.range(-300, 300); rateB = r.range(-300, 300); }   // real waves' +0x16 run to about 300 either way
            }
            t.enc->loopRate[0] = xtWaveLoopRate(rateF);
            t.enc->loopRate[1] = xtWaveLoopRate(rateB);
            fw.c.wr16(t.fw.chunk[4] + 0x14, (uint16_t)(int16_t)rateF);
            fw.c.wr16(t.fw.chunk[4] + 0x16, (uint16_t)(int16_t)rateB);
            t.enc->tune = xtWaveTune(headerCents, fine, tune);
            fw.c.wr32(t.fw.chunk[2] + 0x18, (uint32_t)headerCents);
            fw.c.wr8(t.fw.chunk[4] + 0x1b, (uint8_t)fine);
            fw.c.wr32(t.fw.chunk[4], (uint32_t)tune);
        };
        if (wv) waveOwn(*wv);
        if (wv2) waveOwn(*wv2);
        // A long note under Tempo Sync, at the tempo's top and the Original Tempo's foot, with no loop: its beat position
        // runs past the wave's end, and a pad held and let go has the seek stop there (0x8950fed0, 0x89510bf0)
        const bool farCase = !sweep && !twoInTick && !patchCase && g % 23 == 7 && wv && r.one(2);
        // And the others of those long notes: a finger going backward past the wave's start, held there a while and let go,
        // which a gesture over MIDI reaches easily and the short random ones don't
        const bool backCase = !sweep && !twoInTick && !patchCase && g % 23 == 7 && wv && !farCase;
        if (backCase) { ctl.on = ctl.timeTrip = ctl.beatKeep = true; ctl.loop = false; }
        if (farCase) {
            ctl.on = ctl.tempoSync = ctl.timeTrip = ctl.beatKeep = true;
            ctl.loop = false;
            ctl.origTempo = 400000;
            ctl.patchTempo = 2500;
            ctl.clockSource = 0;
        }
        // a loop's start at the edge of its rounding in half the cases: its region is checked to the sample
        const auto loopEdge = [&](OscC& o, const TestWave* t) {
            if (!t || !o.loop || !r.one(2)) return;
            const float last = (float)(t->fw.frames - 1);
            o.loopStart = (std::floor(o.loopStart * last) + 0.5f) / last;
        };
        loopEdge(ctl, wv);
        loopEdge(ctl.osc2, wv2);
        if (sweep) {
            // analog on both OSCs, all the envelopes and LFOs bound and running, a
            // portamento gliding: whatever a field moves can be seen
            ctl.on = ctl.osc2.on = true;
            ctl.tvaOn = true;
            ctl.lfoKeySync = ctl.osc2.lfoKeySync = true;
            ctl.portamento = true;
            ctl.portaTime = 100;
            ctl.oscTimeOffset = ctl.osc2.oscTimeOffset = 2;
            // Vari Sw moved alone, with Robot Voice on, whose flag and pitch follow it during the note (0x894f3fa8, the converter)
            if (sweepOn) { static const Ctl c0; const size_t at = sweepAt, v1 = (size_t)(reinterpret_cast<const char*>(&c0.vari) - reinterpret_cast<const char*>(&c0)),
                                                v2 = (size_t)(reinterpret_cast<const char*>(&c0.osc2.vari) - reinterpret_cast<const char*>(&c0));
                           if (at == v1) { ctl.robot = true; ctl.beatKeep = false; }   // and no Beat Keep: its way back would run one player's seek on the other's state
                           if (at == v2) { ctl.osc2.robot = true; ctl.osc2.beatKeep = false; }
                           // Playback Mode on a band wave from EVENT, with the Loop on: the move takes the update's Loop bit to the player (0x894f3fb6)
                           const size_t p1 = (size_t)(reinterpret_cast<const char*>(&c0.playbackMode) - reinterpret_cast<const char*>(&c0)),
                                        p2 = (size_t)(reinterpret_cast<const char*>(&c0.osc2.playbackMode) - reinterpret_cast<const char*>(&c0));
                           const auto band = [](const TestWave* w) { return w && (w->type == 3 || w->type == 5); };
                           if (at == p1 && band(wv)) { ctl.loop = true; ctl.playbackMode = 3; }
                           if (at == p2 && band(wv2)) { ctl.osc2.loop = true; ctl.osc2.playbackMode = 3; } }
            // two COSMs whose block takes the envelopes' levels: a Comb, a Side Band Filter
            for (Ctl::Cosm& k : ctl.cosm) { k.on = true; k.lfoKeySync = true; }
            ctl.cosm[0].type = 4;
            ctl.cosm[0].combDetuneEnv = ctl.cosm[0].combToneEnv = 40;
            ctl.cosm[1].type = 5;
            ctl.cosm[1].sb[0].detuneEnv = ctl.cosm[1].sb[0].widthEnv = 40;
            const auto times = [&](int& a, int& d, int& su, int& re) {
                a = sweepStage == 0 ? 70 : 0;
                d = sweepStage == 1 ? 70 : 0;
                su = 90;
                re = sweepStage == 3 ? 70 : 0;
            };
            times(ctl.attack, ctl.decay, ctl.sustain, ctl.release);
            for (OscC* o : { static_cast<OscC*>(&ctl), static_cast<OscC*>(&ctl.osc2) }) {
                times(o->pAttack, o->pDecay, o->pSustain, o->pRelease);
                times(o->tAttack, o->tDecay, o->tSustain, o->tRelease);
                times(o->fAttack, o->fDecay, o->fSustain, o->fRelease);
                times(o->oAttack, o->oDecay, o->oSustain, o->oRelease);
                o->pDepth = o->tDepth = o->fDepth = 40;
                o->oASens = o->oDSens = o->oRSens = 20;
                o->lfoFade = 0;
                o->lfoDelay = 0;
            }
            for (Ctl::Cosm& k : ctl.cosm)
                for (Ctl::CosmEnv* e : { &k.envA, &k.envB }) {
                    times(e->attack, e->decay, e->sustain, e->release);
                    e->aSens = e->dSens = e->rSens = 20;
                    e->timeKF = 50;
                }
            ctl.aSens = ctl.dSens = ctl.rSens = 20;
            // the TVA's LFO on both its depths, a level and a pan not at their limits
            ctl.tvaLevel = ctl.mfxSend = 100; ctl.tvaPan = 40; ctl.tvaLevelLfo = ctl.tvaPanLfo = 40;
            // the sends the same, and the tone on MULTI, but for Output Mode, which only a tone going to DIR can show
            static const Ctl probe;
            ctl.chorusSend = ctl.reverbSend = 100; ctl.parallel = true; ctl.outGainTg = 0;
            ctl.outputAssign = sweepAt == (size_t)(reinterpret_cast<const char*>(&probe.parallel) - reinterpret_cast<const char*>(&probe)) ? 2 : 0;
            // and fading in, or in a short delay, so that a move of each of its fields can be seen
            ctl.tvaLfoKeySync = true; ctl.tvaLfoFadeMode = 0; ctl.tvaLfoFade = 60; ctl.tvaLfoDelay = sweepStage >= 2 ? 9 : 0;
            // one Dest alone, and every knob of it away from its limits: what it moves can be seen
            if (sweepDest || sweepSens) {
                for (XtMatrixControl& m : ctl.matrix) m = {};
                ctl.matrix[0].source = kDestCc;
                ctl.matrix[0].dest[0] = sweepCosm ? 0 : (int)(sweepAt - (sweepDest ? kDestOnly : kDestKnob));
                ctl.matrix[0].sens[0] = sweepSens ? 10 : 40;
            }
            // Each type's four: on COSM 1 or 2 by the stage, the knobs in the middle, every depth on, so that no sum goes unseen
            if (sweepCosm) {
                const int t = (int)(sweepAt - kCosmPrm) / 4, which = (int)(sweepAt - kCosmPrm) % 4, sec = sweepStage % 2;
                static const int kPrm[4] = { 0x2b, 0x2c, 0x34, 0x35 };
                ctl.matrix[0].dest[0] = kPrm[which] + 11 * sec;
                Ctl::Cosm& c = ctl.cosm[sec];
                c.on = true;
                c.type = t;
                c.filter = 1; c.cutoff = c.reso = c.freq1 = c.q1 = c.freq2 = c.q2 = c.tbCutoff = c.tbQ = c.tbHpf = c.tbGain = c.combTone = c.resSize = c.fsDepth = c.lofiFilter = 64;
                c.compAttack = c.compSustain = c.limAttack = c.limRelease = 64; c.ampTreble = 64;
                c.limThreshold = 10;                                       // limiting, or its Attack and Release have no effect to see
                c.wsLevel = c.wsDrive = c.wsTone = c.odLevel = c.odDrive = c.odTone = c.ampLevel = c.ampGain = 64;
                c.resBalance = c.fsBalance = c.lofiBalance = 50;
                c.lofiBit = 2; c.lofiRate = 1; c.kf = c.kf1 = c.kf2 = 0;
                c.tbKf = c.tbHpfKf = c.resSizeKf = c.fsDepthKf = c.lofiFilterKf = c.wsToneKf = c.odToneKf = 0;   // or a key's follow can hold a target at its limit
                c.combDetune = 0;
                for (Ctl::Cosm::SideBand& b : c.sb) { b.hpf = b.q = b.lpf = b.width = 64; b.detune = 0; }
                c.cutoffLfo = c.resoLfo = c.lfo1 = c.lfo2 = c.tbLfo = c.combDetuneLfo = c.combToneLfo = c.resSizeLfo = c.resBalanceLfo = c.fsDepthLfo = c.fsBalanceLfo = c.lofiFilterLfo = c.lofiBalanceLfo = c.wsDriveLfo = c.wsToneLfo = c.odDriveLfo = c.odToneLfo = c.ampGainLfo = 20;
                for (Ctl::Cosm::SideBand& b : c.sb) { b.detuneLfo = b.widthLfo = 20; }
                c.lfoWave = 3; c.lfoRate = 90; c.lfoOffset = 2; c.lfoDelay = c.lfoFade = 0; c.lfoKeySync = true;
            }
        }
        if (twoInTick) {
            // analog on both OSCs, sounding at once, no MOD; both COSMs at a Sustain soon, every
            // Env depth on, the knobs in the middle so that no target stands at a limit: what a
            // new block takes from the envelopes can be heard
            ctl.tvaOn = true;
            ctl.mod.on = false;
            ctl.attack = ctl.decay = 0; ctl.sustain = 100; ctl.ampVelSens = 0;
            for (OscC* o : { static_cast<OscC*>(&ctl), static_cast<OscC*>(&ctl.osc2) }) {
                o->on = true;
                o->oscLevel = 100; o->oscLevelKF = 0; o->oscVelSens = 0;
                o->oAttack = o->oDecay = 0; o->oSustain = 100;
            }
            for (int k = 0; k < 2; ++k) {
                Ctl::Cosm& c = ctl.cosm[k];
                c.on = true;
                c.type = (g - tickFrom + 7 * k) % 16;
                c.filter = 1; c.cutoff = c.reso = c.freq1 = c.q1 = c.freq2 = c.q2 = c.tbCutoff = c.tbQ = c.tbHpf = c.tbGain = c.combTone = c.resSize = c.fsDepth = c.lofiFilter = 64;
                c.wsLevel = c.wsDrive = c.wsTone = c.odLevel = c.odDrive = c.odTone = c.ampLevel = c.ampGain = 64;
                c.resBalance = c.fsBalance = c.lofiBalance = 50;
                c.lofiBit = 2; c.lofiRate = 1;
                c.cutoffEnv = c.resoEnv = c.env1 = c.env2 = c.tbEnv = c.tbHpfEnv = c.combDetuneEnv = c.combToneEnv = c.resSizeEnv = c.resBalanceEnv = 40;
                c.fsDepthEnv = c.fsBalanceEnv = c.lofiFilterEnv = c.lofiBalanceEnv = c.wsDriveEnv = c.wsToneEnv = c.odDriveEnv = c.odToneEnv = c.ampGainEnv = 40;
                for (Ctl::Cosm::SideBand& b : c.sb) { b.hpf = b.q = b.lpf = b.width = 64; b.detuneEnv = b.widthEnv = 40; }
                for (Ctl::CosmEnv* e : { &c.envA, &c.envB }) { e->attack = e->decay = 0; e->sustain = 90; e->velSens = 0; }
            }
        }
        // Some stereo cases where the stereo Side Band follower's words past the block matter:
        // COSM 1 next to OSC 2's buffer, OSC 2 sounding, the level moving Width and Detune.
        // At random this is rare enough for 2000 cases to miss it.  Only with OSC 1 on already:
        // until v108 a wave newly played could leave memory (band reads), which ended a case early
        // and changed the draws of all the cases after it.
        if (!patchCase && wv && wv->channels == 2 && wv->type != 1 && ctl.on && g % 10 == 3) {
            ctl.osc2.on = true;
            ctl.structure = g % 20 == 3 ? 1 : g % 40 == 13 ? 0 : 2;   // on the mix (Type1) what follows is the bus's, not OSC 2's
            ctl.cosm[0].on = true;
            ctl.cosm[0].type = g % 40 < 20 ? 5 : 6;
            for (Ctl::Cosm::SideBand& b : ctl.cosm[0].sb) b.detuneOwn = b.widthOwn = 40;
        }
        // And COSM 2 on a stereo OSC 2's buffer in the third Structure, next to the last tone's mix
        if (!patchCase && wv2 && wv2->channels == 2 && wv2->type != 1 && ctl.osc2.on && ctl.structure == 2) {
            ctl.cosm[1].on = true;
            ctl.cosm[1].type = g % 4 == 0 ? 5 : 6;
            for (Ctl::Cosm::SideBand& b : ctl.cosm[1].sb) b.detuneOwn = b.widthOwn = 40;
        }
        // what the case does besides one note: 0 none, 1 a second note on a POLY part,
        // 2 on a MONO part with Legato, 3 on a MONO part without
        const int kind = patchCase ? (ctl.mono ? (ctl.legato ? 2 : 3) : g % 2) : playCase ? (g % 4 == 3 ? 3 : 1) : sweep || twoInTick ? 2 : g % 5 == 0 ? 1 : g % 5 == 1 ? 2 : g % 5 == 2 ? 3 : 0;
        // Playback Mode by what the OSC plays: any of the four on a band wave, LITE (whose converter doesn't read it) and the analog OSC; RETRIGGER
        // or LEGATO on SOLO, whose STEP and EVENT need the XT's segments by events (not done).  A band wave's list now and then its own: some of
        // its events, out of order or the same twice, or one past its last (the player takes the last)
        if (zoneCase)                                // the zones' first, by the earlier note of its own zone alone
            for (int k = 0; k < 2; ++k) { const TestWave* t = k ? wv2 : wv; (k ? ctl.osc2.playbackMode : ctl.playbackMode) = t && t->type == 1 ? r.range(0, 1) : r.range(0, 3); }
        if (playCase) {
            for (int k = 0; k < 2; ++k) {
                const TestWave* t = k ? wv2 : wv;
                (k ? ctl.osc2.playbackMode : ctl.playbackMode) = t && t->type == 1 ? r.range(0, 1) : r.range(0, 3);
                if (!t || (t->type != 3 && t->type != 5)) continue;
                uint32_t w2 = 0;
                std::memcpy(&w2, t->enc->vpdt.data() + 8, 4);
                const int n = (int)(w2 >> 16);
                t->enc->vpno.clear();
                if (r.one(2)) for (int e = 0; e < n; ++e) t->enc->vpno.push_back((uint16_t)e);
                else for (int e = r.range(1, n >= 12 ? 2 * n : n + 2); e > 0; --e) t->enc->vpno.push_back((uint16_t)(r.one(8) ? n : r.range(0, n - 1)));   // past 24 on the long ones
                t->enc->vpnoCount = (int)t->enc->vpno.size();
                writeVpno(fw, t->fw, *t->enc);
            }
        }
        if (!patchCase) { ctl.mono = kind >= 2; ctl.legato = kind == 2; }
        if (wv) {
            syn.setSample(wv->enc, 0, 1);              // the Wave Numbers patchOf gives, which never change here
            fw.wave = wv->fw;
            fw.c.wr8(wv->fw.chunk[2] + 0xa, (uint8_t)ctl.rootKey);
            fw.c.wr32(wv->fw.chunk[2] + 0x1c, (uint32_t)ctl.origTempo);
        } else {
            // an analog case: in some, a stereo wave left loaded, which it must not read or follow
            syn.setSample(g % 4 == 2 && !waves.empty() ? waves[2].enc : nullptr, 0, 1);
            fw.wave = FwWave{};
        }
        if (osc2Case) {
            syn.setSample(wv2->enc, 1, 2);
            fw.wave2 = wv2->fw;
            fw.c.wr8(wv2->fw.chunk[2] + 0xa, (uint8_t)ctl.osc2.rootKey);
            fw.c.wr32(wv2->fw.chunk[2] + 0x1c, (uint32_t)ctl.osc2.origTempo);
        }
        const int raw[2] = { sweep || twoInTick ? 48 : r.range(0, 127), sweep || twoInTick ? 72 : r.range(0, 127) }, vel[2] = { r.range(1, 127), r.range(1, 127) }, relVel[2] = { r.range(1, 127), r.range(1, 127) };
        const int nBlocks = sweep || twoInTick ? 90 : g % 23 == 7 ? 700 : 60 + r.range(0, 120);     // some long enough to run past a wave's end
        // Hold itself as a source, on a release time, let go after the key: the release takes the new sum though its update has not run
        const bool holdSource = !sweep && !twoInTick && !patchCase && !farCase && !backCase && r.one(8);
        if (holdSource) {
            static const int kRel[] = { 0x44, 0x10, 0x07, 0x0a, 0x0d, 0x25, 0x2f, 0x32, 0x3a, 0x3d };
            ctl.matrix[0] = {};
            ctl.matrix[0].source = 64;
            ctl.matrix[0].dest[0] = kRel[r.range(0, 9)];
            ctl.matrix[0].sens[0] = r.one(2) ? 63 : -63;
            ctl.tvaOn = true;
            ctl.release = 60;
        }
        // TRIP-R as a source of three controls, on any Dest, in a case the pad moves in: a pad's move brings no update,
        // so what takes the new value is whatever next reads the sums
        // X-Y mode's pair: a finger on Pad Assign X and Y, each a controller's message, which the Matrix reads as PAD-X, PAD-Y,
        // +PAD-X and +PAD-Y.  Pad Mode is the XT's own pad's choice of pair, and no tone generator code reads it.
        const bool xyCase = !sweep && !twoInTick && !patchCase && !farCase && !backCase && !holdSource && r.one(8);
        if (xyCase) {
            if (r.one(2)) { ctl.assign[6] = 80; ctl.assign[7] = 81; }
            else { ctl.assign[6] = r.range(12, 31); ctl.assign[7] = r.range(33, 63); }
            for (int i = 0; i < 3; ++i) {
                ctl.matrix[i].source = r.range(98, 101);
                ctl.matrix[i].dest[0] = r.range(1, 78);
                ctl.matrix[i].sens[0] = r.range(-63, 63);
            }
        }
        const bool padSource = !sweep && !twoInTick && !patchCase && !farCase && !backCase && !holdSource && !xyCase && r.one(6);
        if (padSource) {
            // All of an OSC's Releases on note values, in some: a key-off then forms none of that OSC's envelope sums, which only a source with no update of its own can show.
            // Without a case for it this rests on four draws lining up.
            if (g % 3 == 0) for (OscC* o : { static_cast<OscC*>(&ctl), static_cast<OscC*>(&ctl.osc2) }) o->pRelease = o->tRelease = o->fRelease = o->oRelease = 128 + g % 17;
            // half of them on one getter's group alone, with no other control in use.  Then a key-off asks the sources only if
            // that group has an envelope it converts, so the TVA Switch off or every Release on a note value leaves the sums old.
            static const int kGroupDest[5][2] = { { 5, 0x10 }, { 0x1a, 0x25 }, { 0x2b, 0x35 }, { 0x36, 0x40 }, { 0x41, 0x47 } };
            const bool oneGroup = r.one(2);
            const int group = r.one(3) ? 4 : r.range(0, 4);
            const auto dest = [&] { return oneGroup ? r.range(kGroupDest[group][0], kGroupDest[group][1]) : r.range(1, 78); };
            for (XtMatrixControl& m : ctl.matrix) m = {};
            for (int i = 0; i < 3; ++i) {
                ctl.matrix[i].source = 102;
                ctl.matrix[i].dest[0] = dest();
                ctl.matrix[i].sens[0] = r.one(2) ? 63 : -63;
                ctl.matrix[i].dest[1] = dest();
                ctl.matrix[i].sens[1] = r.range(-63, 63);
            }
            if (oneGroup && r.one(2)) {
                const int note = r.range(128, 144);
                if (group == 4) { if (r.one(2)) ctl.tvaOn = false; else ctl.release = note; }
                else if (group >= 2) ctl.cosm[group - 2].envA.release = ctl.cosm[group - 2].envB.release = note;
                else {
                    OscC& o = group ? static_cast<OscC&>(ctl.osc2) : static_cast<OscC&>(ctl);
                    o.pRelease = o.tRelease = o.fRelease = o.oRelease = note;
                }
            }
        }
        // A voice keeps an OSC SYNC note's flag for its next note, and any update clears it (0x894f1c60).  So that next note, on
        // another MOD Type, with analog OSCs to hear it on, and an early update that moves one knob, ahead of the other draws' updates.
        const bool syncAfter = !sweep && !twoInTick && !patchCase && !wv && !wv2 && syn.voice[0].mod.syncOn() && r.one(2);
        if (syncAfter) { ctl.mod.on = true; ctl.mod.type = r.range(0, 3); ctl.on = ctl.osc2.on = true; }
        // Now and then, on a MONO part, the second note waits for the first to have finished its release, and comes
        // in the very block of the tick that will end it: truncating a voice that is already over
        const bool chaseEnd = kind == 3 && !sweep && !twoInTick && !patchCase && !farCase && !backCase && raw[0] != raw[1] && r.one(3);
        if (chaseEnd) { ctl.release = r.range(0, 20); ctl.attack = r.range(1, 40); ctl.tvaOn = true; }   // the new note starts from no gain, so the old one's last trace can be seen
        // Key-unsynced LFOs on a tone with no note value, the Rate moved onto one, and a second tone following
        // them: it starts with the first's clock step, and its first update, here the bender's, gives it its own
        const bool followCase = kind == 1 && !sweep && !twoInTick && !patchCase && raw[0] != raw[1] && r.one(4);
        if (followCase) {
            for (int* k : { &ctl.lfoRate, &ctl.osc2.lfoRate, &ctl.cosm[0].lfoRate, &ctl.cosm[1].lfoRate, &ctl.tvaLfoRate }) *k = r.range(0, 127);
            ctl.lfoKeySync = ctl.osc2.lfoKeySync = ctl.cosm[0].lfoKeySync = ctl.cosm[1].lfoKeySync = ctl.tvaLfoKeySync = false;
        }
        const int secondAt = twoInTick || chaseEnd ? -1 : sweep ? 1 : followCase ? r.range(3, 12) : kind && raw[0] != raw[1] ? r.range(1, 30) : -1;
        // one playback case in eight: the earlier key up before the second note, Hold keeping it sounding, which LEGATO doesn't take (+0x20 bit 29)
        const bool playHeld = playCase && g % 8 == 5 && secondAt >= 3;
        const int offAt[2] = { twoInTick || farCase || backCase ? 1000 : sweep ? (sweepStage == 3 ? 3 : 1000) : playHeld ? r.range(1, secondAt - 1) : padSource ? r.range(35, 60) : 20 + r.range(0, 40), twoInTick ? 1000 : sweep ? (sweepStage == 3 ? 4 : 1000) : secondAt >= 0 ? secondAt + r.range(1, 40) : -1 };
        const int liveAt = patchCase ? -1 : twoInTick ? 60 : sweep ? (sweepStage >= 2 ? 50 : 20) : followCase ? r.range(1, secondAt - 1) : syncAfter ? r.range(1, 3) : padSource ? r.range(12, 30) : g % 3 == 2 ? r.range(1, 50) : -1;     // a stage of 0 still takes a tick
        const int followAt = followCase ? secondAt + r.range(1, 8) : -1;
        // the free-running LFOs' phases, once a note has shown which objects they are
        for (int z = 0; z < 16; ++z)
            for (int n = 0; n < 3; ++n) {
                if (!fwFree[z][n]) continue;
                ++rep.checks;
                const float a = fw.f32(fwFree[z][n] + 0x10), b = n == 2 ? syn.freeTlfo[z].phase() : n ? syn.freeLfo2[z].phase() : syn.freeLfo[z].phase();
                if (!bits(a, b)) rep.bad("free lfo " + std::to_string(kFreeLfo[n]) + " phase" + (z ? " of zone " + std::to_string(z + 1) : std::string()), "fw %.9g port %.9g", a, b, -2);
            }
        // The host's transport, for Clock Source EXTERNAL.  Its tempo, past the firmware's limits in some, and perhaps
        // changing in the note; where it stands; how often it says so, its block's length; a locate or a loop's end; a stretch
        // where it says it is moving and stands still, in which no clock comes; and a stop, in which the clock runs on at the tempo.
        const auto drawBpm = [&r]() { return r.one(3) ? 120.0 : r.one(6) ? (double)r.range(2400, 4000) / 10.0 : (double)r.range(2000, 25000) / 100.0; };
        double hostBpm = drawBpm(), hostPpq = (double)r.range(-16, 3840) / 16.0;
        const int hostEvery = r.one(2) ? 1 : r.range(2, 40), hostJump = r.one(4) ? r.range(0, 100) : -1, hostChange = r.one(4) ? r.range(0, 100) : -1;
        const int hostStill = r.one(6) ? r.range(0, 60) : -1, hostStillLen = r.range(1, 120), hostStop = r.one(6) ? r.range(0, 60) : -1, hostStopLen = r.range(1, 120);
        const auto hostMoves = [&](int blk) {
            if (blk == hostJump) hostPpq = (double)r.range(0, 3840) / 16.0;
            if (blk == hostChange) hostBpm = drawBpm();
            if (blk % hostEvery == 0 || blk == hostJump) syn.hostClock(hostBpm, !(hostStop >= 0 && blk >= hostStop && blk < hostStop + hostStopLen), hostPpq);
            if (!(hostStill >= 0 && blk >= hostStill && blk < hostStill + hostStillLen)) hostPpq += 32.0 / 44100.0 * hostBpm / 60.0;
        };
        syn.hostClock(hostBpm, true, hostPpq);
        // time with nothing sounding: the free-running LFOs run on
        for (int i = r.range(0, 30); i > 0; --i) {
            if (!fwClocks()) return 1;
            if (syn.tickCount >= 441 && !fw.tickFree()) return 1;
            syn.render(L, R, 32);
        }
        // A patch's zones: zones 2…16 each drawn as a case of its own, its waves from either pool, and over the keyboard by the upper
        // keys, some out of order (the table takes the larger) and in some, as in a kit, none at 127.  Their common settings, drawn too,
        // are none of theirs: the getter gives a zone only its blocks' IDs, and XtSynth takes zone 1's for the rest.
        fw.zpar.clear();
        fw.zoneWave.clear();
        for (int& u : fw.upper) u = 127;
        if (zoneCase) {
            auto zs = std::make_shared<XtSynth::Zones>();
            const int nz = r.range(2, 16);
            std::vector<int> cut;
            for (int z = 0; z + 1 < nz; ++z) cut.push_back(r.range(0, 126));
            std::sort(cut.begin(), cut.end());
            for (int z = 0; z < 16; ++z)
                zs->upper[z] = z + 1 < nz ? cut[(size_t)z] : z + 1 == nz ? (r.one(4) ? r.range(90, 126) : 127) : r.range(0, 127);
            if (nz > 2 && r.one(4)) zs->upper[r.range(0, nz - 2)] = r.range(0, 127);
            std::copy(std::begin(zs->upper), std::end(zs->upper), fw.upper);
            fw.zpar.assign(16, {});
            std::vector<const TestWave*> owned { wv, wv2 };
            const auto anyWave = [&]() -> const TestWave* {
                const int i = r.range(0, (int)(waves.size() + liteWaves.size()) - 1);
                return i < (int)waves.size() ? &waves[(size_t)i] : &liteWaves[(size_t)i - waves.size()];
            };
            for (int z = 1; z < 16; ++z) {
                const TestWave* zw[2] = { r.one(2) ? anyWave() : nullptr, r.one(3) ? anyWave() : nullptr };
                Ctl zc = drawControls(r, g, zw[0] != nullptr, zw[1] != nullptr);
                for (int k = 0; k < 2; ++k) {
                    const TestWave* t = zw[k];
                    (k ? zc.osc2.playbackMode : zc.playbackMode) = t && t->type == 1 ? r.range(0, 1) : r.range(0, 3);   // as the playback cases draw it
                    if (!t) continue;
                    if (std::find(owned.begin(), owned.end(), t) == owned.end()) { waveOwn(*t); owned.push_back(t); }
                    OscC& o = k ? zc.osc2 : static_cast<OscC&>(zc);
                    loopEdge(o, t);
                    FwWave w = t->fw;
                    const uint32_t slot = kZoneChunks + 0x2000u * (uint32_t)(2 * z + k);
                    w.chunk[2] = slot; w.chunk[4] = slot + 0x100; w.chunk[10] = slot + 0x800;
                    uint8_t b[0x40];
                    fw.c.read(t->fw.chunk[2], b, sizeof b); fw.c.write(w.chunk[2], b, sizeof b);
                    fw.c.read(t->fw.chunk[4], b, sizeof b); fw.c.write(w.chunk[4], b, sizeof b);
                    fw.c.wr8(w.chunk[2] + 0xa, (uint8_t)o.rootKey);
                    fw.c.wr32(w.chunk[2] + 0x1c, (uint32_t)o.origTempo);
                    writeLoopEvents(fw, *t, o, &w);
                    fw.zoneWave[2999 + 1 + 2 * (uint32_t)z + (uint32_t)k] = w;
                    zs->wave[z][k] = t->enc;
                }
                zs->ctl[z] = zc;
                zs->number[z][0] = 1 + 2 * z;              // as zpar's, on every zone's OSCs
                zs->number[z][1] = 2 + 2 * z;
                fw.zpar[(size_t)z] = patchOf(zc);
                fw.zpar[(size_t)z][0x2be] = 1 + 2 * z;
                fw.zpar[(size_t)z][0x2f2] = 2 + 2 * z;
            }
            syn.setZones(zs);
        }
        syn.setControls(ctl);
        if (zoneCase) syn.pickUpSample();     // after the knobs, as a patch's zones come after its zone 1; and now, not at the first note,
                                              // as what's carried into the firmware's voice reads the zone's
        fw.par = patchOf(ctl);
        if (!fw.updatePatch()) return 1;
        // The last count of six clocks, which a case is mostly too short to reach: the readers' limits and their ends
        if (ctl.clockSource && r.one(4)) {
            static constexpr int kSixes[] = { 1, 59, 60, 61, 125, 749, 750, 751, 3000, 65535 };
            syn.clockSix = r.one(2) ? kSixes[r.range(0, 9)] : r.range(1, 65535);
            fw.c.wr16(kClockObj + 0x48, (uint16_t)syn.clockSix);
        }
        fw.bender = r.one(3) ? r.range(0, 16383) : 8192;
        syn.pitchBend(fw.bender);
        // the controllers the TVA reads: at rest in most cases, or a sweep's move would be hidden
        int cc[kCcCount] = { 127, 127, 64, 0, 127, 127 };
        if (!sweep && !twoInTick && r.one(3)) for (int& x : cc) x = r.range(0, 127);
        XtToneProbe::ccs(syn, cc, kCcNum, kCcCount);
        for (int i = 0; i < kCcCount; ++i) fw.setCc(kCcNum[i], cc[i]);
        // what the Matrix's sources read, at rest in a few cases: the other controllers and the pressures
        const bool srcRest = sweep || twoInTick || r.one(4);
        for (int n = 0; n < 96; ++n) {
            if (std::find(std::begin(kCcNum), std::end(kCcNum), n) != std::end(kCcNum)) continue;
            const int x = srcRest ? 0 : r.one(8) ? (r.one(2) ? 127 : 0) : r.range(0, 127);
            XtToneProbe::sources(syn, n, x);
            fw.setCc(n, x);
        }
        if (sweepDest || sweepSens) { XtToneProbe::sources(syn, kDestCc, sweepSens ? 120 : 30); fw.setCc(kDestCc, sweepSens ? 120 : 30); }
        const int chanPress = srcRest ? 0 : r.range(0, 127);
        XtToneProbe::press(syn, chanPress, -1, 0);
        fw.setPressure(chanPress);
        for (int k : { raw[0], raw[1] }) {
            for (int o = 0; o < 2; ++o) {                  // by the key as the note has it: with the Octave Shift or without
                const int key = o ? std::clamp(k + 12 * ctl.octave, 0, 127) : k, x = srcRest || r.one(2) ? 0 : r.range(0, 127);
                XtToneProbe::press(syn, chanPress, key, x);
                fw.setPoly(key, x);
            }
        }
        if (wv) writeLoopEvents(fw, *wv, ctl);
        if (wv2) writeLoopEvents(fw, *wv2, ctl.osc2);

        // What a voice keeps across notes (its phases, and fields a waveform does not
        // use): the firmware's tone comes from a pool, so it starts from the port's.
        auto carryFrom = [&](XtSynth::Voice& pv) {
            return [&, pv = &pv](FwNote& n) {
                const Ctl& ctl = syn.zc(fw.c.rd8(n.BL + 0x6c6) & 15);   // the note's zone's
                // the Dynamic TVF's follower, which its note-on does not clear
                for (int k = 0; k < 2; ++k) { const float lev = pv->cs[k].tvf.level(); uint32_t u; std::memcpy(&u, &lev, 4); fw.c.wr32(n.TN + (k ? 0x14ec : 0x1388) + 0x58 + 0x98, u); }
                // the two words the stereo TB Filter reads uncleared
                for (int k = 0; k < 2; ++k) {
                    float w8c, w90;
                    pv->cs[k].wordsNow(w8c, w90);
                    uint32_t u;
                    std::memcpy(&u, &w8c, 4); fw.c.wr32(n.TN + (k ? 0x14ec : 0x1388) + 0x58 + 0x8c, u);
                    std::memcpy(&u, &w90, 4); fw.c.wr32(n.TN + (k ? 0x14ec : 0x1388) + 0x58 + 0x90, u);
                }
                // FM's lines are by the tone's number, and a far tap reads the next tone's
                fw.c.wr32(n.TN + 0x1708 + 0x3c, (uint32_t)(pv - syn.voice));
                // the MOD's, which a note-on leaves: ENV RING's last level above all
                const auto wrf = [&](uint32_t at, float x) { uint32_t u; std::memcpy(&u, &x, 4); fw.c.wr32(at, u); };
                const uint32_t M = n.TN + 0x1708;
                for (int i = 0; i < 3; ++i) wrf(M + 0x18 + 4 * (uint32_t)i, pv->mod.current(i));
                wrf(M + 0x28, pv->mod.env());
                wrf(M + 0x2c, pv->mod.lastLevel());
                fw.c.wr32(M + 0x30, (uint32_t)pv->mod.writeIndex());
                fw.c.wr32(M + 0x40, pv->mod.syncOn() ? 1 : 0);   // an OSC SYNC note leaves its flag set for the next
                // an LFO's clock fields, which only a note value sets, and with OSC 1 off the tempo its block had
                const XtLfo* lfos[5] = { &pv->lfo, &pv->osc2.lfo, &pv->cs[0].lfo, &pv->cs[1].lfo, &pv->tlfo };
                for (uint32_t l = 1; l <= 5; ++l) {
                    const XtLfo& q = *lfos[l - 1];
                    const uint32_t O = Firmware::lfoAt(n, l);
                    wrf(O + 0x84, q.clockStep()); wrf(O + 0x88, q.clockTarget()); wrf(O + 0x8c, q.clockTime());
                    fw.c.wr32(O + 0x90, (uint32_t)q.clockWrap()); fw.c.wr32(O + 0x94, (uint32_t)q.clockCount());
                }
                if (!ctl.on) wrf(n.BL + 0x494, pv->blockTempo);
                for (int o = 0; o < 2; ++o) {
                    if ((o ? ctl.osc2.oscType : ctl.oscType) != 0) continue;
                    const XtAnalogOsc& a = osc(*pv, o).ana;
                    const uint32_t V = Firmware::voice(n, o);
                    for (const VField& f : anaFields(a)) { uint32_t u; std::memcpy(&u, &f.v, 4); fw.c.wr32(V + f.off, u); }
                    fw.c.wr8(V + 0x248, (uint8_t)a.noiseHold());
                    fw.c.wr32(V + 0x1dc, a.flags()); fw.c.wr32(V + 0x244, a.noiseState());
                    fw.c.wr32(V + 0x1e8, (uint32_t)a.hqFalling()); fw.c.wr32(V + 0x278, (uint32_t)a.fbWrite());
                    // the two limits the analog note-ons read, 0 after an analog note.  A LITE note with Vari on leaves its first sample and its
                    // backward loop's start there (its player's +0x138, +0x140), which an analog note on that voice then takes on the XT
                    fw.c.wr32(V + 0x20c, 0); fw.c.wr32(V + 0x214, 0);
                }
            };
        };
        // the voice XtSynth's next note will take
        auto nextVoice = [&]() -> XtSynth::Voice& {
            for (XtSynth::Voice& w : syn.voice) if (!w.active) return w;
            return syn.voice[0];
        };
        auto keyOf = [&](int k) { return std::clamp(k + 12 * ctl.octave, 0, 127); };
        auto voiceOfNewest = [&] {
            int best = -1;
            for (int i = 0; i < XtSynth::kVoices; ++i)
                if (syn.voice[i].active && (best < 0 || syn.voice[i].age > syn.voice[best].age)) best = i;
            return best;
        };

        auto compareNote = [&](FwNote& n, int blk) {
            XtSynth::Voice& v = syn.voice[n.vi];
            for (int e = 1; e <= 13; ++e) {
                if (e >= 9 && e <= 12 && (!v.cs[(e - 9) / 2].on || !fw.c.rd32(Firmware::envAt(n, (uint32_t)e) + 0x68))) continue;   // a COSM type that does not use it: the firmware's stands still
                if (e <= 8 && !osc(v, (e - 1) / 4).on) continue;
                XtEnvelope& pe = env(v, e);
                const float a = fw.f32(Firmware::envAt(n, (uint32_t)e) + 0x4c), b = pe.level();
                ++rep.checks;
                if (!bits(a, b)) rep.bad("env " + std::to_string(e) + " output", "fw %.9g port %.9g", a, b, blk);
                const int sa = (int)fw.c.rd32(Firmware::envAt(n, (uint32_t)e)), sb = pe.stage();
                if (sa != sb) rep.bad("env " + std::to_string(e) + " stage", "fw %.0f port %.0f", sa, sb, blk);
            }
            // The Matrix's sums on the note's object, once a pass has read them: a slot is formed by its getter, and
            // its word's dirty bit (M + 0x84) says it has not been since a term moved.  A slot no conversion reads stays so.
            {
                static const int kWordOf[11][2] = { { 1, 4 }, { 5, 8 }, { 9, 0xb }, { 0xc, 0x17 }, { 0x18, 0x1c }, { 0x1d, 0x28 }, { 0x29, 0x2d }, { 0, -1 }, { 0x2e, 0x38 }, { 0x39, 0x43 }, { 0x44, 0x4e } };
                const uint32_t M = fw.c.rd32(n.IN + 0x68);
                bool ran = false;                // the terms are formed by the first getter with a slot in use, and until then all rest at 0
                for (uint32_t i = 0; i < 8; ++i) ran = ran || fw.c.rd16(M + 0x9a + 2 * i) != 0;
                for (uint32_t w = 0; ran && w < 11; ++w) {
                    const uint32_t dirty = fw.c.rd16(M + 0x84 + 2 * w);
                    for (int d = kWordOf[w][0]; d <= kWordOf[w][1]; ++d) {
                        if ((dirty >> (d - kWordOf[w][0])) & 1) continue;
                        const int a = (int16_t)fw.c.rd16(M + 0xca + 2 * (uint32_t)(d - 1)), b = v.mx[d];
                        ++rep.checks;
                        if (a != b) rep.bad("matrix sum " + std::to_string(d), "fw %.0f port %.0f", a, b, blk);
                    }
                }
                static const uint32_t kBeam[4] = { 0x180, 0x181, 0x182, 0x183 };
                for (int i = 0; i < 4; ++i) {
                    const int a = (uint8_t)fw.c.rd8(M + kBeam[i]), b = v.beam[i];
                    ++rep.checks;
                    if (a != b) rep.bad("beam byte " + std::to_string(i), "fw %.0f port %.0f", a, b, blk);
                }
            }
            const float tl = fw.f32(Firmware::lfoAt(n, 5) + 0x38), tlp = v.tlfo.out();
            ++rep.checks;
            if (!bits(tl, tlp)) rep.bad("tva lfo output", "fw %.9g port %.9g", tl, tlp, blk);
            for (int k = 0; k < 2; ++k) {
                if (!v.cs[k].on || !fw.c.rd32(Firmware::lfoAt(n, 3 + (uint32_t)k) + 0x80)) continue;
                const float la = fw.f32(Firmware::lfoAt(n, 3 + (uint32_t)k) + 0x38), lb = v.cs[k].lfo.out();
                ++rep.checks;
                if (!bits(la, lb)) rep.bad("cosm " + std::to_string(k + 1) + " lfo output", "fw %.9g port %.9g", la, lb, blk);
            }
            // the levels a new block reads, by what the binding put in each slot
            for (int k = 0; k < 2; ++k) {
                const XtSynth::Voice::CosmVoice& c = v.cs[k];
                if (!c.on) continue;
                const uint32_t O = n.TN + (k ? 0x14ec : 0x1388), a = Firmware::envAt(n, 9 + 2 * (uint32_t)k), b = Firmware::envAt(n, 10 + 2 * (uint32_t)k), l = Firmware::lfoAt(n, 3 + (uint32_t)k);
                for (uint32_t s = 0; s < 4; ++s) {
                    const uint32_t bound = fw.c.rd32(O + 0x150 + 4 * s);
                    if (bound != a && bound != b && bound != l) continue;
                    const float x = fw.f32(O + 0x104 + 4 * s), y = bound == a ? c.heldA : bound == b ? c.heldB : c.heldLfo;
                    ++rep.checks;
                    if (!bits(x, y)) rep.bad("cosm " + std::to_string(k + 1) + " held level", "fw %.9g port %.9g", x, y, blk);
                }
            }
            for (int o = 0; o < 2; ++o) {
                XtSynth::OscVoice& s = osc(v, o);
                if (!s.on) continue;
                const uint32_t V = Firmware::voice(n, o);
                const OscC& oc = o ? static_cast<const OscC&>(syn.zc(v).osc2) : static_cast<const OscC&>(syn.zc(v));
                const std::string os = "osc " + std::to_string(o + 1) + " ";
                const float la = fw.f32(Firmware::lfoAt(n, (uint32_t)o + 1) + 0x38), lb = s.lfo.out();
                ++rep.checks;
                if (!bits(la, lb)) rep.bad(os + "lfo output", "fw %.9g port %.9g", la, lb, blk);
                const VField vs[] = { { "cents", 8, syn.noteCents(v, s, oc, v.note) }, { "source cents", 0x68, syn.noteCents(v, s, oc, v.srcKey) }, { "pitch knob", 0xc, syn.pitchKnob(v, s, oc) } };
                for (const VField& f : vs) {
                    ++rep.checks;
                    const float x = fw.f32(V + f.off);
                    if (!bits(x, f.v)) rep.bad(os + f.name, "fw %.9g port %.9g", x, f.v, blk);
                }
                ++rep.checks;
                const uint32_t RP = V + 0x880;
                const float rx = fw.c.rd32(RP + 4) ? fw.f32(RP + 0x3c) : 0.0f, ry = s.porta.value();     // as 0x894f4c00 reads it
                if (!bits(rx, ry)) rep.bad(os + "ramp", "fw %.9g port %.9g", rx, ry, blk);
                if (s.analog) {
                    for (const VField& f : anaFields(s.ana)) {
                        ++rep.checks;
                        const float x = fw.f32(V + f.off);
                        if (!bits(x, f.v)) rep.bad(os + f.name, "fw %.9g port %.9g", x, f.v, blk);
                    }
                } else {
                    const VField fs[] = { { "pitch", 0x144, s.curA }, { "formant", 0x148, s.curF }, { "energy", 0x14c, s.curG }, { "walk", 0x150, s.curWalk }, { "pcm gain", 0x154, s.curGain } };
                    for (const VField& f : fs) {
                        if (s.lite && f.off == 0x14c) continue;   // kind 1 has no Energy (0x8950bbb0 leaves +0x14c alone)
                        ++rep.checks;
                        const float x = fw.f32(V + f.off);
                        if (!bits(x, f.v)) rep.bad(os + f.name, "fw %.9g port %.9g", x, f.v, blk);
                    }
                    // The beat (player at voice +0xd4)
                    const XtBeat& bt = s.linear ? s.lin.beat() : s.lite ? s.xfg.beat() : s.wave->type == 1 ? s.solo.beat() : s.mb.beat();
                    const uint32_t PL = V + 0xd4;
                    const int32_t posNow = s.linear ? (int32_t)fw.c.rd32(PL + 0x38) : s.lite ? s.xfg.position() : s.wave->type == 1 ? s.solo.position() : s.mb.position();   // Vari off leaves +0x38 as the SOLO setup did
                    if (s.lite && !s.linear) {       // the LITE player's own state
                        const struct { const char* name; uint32_t off; float v; } lf[] = { { "lite walk", 0x110, s.xfg.walkPosition() }, { "lite fraction a", 0x108, s.xfg.fractionA() },
                            { "lite fraction b", 0x10c, s.xfg.fractionB() }, { "lite filter", 0x100, s.xfg.filterL() } };
                        for (const auto& f : lf) { ++rep.checks; if (!bits(fw.f32(PL + f.off), f.v)) rep.bad(os + f.name, "fw %.9g port %.9g", fw.f32(PL + f.off), f.v, blk); }
                        rep.checks += 2;
                        if ((int32_t)fw.c.rd32(PL + 0xf4) != s.xfg.headA()) rep.bad(os + "lite head a", "fw %.0f port %.0f", (int32_t)fw.c.rd32(PL + 0xf4), s.xfg.headA(), blk);
                        if ((int32_t)fw.c.rd32(PL + 0xf8) != s.xfg.headB()) rep.bad(os + "lite head b", "fw %.0f port %.0f", (int32_t)fw.c.rd32(PL + 0xf8), s.xfg.headB(), blk);
                    }
                    if (s.linear) {                  // its own state
                        rep.checks += 4;
                        if ((int32_t)fw.c.rd32(PL + 0xec) != s.lin.position()) rep.bad(os + "linear position", "fw %.0f port %.0f", (int32_t)fw.c.rd32(PL + 0xec), s.lin.position(), blk);
                        if ((int32_t)fw.c.rd32(PL + 0x128) != s.lin.turningPos()) rep.bad(os + "linear turning", "fw %.0f port %.0f", (int32_t)fw.c.rd32(PL + 0x128), s.lin.turningPos(), blk);
                        if (!bits(fw.f32(PL + 0xe8), s.lin.fraction())) rep.bad(os + "linear fraction", "fw %.9g port %.9g", fw.f32(PL + 0xe8), s.lin.fraction(), blk);
                        if (!bits(fw.f32(PL + 0x124), s.lin.fade())) rep.bad(os + "linear fade", "fw %.9g port %.9g", fw.f32(PL + 0x124), s.lin.fade(), blk);
                    }
                    const struct { const char* name; uint32_t off; int32_t v; } ints[] = { { "position", 0x38, posNow }, { "beat budget", 0x54, bt.budget }, { "beat sync", 0x58, bt.syncPos }, { "beat trip", 0x5c, bt.tripPos },
                        { "beat walk", 0x60, bt.walkPos }, { "beat position", 0x64, bt.beatPos }, { "beat step", 0x50, bt.step } };
                    for (const auto& f : ints) {
                        ++rep.checks;
                        const int32_t x = (int32_t)fw.c.rd32(PL + f.off);
                        if (x != f.v) rep.bad(os + f.name, "fw %.0f port %.0f", x, f.v, blk);
                    }
                    const VField fl[] = { { "beat factor", 0x8c, bt.factor }, { "beat integ", 0x90, bt.integ } };
                    for (const VField& f : fl) {
                        ++rep.checks;
                        const float x = fw.f32(PL + f.off);
                        if (!bits(x, f.v)) rep.bad(os + f.name, "fw %.9g port %.9g", x, f.v, blk);
                    }
                    rep.checks += 3;
                    if ((int8_t)fw.c.rd8(PL + 0x40) != bt.clk) rep.bad(os + "beat clocks", "fw %.0f port %.0f", (int8_t)fw.c.rd8(PL + 0x40), bt.clk, blk);
                    if ((int8_t)fw.c.rd8(PL + 0x41) != bt.trip) rep.bad(os + "beat steps", "fw %.0f port %.0f", (int8_t)fw.c.rd8(PL + 0x41), bt.trip, blk);
                    if ((fw.c.rd32(V + 0x9b0) != 0) != s.chase) rep.bad(os + "chase", "fw %.0f port %.0f", fw.c.rd32(V + 0x9b0), s.chase, blk);
                }
            }
        };
        auto compareAll = [&](int blk) { for (FwNote& n : fn) if (n.live && !n.ended && n.vi >= 0 && syn.voice[n.vi].active) compareNote(n, blk); };

        auto startNote = [&](int i, int blk) {
            fw.frames = syn.hostFrames;
            if (!fw.keyOn(raw[i], vel[i])) return false;
            // the part's note-on (0x894dbcb0)
            const bool heldKey = fw.heldBefore();
            if (fw.partMono() && heldKey && fw.partLegato()) {
                ++legatos;
                for (FwNote& n : fn)
                    if (n.live && !n.released && !fw.legato(n, keyOf(raw[i]))) return false;
            } else {
                if (fw.partMono())
                    for (FwNote& n : fn)
                        if (n.live && !n.ended) { ++truncates; if (!fw.truncate(n)) return false; }   // released ones too (0x894d4a80)
                if (fn[i].live && !fw.stop(fn[i])) return false;
                // the allocator's earlier note (0x894d4c04): of the part and zone, still on its list; the first whose key is down, else the last
                const int zone = fw.zoneOf(keyOf(raw[i]));
                const FwNote* prev = nullptr;
                for (const FwNote* o : { &fn[1 - i] })
                    if (o->live && !o->off && !o->ended && fw.c.rd8(o->BL + 0x6c6) == zone) prev = o;
                if (!fw.start(fn[i], i, keyOf(raw[i]), zone, carryFrom(nextVoice()), prev)) return false;
                ++notes;
                // a LITE player's +4, which LEGATO's copy reads, is what its tone slot last held, which XtSynth's voices don't keep
                if (prev && prev->vi >= 0)
                    for (int o = 0; o < 2; ++o) osc(syn.voice[prev->vi], o).staleEnd = (int8_t)fw.c.rd8(Firmware::voice(*prev, o) + 0xd4 + 4);
                for (int o = 0; o < 2; ++o) {
                    const uint32_t V = Firmware::voice(fn[i], o);
                    // LEGATO: 0x894f21c0's flag, and the copy unless the earlier player had reached its wave's end (0x894f2d24)
                    if (prev && fw.c.rd32(V + 0x87c)) ++((int8_t)fw.c.rd8(Firmware::voice(*prev, o) + 0xd4 + 4) == 1 ? endSkips : playerTakes);
                    // STEP and EVENT: a band player whose block's 0x80 is clear starts from an event (0x894f3150)
                    const uint32_t mode = fw.c.rd32(V + 0x80);
                    if (fw.c.rd32(V) == 2 && (mode == 3 || mode == 5) && !(fw.c.rd8(V + 0xb4) & 0x80)) ++eventNotes;
                }
            }
            syn.noteOn(raw[i], (float)vel[i] / 127.0f);
            if (fn[i].live && fn[i].vi < 0) fn[i].vi = voiceOfNewest();
            if (fn[i].live && fn[i].vi >= 0) {                   // the zone it plays, the block's +0x6c6
                ++rep.checks;
                const int a = fw.c.rd8(fn[i].BL + 0x6c6), b = syn.voice[fn[i].vi].zone;
                if (a != b) rep.bad("zone", "fw %.0f port %.0f", a, b, blk);
                if (a) { ++zoneNotes; zonesSeen |= 1L << a; }
                for (int o = 0; o < 2; ++o) {                 // STEP's counts, the zone's and OSC's (0x89cf7210)
                    ++rep.checks;
                    const int x = fw.c.rd16(0x89cf7210 + 4 * (uint32_t)a + 2 * (uint32_t)o), y = syn.stepCount[a][o];
                    if (x != y) rep.bad("step count", "fw %.0f port %.0f", x, y, blk);
                }   // Playback Mode LEGATO took an earlier player (0x894f21c0's flag)
            }
            for (int o = 0; o < 3; ++o)
                if (fn[i].live)
                    if (const uint32_t f = fw.c.rd32(Firmware::lfoAt(fn[i], kFreeLfo[o]) + 0x7c)) fwFree[fw.c.rd8(fn[i].BL + 0x6c6) & 15][o] = f;
            compareAll(blk);
            return true;
        };
        // The part's note-off (0x894dbea0)
        auto stopNote = [&](int i, int blk) {
            fw.frames = syn.hostFrames;
            int back = -1;
            if (!fw.keyOff(raw[i], relVel[i], back)) return false;
            if (fw.partMono() && back >= 0)
                for (FwNote& n : fn)
                    if (n.live && !n.released) { ++legatos; if (!fw.legato(n, keyOf(back))) return false; }
            for (FwNote& n : fn)
                if (n.live && !n.released && n.raw == raw[i] && !fw.keyRelease(n)) return false;
            syn.noteOff(raw[i], (float)relVel[i] / 127.0f);
            compareAll(blk);
            return true;
        };
        // Knobs moved, or the bender: the part's tables and each note's update on the firmware's
        // side, setControls or pitchBend on XtSynth's
        auto moveLive = [&](const Ctl& n, int bentFrom, int blk, int ccAt = -1, int ccTo = 0) {
            const bool ccMoved = ccAt >= 0 && cc[ccAt] != ccTo;
            const bool panelMoved = !(n == ctl), moved = panelMoved || fw.bender != bentFrom || ccMoved;   // nothing moved, no update on either side
            ctl = n;
            if (ccAt >= 0) { cc[ccAt] = ccTo; fw.setCc(kCcNum[ccAt], ccTo); }
            fw.par = patchOf(ctl);
            fw.frames = syn.hostFrames;
            if (!fw.updatePatch()) return false;
            // a controller alone ends at the pad's handler before its update, as XtSynth::controller does: CC 91 or 93 can be a
            // Time Trip Assign too (the knobs' branch below moves neither side's pad)
            if (ccMoved && !panelMoved && !fw.padCc(kCcNum[ccAt], ccTo)) return false;
            for (FwNote& fnote : fn)
                if (moved && fnote.live && !fnote.ended && !fw.liveUpdate(fnote)) return false;
            if (panelMoved) {                        // the bender and a controller with the knobs, in the one update the firmware's gets
                XtToneProbe::bend(syn, fw.bender);
                XtToneProbe::ccs(syn, cc, kCcNum, kCcCount);
                syn.setControls(ctl);
            } else if (ccMoved) {
                XtToneProbe::bend(syn, fw.bender);
                syn.controller(kCcNum[ccAt], ccTo);
            } else syn.pitchBend(fw.bender);
            compareAll(blk);
            return true;
        };

        // Hold or Sostenuto moved, each note's update after it, as any controller's
        auto pedalMove = [&](int number, int value, int blk) {
            fw.frames = syn.hostFrames;
            fw.setCc(number, value);                 // the channel takes it first, as any controller's (0x89475370)
            if (!fw.pedal(number, value, fn, 2)) return false;
            if (!fw.padCc(number, value)) return false;
            for (FwNote& n : fn)
                if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
            syn.controller(number, value);
            compareAll(blk);
            return true;
        };
        // A source of the Matrix moved: a controller's, or a pressure, each its update.
        auto sourceMove = [&](int blk) {
            fw.frames = syn.hostFrames;
            const int which = r.range(0, 7), source = ctl.matrix[which].source, x = r.range(0, 127);
            int number = source >= 1 && source < 96 ? source : -1;
            if (source >= 98 && source <= 106 && source != 102) {
                static const int kAssignOf[9] = { 6, 7, 6, 7, -1, 4, 5, 8, 9 };   // the assign each reads: PAD-X and -Y twice, BEAM-L and -R, KNOB1 and 2
                number = ctl.assign[kAssignOf[source - 98]];
                if (number == 0) number = 2;
            }
            if (source == 97 && r.one(2)) {
                const int key = r.one(2) ? raw[0] : std::clamp(raw[0] + 12 * ctl.octave, 0, 127);
                fw.setPoly(key, x);
                for (FwNote& n : fn)
                    if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
                syn.polyPressure(key, x);
            } else if (source == 97) {
                fw.setPressure(x);
                for (FwNote& n : fn)
                    if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
                syn.channelPressure(x);
            } else {
                if (number < 0) number = r.range(1, 95);
                if (number == 64 || number == 66 || number == 7 || number == 10 || number == 11 || number == 1) number = 2;   // those have their own draws
                for (int i = 0; i < kCcCount; ++i) if (kCcNum[i] == number) cc[i] = x;                                       // CC 91 and 93, whose own a later move would restore
                fw.setCc(number, x);
                if (!fw.padCc(number, x)) return false;
                for (FwNote& n : fn)
                    if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
                syn.controller(number, x);
            }
            compareAll(blk);
            return true;
        };
        // A mode message, at the values each takes (0x894d7a10, 0x894d7a70, 0x894d7ae0, 0x894d7b40): no update.
        // All Sound Off truncates, the others are every key let go.
        auto modeMessage = [&](int number, int value, int blk) {
            fw.frames = syn.hostFrames;
            if (number == 126 ? value <= 16 : value == 0) {
                if (!fw.keysCleared()) return false;
                for (FwNote& n : fn) {
                    if (!n.live || n.released) continue;
                    if (number == 120) { ++truncates; if (!fw.truncate(n)) return false; }
                    else if (!fw.keyRelease(n)) return false;
                }
            }
            syn.controller(number, value);
            compareAll(blk);
            return true;
        };
        // Reset All Controllers (0x894d7940): both pedals up, the update; at value 0 the channel's
        // controllers first (0x89475200, its object two bytes before C).
        auto resetAll = [&](int value, int blk) {
            fw.frames = syn.hostFrames;
            if (value == 0) {
                if (!fw.c.call(0x89475200, { kCtl - 2 }, kSp)) return false;
                fw.bender = 8192;
                cc[1] = 127; cc[3] = 0;
            }
            if (!fw.pedal(64, 0, fn, 2) || !fw.pedal(66, 0, fn, 2)) return false;
            for (FwNote& n : fn)
                if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
            syn.controller(121, value);
            compareAll(blk);
            return true;
        };

        // The Time Trip pad on both sides.  A step goes straight to the voices; touching and letting go
        // reach the tones at their next update.  A finger round a circle, or back and forth, or jumping
        // across the pad; then perhaps held still, where a walk's budget runs out (0x8950c280); let go
        // by both controllers or one, and perhaps touched again, within the fade the seek finds
        // Time Trip on (0x8950f900).  A first message may have one at 0, which is no touch.
        const auto padFields = [&](int blk) {
            ++rep.checks;
            if (fw.padOn() != syn.pad.on()) rep.bad("pad on", "fw %.0f port %.0f", fw.padOn(), syn.pad.on(), blk);
            if (fw.padRadius() != syn.pad.radius()) rep.bad("pad radius", "fw %.0f port %.0f", fw.padRadius(), syn.pad.radius(), blk);
        };
        const auto padTo = [&](int x, int y, int blk) {
            const bool was = syn.pad.on();
            if (!fw.pad(x, y)) return false;
            syn.timeTrip(x, y);
            padFields(blk);
            if (was != syn.pad.on()) {
                fw.frames = syn.hostFrames;
                for (FwNote& fnote : fn)
                    if (fnote.live && !fnote.ended && !fw.liveUpdate(fnote)) return false;
            }
            compareAll(blk);
            return true;
        };
        // One message to the pad by a controller, as MIDI's and the XT's own reach it (0x894d6f10): the channel takes it, then
        // Hold's or Sostenuto's handler if it is theirs, then the pad, then the update each controller queues
        const auto padMessage = [&](int number, int value, int blk) {
            fw.frames = syn.hostFrames;
            for (int i = 0; i < kCcCount; ++i) if (kCcNum[i] == number) cc[i] = value;                                           // CC 91 and 93, as in sourceMove
            fw.setCc(number, value);
            if ((number == 64 || number == 66) && !fw.pedal(number, value, fn, 2)) return false;
            if (!fw.padCc(number, value)) return false;
            for (FwNote& fnote : fn)
                if (fnote.live && !fnote.ended && !fw.liveUpdate(fnote)) return false;
            syn.controller(number, value);
            padFields(blk);
            compareAll(blk);
            return true;
        };
        const bool tripCase = sweepOn || farCase || backCase || padSource || xyCase || (!sweep && !twoInTick && r.one(3));
        // By its controllers in half of them, on the numbers the case began with, so that moving an assign leaves them behind.
        // A controller sends what changed, so each is its own message and a block may have only one, in either order.
        const bool sweepAssign = sweep && (reinterpret_cast<const char*>(&ctl.assign[10]) - reinterpret_cast<const char*>(&ctl) == (long)sweepAt
                                           || reinterpret_cast<const char*>(&ctl.assign[11]) - reinterpret_cast<const char*>(&ctl) == (long)sweepAt);
        if (sweepAssign) { ctl.assign[10] = 82; ctl.assign[11] = 83; fw.par = patchOf(ctl); syn.setControls(ctl); if (!fw.updatePatch()) return 1; }
        const bool tripByCc = tripCase && !farCase && !padSource && (sweepAssign || xyCase || (!sweepOn && r.one(2)));
        const int tripNumX = ctl.assign[xyCase ? 6 : 10], tripNumY = ctl.assign[xyCase ? 7 : 11];
        const bool tripYFirst = r.one(2);
        int tripSentX = -1, tripSentY = -1;
        const auto padByCc = [&](int x, int y, int blk) {
            const auto send = [&](int number, int value, int& sent) {
                if (value == sent) return true;
                sent = value;
                return padMessage(number, value, blk);
            };
            return tripYFirst ? send(tripNumY, y, tripSentY) && send(tripNumX, x, tripSentX) : send(tripNumX, x, tripSentX) && send(tripNumY, y, tripSentY);
        };
        const int tripFrom = sweepOn ? 5 : padSource ? r.range(0, 8) : tripCase ? r.range(0, 40) : -1, tripRadius = r.range(10, 63);
        const int tripLen = sweepOn ? liveAt + 5 : farCase || backCase ? r.range(200, 500) : r.one(2) ? r.range(0, 60) : r.range(60, 300), tripMoves = sweepOn ? 8 : backCase ? r.range(40, 180) : r.one(2) ? tripLen : r.range(1, 40);
        const float tripTurn = (backCase || r.one(2) ? 1.0f : -1.0f) * (float)r.range(1, 60) * 0.0125f;   // radians a block; rising is anticlockwise, backward
        const int tripBack = !backCase && r.one(4) ? r.range(3, 20) : 0;
        const int tripJump = !sweepOn && !backCase && r.one(4) ? r.range(1, 2) : 0, tripFirst = sweepOn ? 0 : r.range(0, 2), tripLift = sweepOn || farCase || backCase ? 0 : r.range(0, 2), tripAgain = !sweepOn && !farCase && !backCase && r.one(2) ? r.range(1, 40) : -1;
        float tripAngle = (float)r.range(0, 628) * 0.01f;

        for (FwNote& n : fn) { n.vi = -1; n.live = n.released = n.ended = false; }
        fw.cosmBuf = cosmBase;
        bool keyDown[2] = {};
        // The mix's first words, which follow OSC 2's buffer: between the cases XtSynth's tails
        // go on rendering and the firmware's stop, so the note starts from the port's.
        for (uint32_t i = 0; i < 3; ++i) { uint32_t u; std::memcpy(&u, &syn.mixPast[i], 4); fw.c.wr32(kBufMix + 4 * i, u); }
        // Hold in a third of the cases and Sostenuto in a fifth, each down before the keys' release or
        // after, and let go, or not.  The values on both sides of the middle.  A mode message in a few.
        const auto pedalBlocks = [&](int chance, int (&at)[3], int (&val)[3]) {
            at[0] = at[1] = at[2] = -1;
            if (sweep || twoInTick || !r.one(chance)) return;
            at[0] = r.one(4) ? -2 : r.range(0, 45);            // −2: down before the first note
            at[2] = r.one(4) ? -1 : std::max(at[0], 0) + r.range(1, 70);
            at[1] = r.one(2) ? std::max(at[0], 0) + r.range(1, 40) : -1;    // a move that stays down
            if (at[2] >= 0 && at[1] >= at[2]) at[1] = -1;
            val[0] = r.one(2) ? 100 : r.range(64, 127);
            val[1] = r.one(2) ? 64 : r.range(64, 127);
            val[2] = r.one(2) ? 63 : r.range(0, 63);
        };
        int holdAt[3], holdVal[3] = {}, sostAt[3], sostVal[3] = {};
        pedalBlocks(3, holdAt, holdVal);
        if (holdSource || playHeld) {
            holdAt[0] = -2; holdVal[0] = 127;
            holdAt[1] = -1;
            holdAt[2] = (playHeld ? secondAt : offAt[0]) + r.range(1, 10); holdVal[2] = 0;
        }
        pedalBlocks(5, sostAt, sostVal);
        const int modeAt = !sweep && !twoInTick && r.one(8) ? r.range(0, 60) : -1, modeCc = modeAt < 0 ? 0 : r.one(3) ? 120 : r.one(2) ? 121 : 123 + r.range(0, 4);
        const int modeVal = r.one(4) ? r.range(0, 20) : 0;
        const int srcAt[2] = { sweep || twoInTick ? -1 : r.one(2) ? r.range(1, 60) : -1, sweep || twoInTick ? -1 : r.one(2) ? r.range(1, 60) : -1 };
        if (holdAt[0] == -2 && !pedalMove(64, holdVal[0], -1)) return 1;
        if (sostAt[0] == -2 && !pedalMove(66, sostVal[0], -1)) return 1;
        if (!startNote(0, -1)) return 1;
        keyDown[0] = true;
        if (dumpWhich && (dumpWhich[0] == 'a' || std::atoi(dumpWhich) == g))
            std::printf("case %d kind %d keys %d %d vel %d %d wave %d: osc1 on %d wave %d, osc2 on %d wave %d; cosm %d %d, %d %d; second at %d, off at %d %d, live at %d; mod %d type %d level %d a %d r %d, ch %d; wave 2 %d ch %d; clock %d tempo %d, host %.2f; sync %d %d, trip %s from %d len %d\n", g, kind, raw[0], raw[1], vel[0], vel[1], wv ? wv->type : 0, ctl.on, ctl.ana.wave,
                        ctl.osc2.on, ctl.osc2.ana.wave, ctl.cosm[0].on, ctl.cosm[0].type, ctl.cosm[1].on, ctl.cosm[1].type, secondAt, offAt[0], offAt[1], liveAt, ctl.mod.on, ctl.mod.type, ctl.mod.level, ctl.mod.attack, ctl.mod.release, wv ? wv->channels : 0,
                        wv2 ? wv2->type : 0, wv2 ? wv2->channels : 0, ctl.clockSource, ctl.patchTempo, hostBpm, ctl.tempoSync, ctl.osc2.tempoSync, tripCase ? "on" : "off", tripFrom, tripLen);
        bool over = false, leftOff = false;
        if (dumpWhich && (dumpWhich[0] == 'a' || std::atoi(dumpWhich) == g))
        {
            for (const OscC* o : { static_cast<const OscC*>(&ctl), static_cast<const OscC*>(&ctl.osc2) })
                std::printf("  osc: on %d vari %d start %d wave %d/%d loop %d/%d time0 %d playback %d events", o->on, o->vari, o->startOffset, o == &ctl ? (wv ? wv->type : 0) : (wv2 ? wv2->type : 0), o == &ctl ? (wv ? wv->channels : 0) : (wv2 ? wv2->channels : 0), o->loop, o->loopMode, o->oscTimeOffset, o->playbackMode),
                [&] { const TestWave* t = o == &ctl ? wv : wv2; if (t) { uint32_t w2 = 0; if (t->enc->vpdt.size() >= 12) std::memcpy(&w2, t->enc->vpdt.data() + 8, 4); std::printf(" %u, list", w2 >> 16); for (uint16_t e : t->enc->vpno) std::printf(" %u", e);
                    if (t->type == 3 || t->type == 5) { const size_t st = t->channels == 2 ? 0x58 : 0x2e; std::printf(", at"); for (uint32_t e = 0; e < (w2 >> 16); ++e) { uint32_t p = 0; const size_t at = 2 * (e * st + 0xe) + 4; if (at + 4 <= t->enc->vpdt.size()) std::memcpy(&p, t->enc->vpdt.data() + at, 4); std::printf(" %u", p); } std::printf(", loop %.4f..%.4f mode %d", o->loopStart, o->loopEnd, o->loopMode); } } std::printf("\n"); }(),
                std::printf("  osc: type %d coarse %d fine %d pitch %d kf %d random %d key %d robot %d; patch coarse %d fine %d octave %d porta %d; time %d offset %d kf %d\n", o->oscType, o->oscCoarse, o->oscFine, o->oscPitch, o->pitchKF, o->randomPitch, o->rootKey, o->robot, ctl.patchCoarse, ctl.patchFine, ctl.octave, ctl.portamento, o->oscTime, o->oscTimeOffset, o->oscTimeKF);
            std::printf("  assign"); for (int i = 0; i < 12; ++i) std::printf(" %d", ctl.assign[i]);
            std::printf("; system"); for (uint32_t i = 0; i < 10; ++i) std::printf(" %d", (int8_t)fw.c.rd8(0x89d276b4 + 0x844 + i));
            std::printf("; matrix"); for (int i = 0; i < 8; ++i) std::printf(" [%d: %d %d, %d %d]", ctl.matrix[i].source, ctl.matrix[i].dest[0], ctl.matrix[i].sens[0], ctl.matrix[i].dest[1], ctl.matrix[i].sens[1]);
            std::printf("\n");
            std::printf("  hold at %d %d %d (%d %d %d), sostenuto at %d %d %d (%d %d %d), mode message %d at %d\n", holdAt[0], holdAt[1], holdAt[2], holdVal[0], holdVal[1], holdVal[2],
                        sostAt[0], sostAt[1], sostAt[2], sostVal[0], sostVal[1], sostVal[2], modeCc, modeAt);
        }
        for (int blk = 0; blk < nBlocks && !over; ++blk) {
            if (blk == secondAt) { if (!startNote(1, blk)) return 1; keyDown[1] = true; }
            if (chaseEnd && !keyDown[1] && fn[0].live && !fn[0].ended && fn[0].vi >= 0 && syn.tickCount >= 441) {
                const XtSynth::Voice& v0 = syn.voice[fn[0].vi];
                if (v0.env.done() && v0.cur[0] <= 0x1.0000fep-16f && v0.cur[1] <= 0x1.0000fep-16f) { if (!startNote(1, blk)) return 1; keyDown[1] = true; }
            }
            for (int i = 0; i < 3; ++i)
                if (blk == holdAt[i] && !pedalMove(64, holdVal[i], blk)) return 1;
            for (int i = 0; i < 3; ++i)
                if (blk == sostAt[i] && !pedalMove(66, sostVal[i], blk)) return 1;
            for (int i = 0; i < 2; ++i)
                if (blk == offAt[i] && keyDown[i]) { if (!stopNote(i, blk)) return 1; keyDown[i] = false; }
            for (int i = 0; i < 2; ++i)
                if (blk == srcAt[i] && !sourceMove(blk)) return 1;
            if (blk == modeAt && !(modeCc == 121 ? resetAll(modeVal, blk) : modeMessage(modeCc, modeVal, blk))) return 1;
            if (blk == liveAt && twoInTick) {            // no tick between these
                Ctl n = ctl;
                for (Ctl::Cosm& k : n.cosm) k.envA.sustain = k.envB.sustain = 30;
                if (!moveLive(n, fw.bender, blk)) return 1;
                const int second = (g - tickFrom) / 16;
                if (second == 0) {                       // a legato
                    if (!startNote(1, blk)) return 1;
                    keyDown[1] = true;
                } else if (second == 1) {                // the knob again
                    for (Ctl::Cosm& k : n.cosm) k.envA.sustain = k.envB.sustain = 60;
                    if (!moveLive(n, fw.bender, blk)) return 1;
                } else {                                 // the bender
                    const int from = fw.bender;
                    fw.bender = (from + 4000) % 16384;
                    if (!moveLive(ctl, from, blk)) return 1;
                }
            } else if (blk == liveAt && sweep && sweepAt == kModeOnly) {
                if (!modeMessage(123, 0, blk)) return 1;
            } else if (blk == liveAt && sweepDest) {
                fw.frames = syn.hostFrames;
                fw.setCc(kDestCc, 120);
                if (!fw.padCc(kDestCc, 120)) return 1;
                for (FwNote& n : fn)
                    if (n.live && !n.ended && !fw.liveUpdate(n)) return 1;
                syn.controller(kDestCc, 120);
                compareAll(blk);
            } else if (blk == liveAt && sweepSens) {
                Ctl n = ctl;
                n.matrix[0].sens[0] = 60;
                if (!moveLive(n, fw.bender, blk)) return 1;
            } else if (blk == liveAt) {                  // knobs moved, and the bender
                Ctl n = drawControls(r, g, wv != nullptr, wv2 != nullptr);
                if (followCase) {
                    const Ctl d = n;
                    n = ctl;
                    n.lfoRate = (d.lfoRate & 15) + 128; n.osc2.lfoRate = (d.osc2.lfoRate & 15) + 128; n.tvaLfoRate = (d.tvaLfoRate & 15) + 128;
                    for (int k = 0; k < 2; ++k) n.cosm[k].lfoRate = (d.cosm[k].lfoRate & 15) + 128;
                }
                else if (sweepOn) {                      // the field itself: a switch thrown, or another tempo
                    n = ctl;
                    char* p = reinterpret_cast<char*>(&n) + sweepAt;
                    if (p == reinterpret_cast<char*>(&n.patchTempo)) n.patchTempo = ctl.patchTempo == 1200 ? 2400 : 1200;
                    else if (p == reinterpret_cast<char*>(&n.startOffset)) n.startOffset = ctl.startOffset ? 0 : 3000;
                    else if (p == reinterpret_cast<char*>(&n.osc2.startOffset)) n.osc2.startOffset = ctl.osc2.startOffset ? 0 : 3000;
                    else if (p == reinterpret_cast<char*>(&n.playbackMode)) n.playbackMode = ctl.playbackMode == 3 ? 0 : ctl.playbackMode == 0 ? 1 : 0;
                    else if (p == reinterpret_cast<char*>(&n.osc2.playbackMode)) n.osc2.playbackMode = ctl.osc2.playbackMode == 3 ? 0 : ctl.osc2.playbackMode == 0 ? 1 : 0;
                    else *p = !*p;
                }
                else if (sweep && sweepAt >= kCcOnly) n = ctl;
                else if (sweep || syncAfter || r.one(2)) {   // one word of the panel only: what XtSynth notices has changed
                    static_assert(std::is_trivially_copyable_v<Ctl>);
                    Ctl one = ctl;
                    const size_t w = sweep ? sweepAt & ~(size_t)3 : (size_t)r.range(0, (int)(sizeof(Ctl) / 4) - 1) * 4;
                    for (int tries = 0; tries < 16 && !std::memcmp(reinterpret_cast<const char*>(&ctl) + w, reinterpret_cast<const char*>(&n) + w, 4); ++tries)
                        n = drawControls(r, g, wv != nullptr, wv2 != nullptr);
                    std::memcpy(reinterpret_cast<char*>(&one) + w, reinterpret_cast<const char*>(&n) + w, 4);
                    n = one;
                }
                else if (playCase)                       // the whole panel: Playback Mode drawn again as at the case's start, not left at RETRIGGER
                    for (int k = 0; k < 2; ++k) { const TestWave* t = k ? wv2 : wv; (k ? n.osc2.playbackMode : n.playbackMode) = t && t->type == 1 ? r.range(0, 1) : r.range(0, 3); }
                n.on = ctl.on; n.oscType = ctl.oscType; n.rootKey = ctl.rootKey; n.ana.wave = ctl.ana.wave; n.ana.subOctave = ctl.ana.subOctave; n.octave = ctl.octave;
                n.osc2.on = ctl.osc2.on; n.osc2.oscType = ctl.osc2.oscType; n.osc2.ana.wave = ctl.osc2.ana.wave; n.osc2.ana.subOctave = ctl.osc2.ana.subOctave;
                // Switched during a LITE note, the XT's next update installs the other player on this one's state (0x894f6bf0 → 0x894f2010 by the
                // block's mode as it stands, 0x894f3590; its loop's update, 0x8950bcc0, writes over the linear player's position): the check holds
                // Vari as the note started, there and on the playback modes' LITE waves
                if (liteCase) { n.vari = ctl.vari; n.osc2.vari = ctl.osc2.vari; }
                if (playCase && wv && wv->type == 0) n.vari = ctl.vari;
                if (playCase && wv2 && wv2->type == 0) n.osc2.vari = ctl.osc2.vari;
                n.oscTimeOffset = ctl.oscTimeOffset; n.lfoKeySync = ctl.lfoKeySync; n.osc2.lfoKeySync = ctl.osc2.lfoKeySync; n.tvaLfoKeySync = ctl.tvaLfoKeySync;
                n.structure = ctl.structure;             // latched at note-on, but its own check is xt_analog's
                // switched with the pad in play, a Beat Keep's way back runs one player's seek on the other's state, on the XT too; the sweep's own
                // throw of Vari goes through, with Beat Keep off
                static const Ctl c0;
                const bool sweepVari = sweepOn && (sweepAt == (size_t)(reinterpret_cast<const char*>(&c0.vari) - reinterpret_cast<const char*>(&c0))
                                                   || sweepAt == (size_t)(reinterpret_cast<const char*>(&c0.osc2.vari) - reinterpret_cast<const char*>(&c0)));
                if (tripCase && !sweepVari) { n.vari = ctl.vari; n.osc2.vari = ctl.osc2.vari; }
                // and so wherever else that way back can come: the pad also moves by a controller the case moves for itself as a Time Trip Assign,
                // Hold, Sostenuto, CC 91 or 93, or a source of the Matrix (0x894f213e's seek then ran the SOLO's records on the linear player's
                // state, and the note left memory)
                const auto padByCc = [](const Ctl& c) {
                    for (int a : { c.assign[10], c.assign[11] }) {
                        if (a == 64 || a == 66 || a == 91 || a == 93) return true;
                        for (const auto& m : c.matrix) if (a != 0 && a != 32 && m.source == a) return true;
                    }
                    return false;
                };
                // and on a SOLO wave with LEGATO: thrown before the next note, the XT takes the earlier player into the other kind (Vari is the
                // block's 0x20, not the voice's mode) and reads anywhere; XtSynth takes a player only into its own kind
                if (wv && wv->type == 1 && (ctl.playbackMode == 1 || n.playbackMode == 1)) n.vari = ctl.vari;
                if (wv2 && wv2->type == 1 && (ctl.osc2.playbackMode == 1 || n.osc2.playbackMode == 1)) n.osc2.vari = ctl.osc2.vari;
                if (padByCc(ctl) || padByCc(n)) {
                    if ((ctl.timeTrip || n.timeTrip) && (ctl.beatKeep || n.beatKeep)) n.vari = ctl.vari;
                    if ((ctl.osc2.timeTrip || n.osc2.timeTrip) && (ctl.osc2.beatKeep || n.osc2.beatKeep)) n.osc2.vari = ctl.osc2.vari;
                }
                n.mono = ctl.mono; n.legato = ctl.legato; n.robot = ctl.robot; n.loop = ctl.loop; n.loopMode = ctl.loopMode; n.loopStart = ctl.loopStart; n.loopEnd = ctl.loopEnd;
                n.origTempo = ctl.origTempo; n.osc2.origTempo = ctl.osc2.origTempo;
                n.osc2.rootKey = ctl.osc2.rootKey; n.osc2.oscTimeOffset = ctl.osc2.oscTimeOffset; n.osc2.robot = ctl.osc2.robot; n.osc2.loop = ctl.osc2.loop;
                n.osc2.loopMode = ctl.osc2.loopMode; n.osc2.loopStart = ctl.osc2.loopStart; n.osc2.loopEnd = ctl.osc2.loopEnd;
                for (int k = 0; k < 2; ++k) { n.cosm[k].on = ctl.cosm[k].on; n.cosm[k].type = ctl.cosm[k].type; n.cosm[k].lfoKeySync = ctl.cosm[k].lfoKeySync; n.cosm[k].odType = ctl.cosm[k].odType; n.cosm[k].ampType = ctl.cosm[k].ampType; }
                const int bentFrom = fw.bender;
                if (sweep ? sweepAt == kBenderOnly : r.one(2)) fw.bender = r.range(0, 16383);
                int ccAt = -1, ccTo = 0;
                if (sweep && sweepAt >= kCcOnly && sweepAt != kBenderOnly) { ccAt = (int)(sweepAt - kCcOnly); ccTo = 90; }
                else if (!sweep && r.one(3)) { ccAt = r.range(0, 3); ccTo = r.range(0, 127); }
                if (!moveLive(n, bentFrom, blk, ccAt, ccTo)) return 1;
            }
            if (blk == followAt) {
                const int from = fw.bender;
                fw.bender = (from + 4000) % 16384;
                if (!moveLive(ctl, from, blk)) return 1;
            }
            if (tripCase && blk >= tripFrom) {
                const int at = blk - tripFrom;
                if (at == tripLen || (at < tripLen && at < tripMoves) || (tripAgain >= 0 && at == tripLen + tripAgain)) {
                    int x = 1, y = 64;
                    if (tripJump == 1) x = at % 3 == 2 ? 127 : 1;
                    else if (tripJump == 2) { x = r.range(1, 127); y = r.range(1, 127); }
                    else {
                        tripAngle += tripBack && at % (2 * tripBack) >= tripBack ? -tripTurn : tripTurn;
                        const int radius = padSource ? 5 + (at * 7) % 58 : tripRadius;   // for TRIP-R, in and out
                        x = 64 + (int)std::lround((double)radius * std::cos((double)tripAngle));
                        y = 64 + (int)std::lround((double)radius * std::sin((double)tripAngle));
                    }
                    if (at == tripLen) { if (tripLift != 2) x = 0; if (tripLift != 1) y = 0; }
                    else if (at == 0 && tripFirst == 1) x = 0;
                    else if (at == 0 && tripFirst == 2) y = 0;
                    if (!(tripByCc ? padByCc(x, y, blk) : padTo(x, y, blk))) return 1;
                }
            }
            hostMoves(blk);
            const bool tick = syn.tickCount >= 441;
            fw.frames = syn.hostFrames;
            if (!fwClocks()) return 1;
            if (tick && !fw.tickFree()) return 1;
            // in XtSynth's voice order, so that the last rendered is the same on both sides
            FwNote* order[2] = { &fn[0], &fn[1] };
            if (fn[0].vi >= 0 && fn[1].vi >= 0 && fn[1].vi < fn[0].vi) std::swap(order[0], order[1]);
            fw.clearBus();
            bool leftMemory = false;
            fw.fr11.clear();
            for (FwNote* n : order) {
                if (!n->live || n->ended) continue;
                if (tick && !fw.tick(*n)) return 1;
                if (fw.c.rd32(n->TN + 0xc) != 2) { n->ended = true; continue; }
                if (!fw.render(*n)) { leftMemory = true; break; }
            }
            for (FwNote* n : order)
                for (int o = 0; o < 2; ++o)
                    if (n->live && n->vi >= 0)
                        if (const auto it = fw.fr11.find(Firmware::voice(*n, o) + 0xd4); it != fw.fr11.end()) {
                            float v;
                            std::memcpy(&v, &it->second, 4);
                            osc(syn.voice[n->vi], o).lin.setStaleFr11(v);
                        }
            syn.render(L, R, 32);
            ++blocks;
            ++rep.checks;
            if ((fw.c.rd8(kClockObj + 0x40) != 0) != syn.clockExt) rep.bad("clock source", "fw %.0f port %.0f", fw.c.rd8(kClockObj + 0x40), syn.clockExt, blk);
            if (syn.clockExt) clockFields(syn, blk);
            if (leftMemory) {                            // a player's read outside all memory: the note ends here
                ++wild;
                if (rep.first) std::printf("  case %d block %d: left memory: %s\n", g, blk, fw.c.error().c_str());
                leftOff = true;
                break;
            }
            // A band player's seek that read its records past the vpdt (XtMbPlayer::startAt): the XT reads RAM there, and the port
            // stops where it would spin on its zeros, so from here neither is the XT's own.  As a read outside memory, the case ends here
            {
                bool past = false;
                for (FwNote* n : order)
                    if (n->live && n->vi >= 0)
                        for (int o = 0; o < 2; ++o) { const XtSynth::OscVoice& sv = osc(syn.voice[n->vi], o); past |= sv.on && !sv.analog && !sv.lite && !sv.linear && sv.wave && sv.wave->type != 1 && sv.mb.readPastWave(); }
                if (past) {
                    ++wild;
                    if (rep.first) std::printf("  case %d block %d: a band seek read past its vpdt\n", g, blk);
                    leftOff = true;
                    break;
                }
            }
            for (FwNote* n : order) {
                if (!n->live) continue;
                // a truncated tone is over when its fade is (+0x44), ended at its next update
                const bool fwDone = n->ended || fw.c.rd32(Firmware::tva(*n) + 0x44) != 0, synDone = n->vi < 0 || !syn.voice[n->vi].active;
                if (fwDone != synDone) { rep.bad("voice end", "fw %.0f port %.0f", fwDone, synDone, blk); over = true; break; }
                if (fwDone) {                            // compared once: the next note may take its voice, and it finds no tone here to follow
                    n->ended = true;
                    n->vi = -1;
                    if (!fw.stop(*n)) return 1;
                }
            }
            if (over) break;
            FwNote* last = nullptr;
            for (FwNote* n : order) if (n->live && !n->ended) last = n;
            if (!last) { if (secondAt > blk) continue; break; }
            if (tick) compareAll(blk);
            struct B { const char* name; uint32_t a; const float* p; } bs[] = {
                { "osc 2 buffer", kBuf2, syn.tapOsc2 }, { "osc 1 buffer", kBuf1, syn.tapOsc1 }, { "mix", kBufMix, syn.tapMix }, { "cosm output", kBufMix, syn.tapCosm } };
            for (const B& b : bs) {
                // the COSMs work in place on the mix: a mix's own buffer is theirs with one on
                if (b.p == syn.tapMix && (ctl.cosm[0].on || ctl.cosm[1].on)) continue;
                ++rep.checks;
                for (int i = 0; i < 64; ++i) {
                    const float x = fw.f32(b.a + 4 * (uint32_t)i);
                    if (!bits(x, b.p[i])) {
                        rep.bad(b.name, "fw %.9g port %.9g", x, b.p[i], blk);
                        break;
                    }
                }
            }
            // The TVA, bit for bit; the output, Output Assign MFX's bus, the frame's third
            // pair, likewise.  A truncated tone's target and step, which
            // its fade does not read, are the firmware's alone.
            for (FwNote* n : order) {
                if (!n->live || n->ended) continue;
                const XtSynth::Voice& v = syn.voice[n->vi];
                const uint32_t A = Firmware::tva(*n);
                const VField tv[] = { { "tva level", 0xc, v.tb.level }, { "tva lfo depth", 8, v.tb.lfoDepth }, { "tva pan lfo depth", 0x1c, v.tb.panLfoDepth },
                                      { "tva gain", 0x68, v.cur[0] }, { "tva gain r", 0x6c, v.cur[1] }, { "tva target", 0x60, v.target[0] }, { "tva target r", 0x64, v.target[1] },
                                      { "tva step", 0x58, v.step[0] }, { "tva step r", 0x5c, v.step[1] }, { "tva fade", 0x54, v.damping ? v.damp : 1.0f } };
                for (const VField& f : tv) {
                    if (v.damping && (f.off == 0x60 || f.off == 0x64 || f.off == 0x58 || f.off == 0x5c)) continue;
                    ++rep.checks;
                    const float x = fw.f32(A + f.off);
                    if (!bits(x, f.v)) rep.bad(f.name, "fw %.9g port %.9g", x, f.v, blk);
                }
                // The output bus's eight levels, and the block's that they move toward
                static const char* const kBusName[XtBusCount] = { "main l", "main r", "multi l", "multi r", "reverb", "chorus", "dir l", "dir r" };
                for (int b = 0; b < XtBusCount; ++b) {
                    char name[32];
                    rep.checks += 2;
                    const float lev = fw.f32(A + 0x70 + 4 * (uint32_t)b), to = fw.f32(A + 0x20 + 4 * (uint32_t)b);
                    std::snprintf(name, sizeof name, "tva bus level %s", kBusName[b]);
                    if (!bits(lev, v.bus[b])) rep.bad(name, "fw %.9g port %.9g", lev, v.bus[b], blk);
                    std::snprintf(name, sizeof name, "tva bus target %s", kBusName[b]);
                    if (!bits(to, v.tb.bus[b])) rep.bad(name, "fw %.9g port %.9g", to, v.tb.bus[b], blk);
                }
            }
            // The whole frame, each word of each sample; what the plugin plays, the pairs that reach the outs
            static const char* const kWordName[XtSynth::kFrameWords] = { "output main l", "output main r", "output reverb", "output chorus", "output multi l", "output multi r", "output dir l", "output dir r" };
            for (int w = 0; w < XtSynth::kFrameWords; ++w) {
                ++rep.checks;
                for (int i = 0; i < 32; ++i) {
                    const float a = fw.f32(kBuf + 0x20 * (uint32_t)i + 4 * (uint32_t)w), b = syn.frame[XtSynth::kFrameWords * i + w];
                    if (!bits(a, b)) { rep.bad(kWordName[w], "fw %.9g port %.9g", a, b, blk); break; }
                }
            }
            rep.checks += 2;
            for (int i = 0; i < 32; ++i) {
                const auto word = [&](XtSynth::FrameWord w) { return fw.f32(kBuf + 0x20 * (uint32_t)i + 4 * (uint32_t)w); };
                const float a = (word(XtSynth::FrameMultiL) + word(XtSynth::FrameMainL) + word(XtSynth::FrameDirL)) * 0x1p-34f;
                const float a2 = (word(XtSynth::FrameMultiR) + word(XtSynth::FrameMainR) + word(XtSynth::FrameDirR)) * 0x1p-34f;
                if (!bits(a, L[i]) || !bits(a2, R[i])) { rep.bad("output", "fw %.9g port %.9g", a, L[i], blk); break; }
            }
        }
        if (syn.pad.on()) {                          // the pad up again for the next case
            if (!fw.pad(0, 0)) return 1;
            syn.pad.setX(0);
            int unused = 0;
            syn.pad.setY(0, unused);
        }
        for (int i = 0; i < 2; ++i)
            if (keyDown[i]) {
                int back = -1;
                if (!fw.keyOff(raw[i], relVel[i], back)) return 1;
                syn.noteOff(raw[i], (float)relVel[i] / 127.0f);
            }
        syn.allNotesOff();
        for (FwNote& n : fn) if (n.live && !fw.stop(n)) return 1;
        for (uint32_t a : { 0x894d5fc0u, 0x894d5fd0u }) if (!fw.c.call(a, { kN, 0 }, kSp)) return 1;   // the pedals up, no note left
        // FM's lines outlast a note and a modulator past full scale reads the next tone's, so
        // they must agree after a case.  A note that left memory stopped a block before
        // XtSynth's: a later case reading those has no tolerance to take them in.  So each case
        // starts from the port's.
        if (!leftOff) {
            ++rep.checks;
            for (uint32_t i = 0; i < 2 * XtModShared::kTones * XtModShared::kLine; ++i) {
                const float a = fw.f32(kFmLines + 4 * i), b = syn.modShared.line[i];
                if (!bits(a, b)) { rep.bad("fm lines", "fw %.9g port %.9g", a, b, nBlocks); break; }
            }
        }
        fw.c.write(kFmLines, syn.modShared.line, sizeof(syn.modShared.line));
    }
    if (nZone && !nPlay) zoneLine();
    if (nPlay) std::printf("tone, playback modes: %ld OSCs took an earlier note's player (LEGATO), %ld didn't as it had reached its wave's end; %ld started from an event (STEP, EVENT)\n",
                           playerTakes, endSkips, eventNotes);
    summary(nPlay ? "tone, playback modes" : nZone ? "tone, zones" : nLite ? "tone, LITE waves" : nPatch + nTune ? "tone, a wave's own tuning" : "tone, a second update in a tick");
    syn.setZones(nullptr);
    fw.zpar.clear();
    fw.zoneWave.clear();
    for (int& u : fw.upper) u = 127;
    // The clock's rate.  Above the firmware's clock runs on XtSynth's, which could be any speed: here
    // two minutes (165375 blocks) against the firmware's own timer routine, within one clock.
    {
        XtSynth clk;
        clk.prepare(44100.0);
        Ctl c;
        long all = 0;
        for (const int tempo : { 200, 777, 1200, 2330, 2500 }) {
            rep.caseNo = tempo;
            c.patchTempo = tempo;
            clk.setControls(c);
            uint32_t ticks = 0;
            if (!fw.msTicks(tempo, 120000, ticks)) return 1;
            long clocks = 0;
            for (int i = 0; i < 165375; ++i) { clocks += clk.clocksDue(); clk.render(L, R, 32); }
            ++rep.checks;
            all += clocks;
            if (std::labs(clocks * 20 - (long)ticks) > 20) rep.bad("clock rate", "fw %.0f port %.0f", (float)ticks, (float)(clocks * 20), 0);
        }
        std::printf("tone, the clock's rate: 5 tempos, 2 minutes each, %ld clocks\n", all);
        summary("tone, the clock's rate");
    }
    // The outside clock alone, on what no transport gives: clocks in no time, and long silences.  The firmware's
    // F8 and timer routines against the port's, and one clock to an F8: its ticks on to the next 20 (0x894d4210).
    {
        XtSynth clk;
        clk.prepare(44100.0);
        Ctl c;
        c.clockSource = 1;
        clk.hostClock(120.0, false, 0.0);
        clk.setControls(c);
        if (!fw.clockSource(0) || !fw.clockSource(1)) return 1;
        clockFields(clk, 0);
        long f8s = 0;
        const auto silence = [&](int ms, int at) {
            for (int k = 0; k < ms; ++k) { if (!fw.timer()) return false; clk.clockMs(); }
            clockFields(clk, at);
            return true;
        };
        for (int i = 0; i < 600; ++i) {
            rep.caseNo = i;
            const int kind = r.range(0, 9);
            // (and a stretch with no ms at all, so that a count of six takes none)
            if (!silence(i >= 400 && i < 414 ? 0 : kind < 3 ? 0 : kind < 6 ? r.range(0, 3) : kind < 9 ? r.range(0, 900) : r.range(1000, 5000), i)) return 1;
            // the count's end, a ms at a time: it gives up 65536 ms into a count
            if (i == 300 && (!silence(0xffff - clk.clockSince, i) || !silence(1, i) || !silence(1, i))) return 1;
            const uint32_t from = fw.c.rd32(kClockObj + 0x28);
            if (!fw.clockIn()) return 1;
            clk.clockIn();
            ++f8s;
            clockFields(clk, i);
            const uint32_t to = fw.c.rd32(kClockObj + 0x28);
            ++rep.checks;
            if (to % 20 != 0 || to == from || to - from > 20) rep.bad("an F8's ticks", "from %.0f to %.0f", (double)from, (double)to, i);
        }
        std::printf("tone, an outside clock alone: %ld F8s\n", f8s);
        summary("tone, an outside clock alone");
    }
    // The host's transport to the clocks.  The firmware has no say in this, so against where the transport says it is.
    {
        std::vector<float> hl(4096), hr(4096);
        long all = 0;
        const auto start = [](XtSynth& clk, double rate, double bpm, double ppq) {
            clk.prepare(rate);
            Ctl c;
            c.clockSource = 1;
            clk.hostClock(bpm, true, ppq);
            clk.setControls(c);
        };
        rep.caseNo = 0;
        for (const double rate : { 44100.0, 48000.0 })
            for (const int hb : { 441, 4096 })
                for (const double bpm : { 20.5, 97.3, 120.0, 120.5, 300.0 }) {
                    ++rep.caseNo;
                    const double perBlock = (double)hb / rate * bpm / 60.0;     // quarter notes
                    const int nb = (int)(20.0 * rate / hb);
                    {   // Running: the clocks it passes, and no correction to speak of at each block, whatever the frames waiting
                        XtSynth clk;
                        const double from = 3.3;
                        start(clk, rate, bpm, from);
                        const int64_t done = clk.hostDone;
                        double ppq = from;
                        for (int b = 0; b < nb; ++b) {
                            const double was = clk.hostAt;
                            clk.hostClock(bpm, true, ppq);
                            ++rep.checks;
                            if (b && std::fabs(clk.hostAt - was) > 1e-6) rep.bad("host: where a block is", "said %.6f had %.6f", clk.hostAt, was, b);
                            clk.render(hl.data(), hr.data(), hb);
                            ppq += perBlock;
                        }
                        const double exact = std::floor(ppq * 24.0) - std::floor(from * 24.0), got = (double)(clk.hostDone - done);
                        all += clk.hostDone - done;
                        ++rep.checks;
                        if (std::fabs(exact - got) > 2.0) rep.bad("host: clocks running", "transport %.0f port %.0f", exact, got, 0);
                    }
                    {   // Stopped: the clocks run on at the tempo, and go on at the last it told
                        XtSynth clk;
                        start(clk, rate, bpm, 0.0);
                        const int64_t done = clk.hostDone;
                        for (int b = 0; b < nb; ++b) {
                            clk.hostClock(b % 2 ? bpm : 0.0, false, 12345.0);
                            clk.render(hl.data(), hr.data(), hb);
                        }
                        const double exact = (double)nb * perBlock * 24.0, got = (double)(clk.hostDone - done);
                        ++rep.checks;
                        if (std::fabs(exact - got) > 2.0) rep.bad("host: clocks stopped", "tempo %.0f port %.0f", exact, got, 0);
                    }
                    {   // A loop's end and a locate: what it passes gives no clocks, a block never has more than it should
                        XtSynth clk;
                        start(clk, rate, bpm, 8.0);
                        double ppq = 8.0, exact = 0.0;
                        int64_t done = clk.hostDone;
                        for (int b = 0; b < nb; ++b) {
                            if (b % 50 == 49) { ppq = b % 100 == 99 ? 8.0 : ppq + 1000.0; done = (int64_t)std::ceil(ppq * 24.0) - 1; }
                            clk.hostClock(bpm, true, ppq);
                            const int64_t was = clk.hostDone;
                            clk.render(hl.data(), hr.data(), hb);
                            const double should = std::floor((ppq + perBlock) * 24.0) - (double)done;
                            ++rep.checks;
                            if (std::fabs((double)(clk.hostDone - was) - should) > 2.0) rep.bad("host: clocks in a block", "transport %.0f port %.0f", should, (double)(clk.hostDone - was), b);
                            ppq += perBlock;
                            done = (int64_t)std::floor(ppq * 24.0);
                            exact += should;
                        }
                    }
                }
        // The tempo the converters read: on average the transport's, and never more than the blocks and the ms can take it from there
        for (const double bpm : { 25.0, 97.3, 120.0, 120.5, 174.0, 233.0, 249.0 }) {
            ++rep.caseNo;
            XtSynth clk;
            start(clk, 44100.0, bpm, 0.0);
            double ppq = 0.0, sum = 0.0, worst = 0.0;
            long counts = 0;
            const double exact = 15000.0 / bpm;
            for (int b = 0; b < 82688; ++b) {            // a minute
                if (b % 16 == 0) clk.hostClock(bpm, true, ppq);
                const bool due = clk.clocksDue() > 0;
                clk.render(L, R, 32);
                ppq += 32.0 / 44100.0 * bpm / 60.0;
                if (due && clk.clockSeen == 0 && counts++ > 0) {
                    sum += clk.clockSix;
                    worst = std::max(worst, std::fabs((double)clk.clockSix - exact));
                }
            }
            rep.checks += 2;
            if (worst > 3.0) rep.bad("host: a count of six", "exact %.3f off by %.3f", exact, worst, 0);
            if (std::fabs(sum / (double)(counts - 1) - exact) > 0.002 * exact) rep.bad("host: the count's mean", "exact %.3f port %.3f", exact, sum / (double)(counts - 1), 0);
            if (std::getenv("XT_TONE_HOST")) std::printf("  host %.1f BPM: six %.3f ms, mean %.3f, worst off %.3f, reads %d\n", bpm, exact, sum / (double)(counts - 1), worst, clk.tempo10Now());
        }
        {   // No tempo told: INTERNAL, as if Clock Source were; the first tempo goes EXTERNAL, as at the object's start; the source back again
            XtSynth clk;
            clk.prepare(44100.0);
            Ctl c;
            c.clockSource = 1;
            c.patchTempo = 900;
            clk.hostClock(0.0, true, 16.0);
            clk.setControls(c);
            rep.checks += 6;
            if (clk.clockExt) rep.bad("host: no tempo", "ext %.0f want %.0f", clk.clockExt, 0, 0);
            if (clk.tempo10Now() != 900) rep.bad("host: no tempo's reading", "port %.0f want %.0f", clk.tempo10Now(), 900, 0);
            clk.render(L, R, 32);
            clk.hostClock(180.0, true, 16.0);
            if (!clk.clockExt) rep.bad("host: its first tempo", "ext %.0f want %.0f", clk.clockExt, 1, 0);
            if (clk.tempo10Now() != 1200) rep.bad("host: its first reading", "port %.0f want %.0f", clk.tempo10Now(), 1200, 0);
            if (clk.hostDone != 383) rep.bad("host: where it starts", "port %.0f want %.0f", (double)clk.hostDone, 383, 0);
            c.clockSource = 0;
            clk.setControls(c);
            if (clk.clockExt || clk.tempo10Now() != 900) rep.bad("host: back to INTERNAL", "ext %.0f reads %.0f", clk.clockExt, clk.tempo10Now(), 0);
            // Going EXTERNAL after the transport has run on with nothing taking its clocks: none for that time
            clk.hostClock(120.0, false, 0.0);
            for (int b = 0; b < 2000; ++b) clk.render(L, R, 32);
            c.clockSource = 1;
            clk.setControls(c);
            ++rep.checks;
            if (clk.clocksDue() > 1) rep.bad("host: going to it", "clocks due %.0f want %.0f", clk.clocksDue(), 1, 0);
        }
        std::printf("tone, the host's clock: %ld clocks\n", all);
        summary("tone, the host's clock");
    }
    return total ? 3 : 0;
}
#endif
