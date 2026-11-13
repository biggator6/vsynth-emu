// xt_tone's firmware side (XtToneFirmware.h): its power-on.  Moved from XtToneCheck.cpp as it stood (v193).
#ifdef XT_TONE_MAIN
#include "XtToneFirmware.h"

bool Firmware::open(const char* path) {
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
#endif
