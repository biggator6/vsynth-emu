#pragma once
// xt_msm, XtMsm's firmware check (XT_MSM_MAIN only): the firmware's own Multi Step Modulator (0x89cf7634) in the
// interpreter and the port, given the same events (settings, note-ons, tempo ticks, the tone task's clocks, the
// arpeggio's flag); the object, its timer and the tone updates it asks for compared after each, and the terms the
// converters read (XtMsmCheck.cpp).  The cases, the presets' and random ones, and main: XtMsmCases.cpp.
#ifdef XT_MSM_MAIN
#include "XtFwMachine.h"
#include "XtMsm.h"
#include <cstdint>
#include <random>
#include <string>
#include <vector>

// What both sides are given, one at a time; each kind's handling on the two sides is one entry of XtMsmCheck.cpp's table
struct XtMsmEvent {
    enum Kind { kLoad, kEdit, kNoteOn, kTick, kClocks, kArpeggio, kKinds };
    Kind kind = kTick;
    XtMsmSettings settings;   // kLoad, kEdit
    int part = 0;             // kLoad, kEdit, kNoteOn
    bool flag = false;        // kNoteOn: a key of part 0 held besides; kClocks: a note sounding; kArpeggio: it plays
    int32_t ticks = 0;        // kTick
};

// The firmware's object, its routines called as the tone task, the note allocator and the arpeggio call them
struct XtMsmFw {
    static constexpr uint32_t kObj = 0x89cf7634, kNow = 0x898586f0;
    explicit XtMsmFw(const std::string& rom);
    bool ok() const { return m.ok && built; }
    void begin();                                  // the OS's timers empty at 0, a new object (0x894d9820, 0x894d9580)
    // The loaders read this patch's store by the firmware's own pointing routine and getter (the presets'), where the
    // port is given xtMsmSettingsOf it; with none, the getter answers from the event's settings (the random cases')
    void readStore(const std::vector<uint8_t>* store);
    uint32_t call(uint32_t addr, std::initializer_list<uint32_t> args);
    int term(int dest, int part, int zone);        // 0x894d96d0
    int termAnyZone(int dest, int part);           // 0x894d9750
    int termAll(int dest);                         // 0x894d97b0
    int shownStep(int n);                          // 0x894d9870
    uint8_t rd8(uint32_t off) { return m.cpu.rd8(kObj + off); }
    uint32_t rd32(uint32_t off) { return m.cpu.rd32(kObj + off); }
    int32_t now() { return (int32_t)m.cpu.rd32(kNow); }

    xtfw::Machine m;
    XtMsmSettings given;                           // what the getter answers by ID
    bool sounding = false, keyHeld = false;        // what 0x894d55a0 and 0x894d5550 answer
    bool fromStore = false;
    int updates = 0, posts = 0;                    // 0x894d72f0's calls, the timer's messages (0x893018a0)
    std::string failed;                            // a call that faulted or ran out of steps
private:
    bool built = false;
    uint32_t entryPr = 0;
};

// The two sides and what's been compared
struct XtMsmCheck {
    XtMsmFw& fw;
    XtMsm port;
    long events = 0, states = 0, terms = 0, different = 0;
    int fires = 0;                                 // the port's timer, as the firmware's messages count it
    bool first = false;                            // XT_MSM_FIRST: say what differs
    std::string where;                             // the case, for what's said
    explicit XtMsmCheck(XtMsmFw& f) : fw(f) {}
    void begin();
    void run(const XtMsmEvent& e);                 // on both, then the two compared
    void termsAlike(std::mt19937& r);              // each track's own terms, a drawn one, and the steps shown
    void sweep();                                  // every destination and zone of part 0
private:
    int said = 0;
    void compare();
    void same(const char* what, long fwValue, long portValue);
};
#endif
