#pragma once
// xt_arp, XtArpeggio's firmware check (XT_ARP_MAIN only): the firmware's own arpeggio object (0x8987cef8) in the
// interpreter and the port, given the same channel, settings, pattern, keys, pedal and tempo ticks, their messages
// compared one by one and the object's fields after each step (XtArpeggioCheck.cpp).  The cases, the presets' and random
// ones, and main: XtArpeggioCases.cpp.
#ifdef XT_ARP_MAIN
#include "XtArpeggio.h"
#include "XtFwMachine.h"
#include <cstdint>
#include <string>
#include <vector>

// The firmware's object, its routines called as 0x8930c190, the router and the task call them
class XtArpFw {
public:
    explicit XtArpFw(const std::string& rom);
    bool ok() const { return m.ok && built; }
    void begin();                                  // the OS's timer lists empty, the tempo timer at 0, a new object
    void channel(int ch);                          // +0xd2d, then 0x89304920(obj, 1)
    void settings(const XtArpSettings& s);         // 0x8930c190's part for the main object
    void pattern(const XtArpPattern& p);           // 0x8930ca30's buffer, at +0xb8
    void noteOn(int key, int vel);
    void noteOff(int key);
    void pedal(int value);
    void stop();
    void tick(int32_t ticks);                      // 0x8930eb00, and the task's step on flag 0x200
    uint32_t rd(uint32_t off, int bytes);          // the object's field at `off`
    uint32_t global(uint32_t addr) { return m.cpu.rd32(addr); }
    std::vector<XtArpEvent> sent;                  // the messages it posted (0x89472e70)
    std::string failed;                            // a call that faulted or ran out of steps
private:
    xtfw::Machine m;
    bool built = false, stepFlag = false;
    void call(uint32_t addr, std::initializer_list<uint32_t> args);
};

// The two sides and what's been compared
struct XtArpCheck {
    XtArpFw& fw;
    XtArpeggio port;
    long events = 0, states = 0, different = 0, ops = 0;
    long byMotif[8] = {};
    bool first = false;                            // XT_ARP_FIRST: say what differs
    std::string where;                             // the case, for what's said
    explicit XtArpCheck(XtArpFw& f) : fw(f) {}
    void begin(int ch, const XtArpSettings& s, const XtArpPattern& p);
    void settings(const XtArpSettings& s);
    void pattern(const XtArpPattern& p);
    void noteOn(int key, int vel);
    void noteOff(int key);
    void pedal(int value);
    void stop();
    void tick(int32_t ticks);
private:
    void compare(bool fields);
    void scalarsAlike();
    void nodesAlike();
    void same(const char* name, uint32_t off, int bytes, long want);              // a field of `bytes`
    template <class T>
    void sameAll(const char* name, uint32_t off, int bytes, const T* want, int n) {   // a run of them
        for (int i = 0; i < n; ++i) same(name, off + (uint32_t)(i * bytes), bytes, (long)want[i]);
    }
    void link(const char* name, uint32_t off, uint32_t nodes, long want);         // a pointer, as the node it points at
    void differs(const char* what, long fwValue, long portValue);
};

// The tempo ticks the arpeggio steps on: XtTempoTicks against the clock object's routines (XtTempoTicksCheck.cpp)
struct XtTicksTally {
    long cases = 0, ms = 0, values = 0, different = 0;
    bool failed = false;                           // a routine faulted or ran out of steps
};
XtTicksTally xtTempoTicksCheck(const std::string& rom, long cases, unsigned seed, bool first);
#endif
