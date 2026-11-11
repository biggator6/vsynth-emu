#pragma once
// The V-Synth XT's arpeggiator (v188): the firmware's arpeggio object (constructor 0x89304960, class 0x893036b0), above the
// tone generator in the realtime layer.  It takes a part's keys, steps through the patch's pattern on the OS's tempo timer
// (480 ticks a quarter; 0x8930eb00) and sends notes and controllers as MIDI messages, which the firmware posts to the tone
// generator's input (0x89472e70 on 0x89c1d518).  HARDWARE_FIRMWARE.md, "The arpeggiator".
//   XtArpeggio.cpp: keys, settings, the timer, the step and what it sends.  XtArpeggioMotifs.cpp: which key a step plays.
// Real-time: no allocation, no locks.  Checked against the firmware's own object by XT_ARP_MAIN (XtArpeggioCheck.cpp).
#include <cstdint>

// The settings the patch gives it, as 0x8930c190 reads them (IDs 0x42e…0x437), copied to the object's +0xac…+0xb4
struct XtArpSettings {
    bool on = false;          // 0x42e Switch: a change stops it (0x89303ab0); the router sends it keys only when on
    int hold = 0;             // 0x42f, +0xac
    int motif = 0;            // 0x430, +0xad: UP, DOWN, UP&DOWN, RANDOM, NOTE-ORDER, RHYTHM, PHRASE, AUTO
    int shuffleRate = 50;     // 0x432, +0xae: 0…100 (the shuffle's table takes 1…99)
    int shuffleRes = 0;       // 0x433, +0xaf: 16TH, 8TH
    int duration = 100;       // 0x431, +0xb0: 0…100
    int grid = 3;             // 0x436, +0xb1: QUARTER, 8TH, 8TH-T, 16TH, 16TH-T, 32TH (480, 240, 160, 120, 80, 60 ticks)
    int endPoint = 31;        // 0x437, +0xb2: 0…31, the pattern's last step
    int octave = 0;           // 0x435, +0xb3: −3…+3
    int keyVelocity = 0;      // 0x434, +0xb4: 0 REAL, 1…127
};

// The pattern as 0x8930ca30 lays it out from the patch (0x89880f83): a line's State and Note/CC, and each step's cell
struct XtArpPattern {
    static constexpr int kLines = 16, kSteps = 32;
    uint8_t line[kLines] = {};          // bit 7 CTRL, else NOTE; the low 7 bits the note or controller (a controller at most 95)
    uint8_t cell[kSteps][kLines] = {};  // 0 REST, 1 TIE, 2…128 velocity (or a controller's value + 1)
};

// What it sends: a MIDI message on its channel (0x90 note-on, 0x80 note-off at velocity 64, 0xb0 controller)
struct XtArpEvent {
    uint8_t status = 0;
    uint8_t d1 = 0;
    uint8_t d2 = 0;
};

// A key's place in the keys held, in the order played (+0x108 + key·12: +0 key, +1 velocity, +4 prev, +8 next)
struct XtArpKeyNode {
    uint8_t key = 0;
    uint8_t vel = 0;          // 0: not in the list
    int16_t prev = -1;
    int16_t next = -1;
};

// A sounding note's end, in the list by time (+0x728 + note·12: +0 channel, +4 time, +8 next)
struct XtArpOffNode {
    int8_t ch = -1;
    int32_t time = 0x7fffffff;
    int16_t next = -1;
};

class XtArpeggio {
public:
    static constexpr int kKeys = 128, kShuffle = 120, kMaxEvents = 1024;
    static constexpr int32_t kNever = 0x7fffffff, kFiring = 0x7ffffffe;

    XtArpeggio();                                  // the constructor's state (0x89304960)
    void reset();                                  // back to it
    void setChannel(int ch) { channel = (int8_t)ch; }   // +0xbc, from +0xd2d (0x89304920); −1 sends nothing
    void setSettings(const XtArpSettings& s);      // 0x8930c190's part for this object
    void setPattern(const XtArpPattern& p) { pattern = p; }
    void noteOn(int key, int velocity);            // 0x89303830
    void noteOff(int key);                         // 0x893039c0
    void pedal(int value);                         // 0x89303a80: the Hold pedal (CC 64)
    void stop();                                   // 0x89303ab0: every note off, the keys let go, the controllers reset
    // The OS's tempo timer moved on by `ticks` (0x8930eb00): it fires at the first tick at or past its time, and the
    // task's flag 0x200 runs a step (0x8930d54c) before the next.
    void tick(int32_t ticks);
    bool running() const { return next < kNever; }

    // The messages sent since the last clearEvents (none are lost below kMaxEvents)
    int eventCount() const { return nEvents; }
    const XtArpEvent* events() const { return eventBuf; }
    void clearEvents() { nEvents = 0; }

    // The object's fields (offsets in the firmware's), public for its check
    int32_t pos = 0;                          // +0x1c: ticks since the start
    int32_t base = 0;                         // +0x20: the tempo timer's count at the start
    uint8_t shuffleRate = 0;                  // +0x30, +0x31: what the shuffle table was made for
    uint8_t shuffleRes = 0;
    uint8_t shuffle[kShuffle] = {};           // +0x32: where in a 480-tick pair each 4-tick place falls, /4
    uint8_t timerMode = 1;                    // +0xaa: 0 once on the tempo group (0x89304650)
    uint8_t hold = 0;                         // +0xac…+0xb4, the settings
    uint8_t motif = 0;
    uint8_t rate = 50;
    uint8_t res = 0;
    uint8_t duration = 100;
    uint8_t grid = 0x78;
    uint8_t endPoint = 0x20;
    int8_t octaveRange = 0;
    uint8_t keyVelocity = 0;
    int8_t channel = 0;                       // +0xbc
    int8_t lastNote = -1;                     // +0xbe
    uint8_t lastVelocity = 0;                 // +0xbf
    int8_t octave = 0;                        // +0xc0
    int8_t direction = 1;                     // +0xc1
    uint16_t octaveSeed = 0x7ab7;             // +0xc6
    uint8_t pedalValue = 0;                   // +0xce
    uint16_t held[8] = {};                    // +0xd0: the keys down
    uint8_t step = 0;                         // +0xe0
    uint8_t line = 0;                         // +0xe1
    uint8_t started = 0;                      // +0xe2
    uint8_t lineCc[16] = {};                  // +0xe4: the controller each CTRL line last sent
    uint8_t cc64 = 0;                         // +0xf4, +0xf5: a CTRL line sent CC 64 or 66 above 0
    uint8_t cc66 = 0;
    uint8_t octavePending = 0;                // +0xf6
    int32_t next = kNever;                    // +0xfc: the next step's time
    int32_t nominal = 0;                      // +0x100: the step's own, which its notes' ends count from
    int16_t keyHead = -1;                     // +0x104: the keys in the order played (circular)
    XtArpKeyNode keys[kKeys];
    uint16_t sounding[8] = {};                // +0x708: the notes sounding
    uint8_t curKey = 0x80;                    // +0x718
    uint8_t soundingCount = 0;                // +0x719
    int16_t curNode = -1;                     // +0x71c
    uint16_t keySeed = 0x7ab7;                // +0x720: RANDOM's
    int16_t offHead = -1;                     // +0x724: the notes sounding, by when they end
    XtArpOffNode offs[kKeys];
    int32_t due = kNever;                     // the timer's +8 (kFiring once fired, until set again) and +0xc
    int32_t fired = 0;
    int32_t now = 0;                          // the tempo timer's count (0x898586f0)
    bool on = false;                          // +0xd2c
    XtArpPattern pattern;                     // what +0xf8 points at

    // XtArpeggioMotifs.cpp: the line a step is on, played by the motif (0x893041e0), and a note sent (0x89302840)
    void playLine();
    void playNote(int note, int velocity);

private:
    XtArpEvent eventBuf[kMaxEvents];
    int nEvents = 0;
    void send(int status, int d1, int d2);
    void makeShuffle(int r, int s);           // 0x89304430
    void pedalCheck();                        // 0x89303380
    void start();                             // 0x89302400
    void halt();                              // 0x893024e0
    void restart(int32_t delay);              // 0x89303480
    void schedule(int32_t t);                 // 0x8930ea10
    void runStep();                           // 0x89302570
    void ccLine();                            // 0x89302ba0
    void resetControllers();                  // 0x89303b40
    void unlinkKey(int k);                    // 0x893040c0
    void linkKey(int k);                      // 0x893040a0
    void linkOff(int n);                      // 0x893040f0
    void unlinkOff(int n);                    // 0x89304140
    void endNote(int n);                      // 0x89302b00
    void dropKeys();
    bool anyHeld() const;
};

// 0x89301e90: a step's ticks by the Grid
int xtArpGridTicks(int grid);

// A map of the 128 keys or notes, bit k & 15 of word k >> 4 (+0xd0, +0x708)
inline bool xtArpBit(const uint16_t* map, int k) {
    return (map[(k & 127) >> 4] >> (k & 15)) & 1;
}
inline void xtArpSetBit(uint16_t* map, int k) {
    map[(k & 127) >> 4] |= (uint16_t)(1u << (k & 15));
}
inline void xtArpClearBit(uint16_t* map, int k) {
    map[(k & 127) >> 4] &= (uint16_t)~(1u << (k & 15));
}
