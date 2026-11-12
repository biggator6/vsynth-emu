#pragma once
// The firmware's Multi Step Modulator (0x89cf7634) beside the firmware's tone, for xt_tone's MSM mode (XtToneMsm.cpp,
// XT_TONE_MAIN only, v193): its object built as at boot, its settings read through the getter from the patch, the
// allocator's head, its timer on the OS's tempo ticks and the tone task's flag.  The tone updates it asks for (0x894d72f0)
// are taken as the object stood then, and given to the notes once the call that asked has returned (the interpreter's
// calls don't nest).  HARDWARE_FIRMWARE.md, "The Multi Step Modulator".
#include <cstdio>
#include <vector>
#include "XtToneFirmware.h"

constexpr uint32_t kFwMsmObj = 0x89cf7634, kFwMsmBytes = 0xb0;

struct FwMsm {
    static constexpr int kNotes = 2;
    // the OS's tempo timer: its list, count and next; the tone task's queue and the MSM's flag
    static constexpr uint32_t kTimerGlobals[3] = { 0x898586ec, 0x898586f0, 0x898586f4 };
    static constexpr uint32_t kQueue = 0x8987cda8, kFlag = 0x20;
    static constexpr uint32_t kOwner = 0x89cf76e4, kPartByte = kRam + 0x1580;
    static constexpr uint32_t kCtor = 0x894d9820, kInit = 0x894d9580, kLoad = 0x894d9950, kEdit = 0x894d9b20;
    static constexpr uint32_t kHead = 0x894d4b08, kHeadEnd = 0x894d4bd4, kTick = 0x8930eb00, kClocks = 0x894d4240;
    // the tone update it asks for, on part 0's list of notes (ours aren't on it): 0x894d72f0, which start and the clocks reach
    // through 0x894d96b0 and stop calls itself
    static constexpr uint32_t kPoint = 0x89479b70, kPost = 0x893018a0, kUpdate = 0x894d72f0;
    static constexpr uint32_t kHeld = 0x894d5550, kSounding = 0x894d55a0;

    Firmware& fw;
    FwNote* notes;
    std::vector<std::vector<uint8_t>> asked;
    long posts = 0;
    long updates = 0;
    uint32_t entryPr = 0;

    // A note on the allocator's list (not off, not ended) and not truncated (+0x2c)
    bool listed(const FwNote& n) const { return n.live && !n.off && !n.ended && fw.c.rd8(n.note + 0x2c) == 0; }
    // 0x894d5550(0): one whose key is down (its state word's bit 29); 0x894d55a0(0): any
    bool held() const {
        for (int i = 0; i < kNotes; ++i)
            if (listed(notes[i]) && (fw.c.rd32(fw.c.rd32(notes[i].note + 0x1c)) & 0x20000000)) return true;
        return false;
    }
    bool sounding() const {
        for (int i = 0; i < kNotes; ++i)
            if (listed(notes[i])) return true;
        return false;
    }

    void hook() {
        Sh4& c = fw.c;
        c.hook(kPost, [this](Sh4& m) {
            if (m.r[4] == kQueue && m.r[5] == kFlag) ++posts;
            m.ret(0);
        });
        c.hook(kUpdate, [this](Sh4& m) {
            std::vector<uint8_t> b(kFwMsmBytes);
            m.read(kFwMsmObj, b.data(), kFwMsmBytes);
            asked.push_back(std::move(b));
            m.ret(0);
        });
        c.hook(kHeld, [this](Sh4& m) { m.ret(held() ? 1 : 0); });
        c.hook(kSounding, [this](Sh4& m) { m.ret(sounding() ? 1 : 0); });
        c.hook(kPoint, [](Sh4& m) {
            m.wr32(m.r[4] + 4, 1);                                  // a block there: the getter is asked, from the patch
            m.ret(m.r[4]);
        });
        c.hook(kHead, [this](Sh4& m) { entryPr = m.pr; });
        c.hook(kHeadEnd, [this](Sh4& m) { m.pc = entryPr; });
    }

    bool call(uint32_t at, std::initializer_list<uint32_t> args) {
        if (fw.c.call(at, args, kSp)) return true;
        std::fprintf(stderr, "msm %08x: %s\n", at, fw.c.error().c_str());
        return false;
    }

    // The updates asked for, each on every sounding note with the object as it stood (0x894d50d0 → 0x894dc0b0)
    bool replay() {
        if (asked.empty()) return true;
        std::vector<uint8_t> now(kFwMsmBytes);
        fw.c.read(kFwMsmObj, now.data(), kFwMsmBytes);
        for (const std::vector<uint8_t>& b : asked) {
            ++updates;
            fw.c.write(kFwMsmObj, b.data(), kFwMsmBytes);
            for (int i = 0; i < kNotes; ++i)
                if (notes[i].live && !notes[i].ended && !fw.liveUpdate(notes[i])) return false;
        }
        fw.c.write(kFwMsmObj, now.data(), kFwMsmBytes);
        asked.clear();
        return true;
    }

    // As at boot: the timer's globals, the constructor, the tone task's queue (0x894d9580)
    bool begin() {
        hook();
        for (const uint32_t a : kTimerGlobals) fw.c.wr32(a, 0);
        const std::vector<uint8_t> zero(kFwMsmBytes, 0);
        fw.c.write(kFwMsmObj, zero.data(), kFwMsmBytes);
        return call(kCtor, {}) && call(kInit, { kFwMsmObj, kQueue, kFlag });
    }
    // A patch (0x894d9950) or an edit of the section (0x894d9b20), part 0's
    bool settings(bool patch) {
        const uint32_t at = patch ? kLoad : kEdit;
        return call(at, { kOwner, 0 }) && replay();
    }
    bool head() {
        fw.c.wr8(kPartByte, 0);
        return call(kHead, { 0, kPartByte, 0, 0 }) && replay();
    }
    // A ms of tempo ticks (0x8930eb00), then the tone task's flag 0x20 if the timer posted it (0x894d4240)
    bool ms(int32_t d) {
        if (d != 0 && !call(kTick, { (uint32_t)d })) return false;
        if (posts == 0) return true;
        posts = 0;
        return call(kClocks, { kFwMsmObj }) && replay();
    }
};
