// A whole firmware tone against XtSynth on the same settings: the wiring check.
//
// xt_tone <rom.2.bin> [cases] [seed] [inputs dir]
//   XT_TONE_FIRST=1: only each case's first difference; XT_TONE_DUMP=<case>|a: its settings
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
//   a knob moved during a note: the updaters and the note's update (0x894dc0b0).
// Only the OS is hooked: its locks and clock, the voice allocator's lists, and
// the wave store (0x8949e5c0…): the waves lie in RAM as the loader leaves them,
// from the same encodes XtSynth plays (their vpdt and load-time bands).
//
// XtSynth runs on the same Controls and notes.  Compared, every block: both OSC
// buffers, the mix, the output of the COSM stage, the TVA and the output; at every
// update, the envelopes' and LFOs' outputs and stages, the pitch terms and ramps,
// and each voice's fields; at each case's end, FM's lines; all bit for bit in the
// XT's units.  Within a tolerance:
// only a SOLO wave's buffers and what follows them (kSoloTol of the block's peak),
// since XtSoloVoice is not bit-exact (xt_fw solocmp).  Past the wave's data a player
// reads whatever lies there: the waves lie among zeros, which is what the port reads,
// and a note whose read leaves memory altogether is left off and counted.
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
#include <map>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>
#include "Sh4Interp.h"
#include "XtSoloEncoder.h"
#include "XtSynth.h"

using OscC = XtSynth::OscControls;
using Ctl = XtSynth::Controls;

// The peek at XtSynth's voices (a friend of the class).
struct XtToneProbe {
    static void bend(XtSynth& s, int b) { s.bender = b; }
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
constexpr float kSoloTol = 2e-2f, kSoloFloor = 8192.0f;   // a SOLO wave's buffers: of the block's peak, plus a floor (−108 dB of full scale)

constexpr uint32_t kDescTab = 0x897a167c, kRomBase = 0x89300000;
constexpr int kCosm2 = 0x10000;      // COSM 2's IDs in the patch table: COSM 1's plus this

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
};

class Firmware {
public:
    Sh4 c;
    std::vector<uint8_t> rom;
    std::unordered_map<int, int> par;   // the patch, by parameter ID
    long frames = 0;                    // the system clock
    FwWave wave, wave2;                 // the waves the Wave Numbers find: 1 OSC 1's, 2 OSC 2's
    int bender = 8192;
    int cosmSection = 0;
    uint32_t cosmBuf = 0;
    uint16_t held[8] = {};
    const FwWave& waveOf(uint32_t id) const { return id == 2999 + 2 ? wave2 : wave; }

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
        for (uint32_t a : { 0x89301920u, 0x89301930u, 0x89301950u, 0x89301960u, 0x89301860u }) c.hook(a, [](Sh4& m) { m.ret(0); });
        c.hook(0x89301940, [this](Sh4& m) { m.ret((uint32_t)(int32_t)std::floor((double)frames * 1000.0 / 44100.0)); });
        auto getter = [this](Sh4& m) {
            const int id = (int16_t)m.r[5];
            const bool cosm = id == 0x3e7 || id == 0x3e8 || (id >= 0x8d0 && id <= 0x954);
            auto it = par.find(cosm && cosmSection ? id + kCosm2 : id);
            m.ret((uint32_t)(it != par.end() ? it->second : defaultOf(id)));
        };
        c.hook(0x8947d310, getter);
        c.hook(0x8947d330, getter);
        c.hook(0x894798a0, [this](Sh4& m) { cosmSection = (int)m.r[7]; m.ret(0); });   // which COSM's parameters are read next
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
        c.hook(0x894796d0, [](Sh4& m) { m.ret(0); });
        if (!c.call(0x894e5f80, { 0x89d1b070u, 0, 0, 0 }, kSp)) { std::fprintf(stderr, "system: %s\n", c.error().c_str()); return false; }
        return true;
    }

    bool updatePatch() {
        if (!c.call(0x894dfe10, { 0 }, kSp)) { std::fprintf(stderr, "updaters: %s\n", c.error().c_str()); return false; }
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
    template <class Carry> bool start(FwNote& n, int idx, int key, Carry carry) {
        n.note = kNote + 0x100 * (uint32_t)idx;
        c.wr8(kMsg, 0);                                // the part
        c.write(n.note, std::vector<uint8_t>(0x100, 0).data(), 0x100);
        c.wr32(n.note + 0x10, 0x89833e44);
        c.wr8(n.note + 0x40, (uint8_t)idx);
        c.wr8(n.note + 0x52, 0xff);
        c.wr8(kA1, 0); c.wr8(kA1 + 1, 0); c.wr8(kA1 + 2, 0);
        c.wr32(kSp, kA1); c.wr32(kSp + 4, 0);
        if (!c.call(0x894dc130, { n.note, kK, kMsg, (uint32_t)key }, kSp)) { std::fprintf(stderr, "note: %s\n", c.error().c_str()); return false; }
        n.BL = c.rd32(n.note + 0x4c); n.IN = c.rd32(n.note + 0x5c);
        n.TN = c.rd32(n.BL + 0x6bc);
        n.raw = c.rd8(kK + 0xc);
        n.live = true; n.released = n.ended = false;
        carry(n);
        if (!c.call(0x894db830, { n.BL, 0, (uint32_t)(int8_t)key }, kSp)) { std::fprintf(stderr, "tone note-on: %s\n", c.error().c_str()); return false; }
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
    bool keyOff(int raw, int& back) {
        held[raw >> 4] &= (uint16_t)~(1u << (raw & 15));
        c.wr16(kN + 0xc + 2 * (uint32_t)(raw >> 4), held[raw >> 4]);
        c.wr8(kN + 9, (uint8_t)raw);
        uint32_t r0 = 0;
        if (!c.call(0x894d63c0, { kK, 0, 0x7f }, kSp, &r0)) { std::fprintf(stderr, "key off: %s\n", c.error().c_str()); return false; }
        back = (int32_t)r0;
        return true;
    }

    bool noteOff(FwNote& n, int relVel) {
        c.wr8(n.note + 0x2f, (uint8_t)relVel);
        if (!c.call(0x894dc270, { n.note }, kSp)) { std::fprintf(stderr, "note-off: %s\n", c.error().c_str()); return false; }
        n.released = true;
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

    bool stop(FwNote& n) {
        if (!c.call(0x894f5a70, { n.TN, 3 }, kSp)) { std::fprintf(stderr, "stop: %s\n", c.error().c_str()); return false; }
        if (!c.call(0x894eb810, { n.IN, 3 }, kSp)) { std::fprintf(stderr, "stop input: %s\n", c.error().c_str()); return false; }
        n.live = false;
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

bool loadWave(Firmware& f, uint32_t& next, const std::string& path, int channels, int type, long maxFrames, TestWave& out) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    std::vector<int16_t> raw;
    int16_t x;
    while (raw.size() < (size_t)(maxFrames * channels) && std::fread(&x, 2, 1, fp) == 1) raw.push_back(x);
    std::fclose(fp);
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
        next += 0x40000;                                   // zeros around each, as a band's reads outside it get from the port
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
    if (type == 1) {
        std::vector<int16_t> x16((size_t)frames);
        for (long i = 0; i < frames; ++i)
            x16[(size_t)i] = channels == 2 ? (int16_t)(((int)raw[(size_t)(2 * i)] + (int)raw[(size_t)(2 * i + 1)]) >> 1) : raw[(size_t)i];
        out.recs = xtSoloEncode(x16.data(), (int)frames);
        const std::vector<uint8_t> v = soloVpdt(out.recs, frames, channels);
        w.chunk[5] = put(v.data(), v.size());
        w.band[0] = put(raw.data(), raw.size() * 2);
    } else {
        w.chunk[5] = put(out.enc->vpdt.data(), out.enc->vpdt.size());
        static const int slot[6] = { 0, 2, 3, 4, 5, 6 };
        for (int k = 0; k < 6; ++k) w.band[slot[k]] = put(out.enc->bands.buf[k].data(), out.enc->bands.buf[k].size() * 2);
    }
    w.loaded = true;
    return true;
}

// The cached loop events a wave's player reads, for the loop XtSynth computes: the
// firmware's own builder of them is not found.  As xtFwSoloPlay and xtFwBandPlay lay them.
void writeLoopEvents(Firmware& f, const TestWave& t, const OscC& c) {
    // the region from the fractions, at least 16 long; One Shot's modes are 0
    XtLoop l;
    const int last = (int)t.fw.frames - 1;
    int a = (int)std::lround((double)c.loopStart * last), b = (int)std::lround((double)std::min(c.loopEnd, 1.0f) * last);
    if (b - a < 16) { b = std::min(a + 16, last); a = b - 16; }
    l.on = c.loop;
    l.fwdMode = l.bwdMode = c.loopMode;
    l.fwdStart = l.bwdStart = a;
    l.fwdEnd = l.bwdEnd = b;
    const uint32_t W2 = t.fw.chunk[10];
    f.c.write(W2, std::vector<uint8_t>(0x400, 0).data(), 0x400);
    if (!l.on) { f.c.wr32(W2 + 0xb8, 0x7fffffff); f.c.wr32(W2 + 0x16c, 0x7fffffff); return; }
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
    p[b + 0x23] = o.oscTime; p[b + 0x24] = o.oscTimeKF / 10; p[b + 0x25] = o.lfoTimeDepth; p[b + 0x26] = o.tDepth; p[b + 0x27] = o.oscTimeOffset;
    p[b + 0x28] = o.oscFormant; p[b + 0x29] = o.oscFormantKF / 10; p[b + 0x2a] = o.lfoFormantDepth; p[b + 0x2b] = o.fDepth;
    p[b + 0x2d] = o.robot; p[b + 0x2f] = o.loop; p[b + 0x31] = o.energy;
    p[0x30d + 3 * n] = o.ana.detune; p[0x30e + 3 * n] = o.ana.subOctave; p[0x30f + 3 * n] = o.ana.subLevel;
    const int e = 4 * n;
    envParams(p, e, o.pAttack, o.pDecay, o.pSustain, o.pRelease, 0, 32, 0, 0, 0, 0);
    envParams(p, e + 1, o.tAttack, o.tDecay, o.tSustain, o.tRelease, 0, 32, 0, 0, 0, 0);
    envParams(p, e + 2, o.fAttack, o.fDecay, o.fSustain, o.fRelease, 0, 32, 0, 0, 0, 0);
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
    p[0x110] = c.patchCoarse; p[0x111] = c.patchFine; p[0x112] = c.octave; p[0x113] = !c.mono; p[0x114] = c.legato;   // Mono/Poly: 0 MONO
    p[0x115] = c.portamento; p[0x116] = c.portaMode; p[0x117] = c.portaType; p[0x118] = c.portaTime; p[0x119] = c.portaVeloSens;
    p[0x11e] = c.bendUp; p[0x11f] = c.bendDown;
    p[0x134] = c.scaleTune;
    for (int i = 0; i < 12; ++i) p[0x136 + 2 * i] = c.scale[i];
    p[0x293] = c.structure; p[0x294] = c.mod.on; p[0x295] = c.mod.type; p[0x296] = c.mod.level; p[0x297] = c.mod.attack; p[0x298] = c.mod.release;
    p[0x299] = c.tvaOn;
    envParams(p, 12, c.attack, c.decay, c.sustain, c.release, c.ampVelCurve, c.ampVelSens, c.aSens, c.dSens, c.rSens, c.timeKF / 10);
    return p;
}

// ── Random settings ─────────────────────────────────────────────────────────
struct Rng {
    uint32_t s;
    uint32_t next() { s = s * 1103515245u + 12345u; return (s >> 8) & 0xffff; }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); }
    bool one(int n) { return next() % (uint32_t)n == 0; }
};

void drawOsc(Rng& r, OscC& o) {
    o.on = true;
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
    o.pAttack = r.range(0, 90); o.pDecay = r.range(0, 90); o.pSustain = r.range(0, 127); o.pRelease = r.range(0, 90); o.pDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.tAttack = r.range(0, 90); o.tDecay = r.range(0, 90); o.tSustain = r.range(0, 127); o.tRelease = r.range(0, 90);
    o.fAttack = r.range(0, 90); o.fDecay = r.range(0, 90); o.fSustain = r.range(0, 127); o.fRelease = r.range(0, 90);
    o.lfoWave = r.range(0, 7); o.lfoRate = r.range(0, 127); o.lfoOffset = r.range(0, 4); o.lfoDelay = r.one(2) ? 0 : r.range(0, 60);
    o.lfoFadeMode = r.range(0, 3); o.lfoFade = r.one(2) ? 0 : r.range(0, 60); o.lfoKeySync = !r.one(4);
    o.lfoPitchDepth = r.one(2) ? 0 : r.range(-63, 63);
    o.oscLevel = r.range(0, 127); o.oscLevelKF = 10 * r.range(-20, 20); o.oscLevelLfoDepth = r.one(2) ? 0 : r.range(-63, 63); o.waveGain = r.range(-12, 12);
    o.oAttack = r.range(0, 90); o.oDecay = r.range(0, 90); o.oSustain = r.range(0, 127); o.oRelease = r.range(0, 90);
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
        e->attack = r.range(0, 90); e->decay = r.range(0, 90); e->sustain = u(); e->release = r.range(0, 90); e->velCurve = r.range(0, 7); e->velSens = r.range(-63, 63);
        e->aSens = d(); e->dSens = d(); e->rSens = d(); e->timeKF = kf();
    }
    c.lfoWave = r.range(0, 7); c.lfoRate = u(); c.lfoOffset = r.range(0, 4); c.lfoDelay = r.one(2) ? 0 : r.range(0, 60); c.lfoFadeMode = r.range(0, 3); c.lfoFade = r.one(2) ? 0 : r.range(0, 60); c.lfoKeySync = !r.one(4);
}

Ctl drawControls(Rng& r, int g, bool pcm, bool pcm2 = false) {
    Ctl c;
    drawOsc(r, c);
    drawOsc(r, c.osc2);
    const auto drawPcm = [&r](OscC& o) {
        o.oscType = 1; o.rootKey = r.range(36, 84);
        o.robot = r.one(4);
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
    if (r.one(3)) { c.scaleTune = true; for (int& s : c.scale) s = r.range(-64, 63); }
    c.attack = r.range(0, 90); c.decay = r.range(0, 90); c.sustain = r.range(0, 127); c.release = r.range(0, 90);
    c.aSens = r.one(2) ? 0 : r.range(-63, 63); c.dSens = r.one(2) ? 0 : r.range(-63, 63); c.rSens = r.one(2) ? 0 : r.range(-63, 63);
    c.timeKF = r.one(2) ? 0 : 10 * r.range(-20, 20);
    c.ampVelSens = r.range(-63, 63); c.ampVelCurve = r.range(0, 7);
    c.octave = r.one(3) ? r.range(-3, 3) : 0;
    if (r.one(2)) { c.portamento = true; c.portaMode = r.range(0, 1); c.portaType = r.range(0, 1); c.portaTime = r.range(0, 127); c.portaVeloSens = r.range(-63, 63); }
    return c;
}

// ── Compare ─────────────────────────────────────────────────────────────────
constexpr size_t kBenderOnly = (size_t)-1;     // in the sweep: the bender moved, no knob
// Every field XtSynth keeps in a list of what has changed (setControls): the sweep moves
// these one at a time, where nothing else moving can hide one left out of a list.
std::vector<size_t> listedFields() {
    static const Ctl c;
    std::vector<size_t> f;
    const auto add = [&](const auto& x) { f.push_back((size_t)(reinterpret_cast<const char*>(&x) - reinterpret_cast<const char*>(&c))); };
    for (const auto* x : { &c.attack, &c.decay, &c.sustain, &c.release, &c.aSens, &c.dSens, &c.rSens, &c.timeKF, &c.patchCoarse, &c.portaTime, &c.portaVeloSens, &c.portaType }) add(*x);
    add(c.portamento); add(c.tvaOn);
    for (const OscC* o : { static_cast<const OscC*>(&c), static_cast<const OscC*>(&c.osc2) })
        for (const int* x : { &o->pAttack, &o->pDecay, &o->pSustain, &o->pRelease, &o->tAttack, &o->tDecay, &o->tSustain, &o->tRelease, &o->fAttack, &o->fDecay, &o->fSustain, &o->fRelease,
                              &o->oAttack, &o->oDecay, &o->oSustain, &o->oRelease, &o->oASens, &o->oDSens, &o->oRSens, &o->oTimeKF,
                              &o->lfoWave, &o->lfoRate, &o->lfoOffset, &o->lfoDelay, &o->lfoFadeMode, &o->lfoFade, &o->oscCoarse, &o->pitchKF }) add(*x);
    for (const Ctl::Cosm& k : c.cosm) {
        for (const Ctl::CosmEnv* e : { &k.envA, &k.envB })
            for (const int* x : { &e->attack, &e->decay, &e->sustain, &e->release, &e->aSens, &e->dSens, &e->rSens, &e->timeKF }) add(*x);
        for (const int* x : { &k.lfoWave, &k.lfoRate, &k.lfoOffset, &k.lfoDelay, &k.lfoFadeMode, &k.lfoFade }) add(*x);
    }
    add(c.mod.on);
    for (const int* x : { &c.mod.type, &c.mod.level, &c.mod.attack, &c.mod.release }) add(*x);
    f.push_back(kBenderOnly);
    return f;
}

struct Report {
    std::map<std::string, long> kinds;
    long checks = 0;
    int caseNo = 0, firstCase = -1;
    bool first = std::getenv("XT_TONE_FIRST") != nullptr;      // only each case's first difference
    void bad(const std::string& kind, const char* fmt, double a, double b, int blk) {
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
    std::vector<TestWave> waves;
    if (argc > 4) {
        const std::string dir = std::string(argv[4]) + "/";
        struct W { const char* file; int ch, type; } list[] = { { "vocal.bin", 1, 1 }, { "w358.bin", 2, 1 }, { "drum.bin", 2, 3 }, { "chord.bin", 1, 5 } };
        uint32_t next = kRam + 0x200000;
        for (const W& w : list) {
            TestWave t;
            if (!loadWave(fw, next, dir + w.file, w.ch, w.type, 30000, t)) { std::fprintf(stderr, "wave %s\n", w.file); return 2; }
            waves.push_back(std::move(t));
        }
    }
    XtSynth syn;
    syn.prepare(44100.0);
    Report rep;
    long blocks = 0, notes = 0, wild = 0, legatos = 0, truncates = 0;
    float worstSolo = 0;
    float L[32], R[32];
    uint32_t fwFree[2] = {};
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
        std::printf("%s: a SOLO wave's buffers within %.1e of the block's peak; %ld notes left off, a read outside memory\n", name, worstSolo, wild);
        long sub = 0;
        for (const auto& k : rep.kinds) { std::printf("  %-28s %ld different\n", k.first.c_str(), k.second); sub += k.second; }
        std::printf("%s: %ld different\n", name, sub);
        total += sub;
        rep.kinds.clear();
        rep.checks = notes = legatos = truncates = blocks = wild = 0;
        worstSolo = 0;
    };
    const int nOsc2 = waves.empty() ? 0 : nCases / 2;
    const int tickFrom = nCases + nSweep + nOsc2, nTick = 16 * 3;     // each COSM type, each kind of second update
    for (int g = 0; g < tickFrom + nTick; ++g) {
        rep.caseNo = g;
        const bool twoInTick = g >= tickFrom;
        const bool osc2Case = g >= nCases + nSweep && !twoInTick;
        if (g == nCases + nSweep) summary("tone");
        if (nOsc2 && g == tickFrom) summary("tone, a wave on OSC 2");
        const bool sweep = g >= nCases && g < nCases + nSweep;
        const size_t sweepAt = sweep ? listed[(size_t)(g - nCases) / 4] : 0;
        const int sweepStage = sweep ? (g - nCases) % 4 : 0;
        // one case in two plays a wave on OSC 1 (with the analog OSC 2 or alone); the last
        // ones a wave on OSC 2, over OSC 1 off, analog or on another wave
        const TestWave* wv2 = osc2Case ? &waves[(size_t)r.range(0, (int)waves.size() - 1)] : nullptr;
        const TestWave* wv = !sweep && !twoInTick && !waves.empty() && g % 2 == 1 ? &waves[(size_t)r.range(0, (int)waves.size() - 1)] : nullptr;
        if (wv && wv == wv2) wv = &waves[(size_t)((wv - waves.data() + 1) % (long)waves.size())];   // a wave's loop is its own
        Ctl ctl = drawControls(r, g, wv != nullptr, wv2 != nullptr);
        if (sweep) {
            // analog on both OSCs, all the envelopes and LFOs bound and running, a
            // portamento gliding: whatever a field moves can be seen
            ctl.on = ctl.osc2.on = true;
            ctl.tvaOn = true;
            ctl.lfoKeySync = ctl.osc2.lfoKeySync = true;
            ctl.portamento = true;
            ctl.portaTime = 100;
            ctl.oscTimeOffset = ctl.osc2.oscTimeOffset = 2;
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
        // COSM 1 on OSC 1's buffer, OSC 2 sounding next to it, the level moving Width and Detune.
        // At random this is rare enough for 2000 cases to miss it.  Only with OSC 1 on already:
        // a wave newly played can leave memory (band reads), which ends a case early and changes
        // the draws of all the cases after it.
        if (wv && wv->channels == 2 && wv->type != 1 && ctl.on && g % 10 == 3) {
            ctl.osc2.on = true;
            ctl.structure = g % 20 == 3 ? 1 : 2;
            ctl.cosm[0].on = true;
            ctl.cosm[0].type = g % 40 < 20 ? 5 : 6;
            for (Ctl::Cosm::SideBand& b : ctl.cosm[0].sb) b.detuneOwn = b.widthOwn = 40;
        }
        // And COSM 2 on a stereo OSC 2's buffer in the third Structure, next to the last tone's mix
        if (wv2 && wv2->channels == 2 && wv2->type != 1 && ctl.osc2.on && ctl.structure == 2) {
            ctl.cosm[1].on = true;
            ctl.cosm[1].type = g % 4 == 0 ? 5 : 6;
            for (Ctl::Cosm::SideBand& b : ctl.cosm[1].sb) b.detuneOwn = b.widthOwn = 40;
        }
        const bool solo1 = wv && wv->type == 1 && ctl.on, solo2 = wv2 && wv2->type == 1 && ctl.osc2.on, soloCase = solo1 || solo2;
        // The stereo Side Band follower reads past its block, so a SOLO wave's small differences
        // reach the buffer beside it: that one takes the SOLO tolerance too.
        const auto sideBand = [&](int k) { return ctl.cosm[k].on && (ctl.cosm[k].type == 5 || ctl.cosm[k].type == 6); };
        const bool soloBeside = soloCase && (sideBand(0) || sideBand(1));
        // X-MOD's phase on OSC 2's output, which a SOLO wave's gives only closely
        if (solo2 && ctl.ana.wave == 13) ctl.ana.wave = 0;
        // Lo-Fi's quantizer turns the SOLO player's small differences into a step of its own.
        if ((wv && wv->type == 1) || (wv2 && wv2->type == 1)) for (Ctl::Cosm& k : ctl.cosm) if (k.type == 9) k.type = 0;
        // what the case does besides one note: 0 none, 1 a second note on a POLY part,
        // 2 on a MONO part with Legato, 3 on a MONO part without
        const int kind = sweep || twoInTick ? 2 : g % 5 == 0 ? 1 : g % 5 == 1 ? 2 : g % 5 == 2 ? 3 : 0;
        ctl.mono = kind >= 2;
        ctl.legato = kind == 2;
        if (wv) {
            syn.setSample(wv->enc);
            fw.wave = wv->fw;
            fw.c.wr8(wv->fw.chunk[2] + 0xa, (uint8_t)ctl.rootKey);
        } else {
            // an analog case: in some, a stereo wave left loaded, which it must not read or follow
            syn.setSample(g % 4 == 2 && !waves.empty() ? waves[2].enc : nullptr);
            fw.wave = FwWave{};
        }
        if (osc2Case) {
            syn.setSample(wv2->enc, 1);
            fw.wave2 = wv2->fw;
            fw.c.wr8(wv2->fw.chunk[2] + 0xa, (uint8_t)ctl.osc2.rootKey);
        }
        const int raw[2] = { sweep || twoInTick ? 48 : r.range(0, 127), sweep || twoInTick ? 72 : r.range(0, 127) }, vel[2] = { r.range(1, 127), r.range(1, 127) }, relVel[2] = { r.range(1, 127), r.range(1, 127) };
        const int nBlocks = sweep || twoInTick ? 90 : g % 23 == 7 ? 700 : 60 + r.range(0, 120);     // some long enough to run past a wave's end
        const int secondAt = twoInTick ? -1 : sweep ? 1 : kind && raw[0] != raw[1] ? r.range(1, 30) : -1;
        const int offAt[2] = { twoInTick ? 1000 : sweep ? (sweepStage == 3 ? 3 : 1000) : 20 + r.range(0, 40), twoInTick ? 1000 : sweep ? (sweepStage == 3 ? 4 : 1000) : secondAt >= 0 ? secondAt + r.range(1, 40) : -1 };
        const int liveAt = twoInTick ? 60 : sweep ? (sweepStage >= 2 ? 50 : 20) : g % 3 == 2 ? r.range(1, 50) : -1;     // a stage of 0 still takes a tick
        // the free-running LFOs' phases, once a note has shown which objects they are
        for (int n = 0; n < 2; ++n) {
            if (!fwFree[n]) continue;
            ++rep.checks;
            const float a = fw.f32(fwFree[n] + 0x10), b = n ? syn.freeLfo2.phase() : syn.freeLfo.phase();
            if (!bits(a, b)) rep.bad("free lfo " + std::to_string(n + 1) + " phase", "fw %.9g port %.9g", a, b, -2);
        }
        // time with nothing sounding: the free-running LFOs run on
        for (int i = r.range(0, 30); i > 0; --i) {
            if (syn.tickCount >= 441 && !fw.tickFree()) return 1;
            syn.render(L, R, 32);
        }
        syn.setControls(ctl);
        fw.par = patchOf(ctl);
        if (!fw.updatePatch()) return 1;
        fw.bender = r.one(3) ? r.range(0, 16383) : 8192;
        syn.pitchBend(fw.bender);
        if (wv) writeLoopEvents(fw, *wv, ctl);
        if (wv2) writeLoopEvents(fw, *wv2, ctl.osc2);

        // What a voice keeps across notes (its phases, and fields a waveform does not
        // use): the firmware's tone comes from a pool, so it starts from the port's.
        auto carryFrom = [&](XtSynth::Voice& pv) {
            return [&, pv = &pv](FwNote& n) {
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
                for (int o = 0; o < 2; ++o) {
                    if ((o ? ctl.osc2.oscType : ctl.oscType) != 0) continue;
                    const XtAnalogOsc& a = osc(*pv, o).ana;
                    const uint32_t V = Firmware::voice(n, o);
                    for (const VField& f : anaFields(a)) { uint32_t u; std::memcpy(&u, &f.v, 4); fw.c.wr32(V + f.off, u); }
                    fw.c.wr8(V + 0x248, (uint8_t)a.noiseHold());
                    fw.c.wr32(V + 0x1dc, a.flags()); fw.c.wr32(V + 0x244, a.noiseState());
                    fw.c.wr32(V + 0x1e8, (uint32_t)a.hqFalling()); fw.c.wr32(V + 0x278, (uint32_t)a.fbWrite());
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
                const OscC& oc = o ? static_cast<const OscC&>(syn.ctl.osc2) : static_cast<const OscC&>(syn.ctl);
                const std::string os = "osc " + std::to_string(o + 1) + " ";
                const float la = fw.f32(Firmware::lfoAt(n, (uint32_t)o + 1) + 0x38), lb = s.lfo.out();
                ++rep.checks;
                if (!bits(la, lb)) rep.bad(os + "lfo output", "fw %.9g port %.9g", la, lb, blk);
                const VField vs[] = { { "cents", 8, syn.noteCents(v, s, oc, v.note) }, { "source cents", 0x68, syn.noteCents(v, s, oc, v.srcKey) }, { "pitch knob", 0xc, xtPitchRatio(oc.oscPitch) } };
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
                        ++rep.checks;
                        const float x = fw.f32(V + f.off);
                        if (!bits(x, f.v)) rep.bad(os + f.name, "fw %.9g port %.9g", x, f.v, blk);
                    }
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
                if (!fw.start(fn[i], i, keyOf(raw[i]), carryFrom(nextVoice()))) return false;
                ++notes;
            }
            syn.noteOn(raw[i], (float)vel[i] / 127.0f);
            if (fn[i].live && fn[i].vi < 0) fn[i].vi = voiceOfNewest();
            for (int o = 0; o < 2; ++o)
                if (fn[i].live)
                    if (const uint32_t f = fw.c.rd32(Firmware::lfoAt(fn[i], (uint32_t)o + 1) + 0x7c)) fwFree[o] = f;
            compareAll(blk);
            return true;
        };
        // The part's note-off (0x894dbea0)
        auto stopNote = [&](int i, int blk) {
            fw.frames = syn.hostFrames;
            int back = -1;
            if (!fw.keyOff(raw[i], back)) return false;
            if (fw.partMono() && back >= 0)
                for (FwNote& n : fn)
                    if (n.live && !n.released) { ++legatos; if (!fw.legato(n, keyOf(back))) return false; }
            for (FwNote& n : fn)
                if (n.live && !n.released && n.raw == raw[i] && !fw.noteOff(n, relVel[i])) return false;
            syn.noteOff(raw[i], (float)relVel[i] / 127.0f);
            compareAll(blk);
            return true;
        };
        // Knobs moved, or the bender: the part's tables and each note's update on the firmware's
        // side, setControls or pitchBend on XtSynth's
        auto moveLive = [&](const Ctl& n, int bentFrom, int blk) {
            const bool panelMoved = !(n == ctl), moved = panelMoved || fw.bender != bentFrom;   // nothing moved, no update on either side
            ctl = n;
            fw.par = patchOf(ctl);
            fw.frames = syn.hostFrames;
            if (!fw.updatePatch()) return false;
            for (FwNote& fnote : fn)
                if (moved && fnote.live && !fnote.ended && !fw.liveUpdate(fnote)) return false;
            if (panelMoved) {                        // the bender with the knobs, in the one update the firmware's gets
                XtToneProbe::bend(syn, fw.bender);
                syn.setControls(ctl);
            } else syn.pitchBend(fw.bender);
            compareAll(blk);
            return true;
        };

        for (FwNote& n : fn) { n.vi = -1; n.live = n.released = n.ended = false; }
        fw.cosmBuf = kRam + 0x1000000;
        bool keyDown[2] = {};
        // The mix's first words, which follow OSC 2's buffer: between the cases XtSynth's tails
        // go on rendering and the firmware's stop, so the note starts from the port's.
        for (uint32_t i = 0; i < 3; ++i) { uint32_t u; std::memcpy(&u, &syn.mixPast[i], 4); fw.c.wr32(kBufMix + 4 * i, u); }
        if (!startNote(0, -1)) return 1;
        keyDown[0] = true;
        if (dumpWhich && (dumpWhich[0] == 'a' || std::atoi(dumpWhich) == g))
            std::printf("case %d kind %d keys %d %d vel %d %d wave %d: osc1 on %d wave %d, osc2 on %d wave %d; cosm %d %d, %d %d; second at %d, off at %d %d, live at %d; mod %d type %d level %d a %d r %d, ch %d\n", g, kind, raw[0], raw[1], vel[0], vel[1], wv ? wv->type : 0, ctl.on, ctl.ana.wave,
                        ctl.osc2.on, ctl.osc2.ana.wave, ctl.cosm[0].on, ctl.cosm[0].type, ctl.cosm[1].on, ctl.cosm[1].type, secondAt, offAt[0], offAt[1], liveAt, ctl.mod.on, ctl.mod.type, ctl.mod.level, ctl.mod.attack, ctl.mod.release, wv ? wv->channels : 0);
        bool over = false, leftOff = false;
        for (int blk = 0; blk < nBlocks && !over; ++blk) {
            if (blk == secondAt) { if (!startNote(1, blk)) return 1; keyDown[1] = true; }
            for (int i = 0; i < 2; ++i)
                if (blk == offAt[i] && keyDown[i]) { if (!stopNote(i, blk)) return 1; keyDown[i] = false; }
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
            } else if (blk == liveAt) {                  // knobs moved, and the bender
                Ctl n = drawControls(r, g, wv != nullptr, wv2 != nullptr);
                if (sweep && sweepAt == kBenderOnly) n = ctl;
                else if (sweep || r.one(2)) {            // one word of the panel only: what XtSynth notices has changed
                    static_assert(std::is_trivially_copyable_v<Ctl>);
                    Ctl one = ctl;
                    const size_t w = sweep ? sweepAt & ~(size_t)3 : (size_t)r.range(0, (int)(sizeof(Ctl) / 4) - 1) * 4;
                    for (int tries = 0; tries < 16 && !std::memcmp(reinterpret_cast<const char*>(&ctl) + w, reinterpret_cast<const char*>(&n) + w, 4); ++tries)
                        n = drawControls(r, g, wv != nullptr, wv2 != nullptr);
                    std::memcpy(reinterpret_cast<char*>(&one) + w, reinterpret_cast<const char*>(&n) + w, 4);
                    n = one;
                }
                n.on = ctl.on; n.oscType = ctl.oscType; n.rootKey = ctl.rootKey; n.ana.wave = ctl.ana.wave; n.ana.subOctave = ctl.ana.subOctave; n.octave = ctl.octave;
                n.osc2.on = ctl.osc2.on; n.osc2.oscType = ctl.osc2.oscType; n.osc2.ana.wave = ctl.osc2.ana.wave; n.osc2.ana.subOctave = ctl.osc2.ana.subOctave;
                n.oscTimeOffset = ctl.oscTimeOffset; n.lfoKeySync = ctl.lfoKeySync; n.osc2.lfoKeySync = ctl.osc2.lfoKeySync;
                n.structure = ctl.structure;             // latched at note-on, but its own check is xt_analog's
                n.mono = ctl.mono; n.legato = ctl.legato; n.robot = ctl.robot; n.loop = ctl.loop; n.loopMode = ctl.loopMode; n.loopStart = ctl.loopStart; n.loopEnd = ctl.loopEnd;
                n.osc2.rootKey = ctl.osc2.rootKey; n.osc2.oscTimeOffset = ctl.osc2.oscTimeOffset; n.osc2.robot = ctl.osc2.robot; n.osc2.loop = ctl.osc2.loop;
                n.osc2.loopMode = ctl.osc2.loopMode; n.osc2.loopStart = ctl.osc2.loopStart; n.osc2.loopEnd = ctl.osc2.loopEnd;
                for (int k = 0; k < 2; ++k) { n.cosm[k].on = ctl.cosm[k].on; n.cosm[k].type = ctl.cosm[k].type; n.cosm[k].lfoKeySync = ctl.cosm[k].lfoKeySync; n.cosm[k].odType = ctl.cosm[k].odType; n.cosm[k].ampType = ctl.cosm[k].ampType; }
                const int bentFrom = fw.bender;
                if (sweep ? sweepAt == kBenderOnly : r.one(2)) fw.bender = r.range(0, 16383);
                if (!moveLive(n, bentFrom, blk)) return 1;
            }
            const bool tick = syn.tickCount >= 441;
            fw.frames = syn.hostFrames;
            if (tick && !fw.tickFree()) return 1;
            // in XtSynth's voice order, so that the last rendered is the same on both sides
            FwNote* order[2] = { &fn[0], &fn[1] };
            if (fn[0].vi >= 0 && fn[1].vi >= 0 && fn[1].vi < fn[0].vi) std::swap(order[0], order[1]);
            fw.clearBus();
            bool leftMemory = false;
            for (FwNote* n : order) {
                if (!n->live || n->ended) continue;
                if (tick && !fw.tick(*n)) return 1;
                if (fw.c.rd32(n->TN + 0xc) != 2) { n->ended = true; continue; }
                if (!fw.render(*n)) { leftMemory = true; break; }
            }
            syn.render(L, R, 32);
            ++blocks;
            if (leftMemory) {                            // a player's read outside all memory: the note ends here
                ++wild;
                leftOff = true;
                break;
            }
            for (FwNote* n : order) {
                if (!n->live) continue;
                // a truncated tone is over when its fade is (+0x44), ended at its next update
                const bool fwDone = n->ended || fw.c.rd32(Firmware::tva(*n) + 0x44) != 0, synDone = n->vi < 0 || !syn.voice[n->vi].active;
                if (fwDone != synDone) { rep.bad("voice end", "fw %.0f port %.0f", fwDone, synDone, blk); over = true; break; }
                if (fwDone) n->ended = true;
            }
            if (over) break;
            FwNote* last = nullptr;
            for (FwNote* n : order) if (n->live && !n->ended) last = n;
            if (!last) { if (secondAt > blk) continue; break; }
            if (tick) compareAll(blk);
            struct B { const char* name; uint32_t a; const float* p; bool solo; } bs[] = {
                { "osc 2 buffer", kBuf2, syn.tapOsc2, solo2 || soloBeside }, { "osc 1 buffer", kBuf1, syn.tapOsc1, solo1 || soloBeside }, { "mix", kBufMix, syn.tapMix, soloCase }, { "cosm output", kBufMix, syn.tapCosm, soloCase } };
            for (const B& b : bs) {
                // the COSMs work in place on the mix: a mix's own buffer is theirs with one on
                if (b.p == syn.tapMix && (ctl.cosm[0].on || ctl.cosm[1].on)) continue;
                ++rep.checks;
                float peak = 0;
                if (b.solo) for (int i = 0; i < 64; ++i) peak = std::max(peak, std::fabs(fw.f32(b.a + 4 * (uint32_t)i)));
                for (int i = 0; i < 64; ++i) {
                    const float x = fw.f32(b.a + 4 * (uint32_t)i);
                    if (b.solo) {                        // XtSoloVoice is not bit-exact (solocmp: −83 to −100 dB)
                        const float e = std::fabs(x - b.p[i]);
                        if (peak > 16.0f * kSoloFloor) worstSolo = std::max(worstSolo, e / peak);
                        if (e > kSoloTol * peak + kSoloFloor) { rep.bad(b.name, "fw %.9g port %.9g", x, b.p[i], blk); break; }
                    } else if (!bits(x, b.p[i])) {
                        rep.bad(b.name, "fw %.9g port %.9g", x, b.p[i], blk);
                        break;
                    }
                }
            }
            // The TVA, bit for bit; the output, Output Assign MFX's bus, the frame's third
            // pair, likewise but for a SOLO wave.  A truncated tone's target and step, which
            // its fade does not read, are the firmware's alone.
            for (FwNote* n : order) {
                if (!n->live || n->ended) continue;
                const XtSynth::Voice& v = syn.voice[n->vi];
                const uint32_t A = Firmware::tva(*n);
                const VField tv[] = { { "tva level", 0xc, v.gain }, { "tva gain", 0x68, v.cur }, { "tva gain r", 0x6c, v.cur }, { "tva target", 0x60, v.target }, { "tva target r", 0x64, v.target }, { "tva step", 0x58, v.step }, { "tva step r", 0x5c, v.step }, { "tva fade", 0x54, v.damping ? v.damp : 1.0f } };
                for (const VField& f : tv) {
                    if (v.damping && (f.off == 0x60 || f.off == 0x64 || f.off == 0x58 || f.off == 0x5c)) continue;
                    ++rep.checks;
                    const float x = fw.f32(A + f.off);
                    if (!bits(x, f.v)) rep.bad(f.name, "fw %.9g port %.9g", x, f.v, blk);
                }
            }
            ++rep.checks;
            float outPeak = 0;
            if (soloCase) for (int i = 0; i < 64; ++i) outPeak = std::max(outPeak, std::fabs(i & 1 ? R[i / 2] : L[i / 2]));
            for (int i = 0; i < 32; ++i) {
                const float a = fw.f32(kBuf + 0x20 * (uint32_t)i + 0x10) * 0x1p-31f, a2 = fw.f32(kBuf + 0x20 * (uint32_t)i + 0x14) * 0x1p-31f;
                if (soloCase) {
                    const float e = std::max(std::fabs(a - L[i]), std::fabs(a2 - R[i]));
                    if (e > kSoloTol * outPeak + kSoloFloor * 0x1p-31f) { rep.bad("output", "fw %.9g port %.9g", a, L[i], blk); break; }
                } else if (!bits(a * 0x1p31f, L[i] * 0x1p31f) || !bits(a2 * 0x1p31f, R[i] * 0x1p31f)) { rep.bad("output", "fw %.9g port %.9g", a, L[i], blk); break; }
            }
        }
        for (int i = 0; i < 2; ++i)
            if (keyDown[i]) {
                int back = -1;
                if (!fw.keyOff(raw[i], back)) return 1;
                syn.noteOff(raw[i], (float)relVel[i] / 127.0f);
            }
        syn.allNotesOff();
        for (FwNote& n : fn) if (n.live && !fw.stop(n)) return 1;
        // FM's lines outlast a note and a modulator past full scale reads the next tone's, so
        // they must agree after a case.  A SOLO wave's are only close, and a note that left
        // memory stopped a block before XtSynth's: a later case reading those has no tolerance
        // to take them in.  So each case starts from the port's.
        if (!soloCase && !leftOff) {
            ++rep.checks;
            for (uint32_t i = 0; i < 2 * XtModShared::kTones * XtModShared::kLine; ++i) {
                const float a = fw.f32(kFmLines + 4 * i), b = syn.modShared.line[i];
                if (!bits(a, b)) { rep.bad("fm lines", "fw %.9g port %.9g", a, b, nBlocks); break; }
            }
        }
        fw.c.write(kFmLines, syn.modShared.line, sizeof(syn.modShared.line));
    }
    summary("tone, a second update in a tick");
    return total ? 3 : 0;
}
#endif
