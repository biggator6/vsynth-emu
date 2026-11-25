#pragma once
// rom.4's MIDI receive objects (v355), word for word: MIDI IN's (0x8995662c, row 15), USB MIDI's (0x899598b0, row 18)
// and a second USB port's (0x899543e8, row 14), 0x2244 bytes each: the parser's bytes, the task its ring posts and the
// event, then the ring of 2048 received messages (each a word: status, data 1, data 2, source) and a SysEx buffer.
// The parser (0x893123b0, a byte as the receive interrupt gives it): a byte marks the object (+0xc); a channel status
// sets the running status, the source and the length (the table 0x8950cff8's low bits), data bytes follow it, and at
// the length the message goes to the ring, the task given its event (an event set from an interrupt); a message in a
// full ring makes the marker instead. The marker (0x89311e90; 0x89311f40, the same from a task): 0xfd, its code and
// the source at write + 4, or, the ring full, at the slot after the read, which drops every message waiting; the
// running status, sensing and the mark cleared. The task's take (0x893122d0): the next message, a marker as the reset
// (0x70). The sensing check (0x893125e0, the input task's cyclic handler): the mark cleared, else, sensing seen, the
// marker 1. The ring is the plugin's MIDI waiting for the input task. SysEx, system common and real-time bytes aren't
// ported (none reaches it yet). HARDWARE_FIRMWARE.md, "The part's tasks on the kernel".
#include <array>
#include <cstdint>

struct Vc2MidiReceiveWords {
    uint32_t enabled = 0;               // +0x00: set by the MIDI on (0x8931a590)
    std::array<uint8_t, 4> built {};    // +0x04: the message being built: status, data 1, data 2, source
    uint8_t length = 0;                 // +0x08: its bytes
    uint8_t count = 0;                  // +0x09: those so far
    uint8_t sensing = 0;                // +0x0a: an active sensing (0xfe) seen
    uint8_t unread0b = 0;
    uint8_t mark = 0;                   // +0x0c: a byte since the sensing check's last
    uint8_t running = 0;                // +0x0d: the running status
    uint8_t source = 0;                 // +0x0e
    uint8_t phase = 0;                  // +0x0f: 4 once a status byte's way is taken
    uint32_t task = 0;                  // +0x10: the task the ring gives its event
    uint16_t event = 0;                 // +0x14
    uint16_t pad16 = 0;
    uint32_t start = 0, end = 0, read = 0, write = 0;   // +0x18: the ring, by address, as a queue's
    uint32_t unread28 = 0;              // +0x28
    uint32_t sysex = 0, sysexBytes = 0; // +0x2c, +0x30: the SysEx buffer and its size
    uint8_t inSysex = 0;                // +0x34
    std::array<uint8_t, 3> pad35 {};
    uint32_t sysexAt = 0;               // +0x38
    uint32_t klass = 0;                 // +0x3c
};
static_assert(sizeof(Vc2MidiReceiveWords) == 0x40, "rom.4's layout, a receive object's words");

struct Vc2MidiReceive {
    static constexpr uint32_t kRing = 0x800, kSysexBytes = 0x200;   // [FW] 0x89312260's sizes, row 15's
    Vc2MidiReceiveWords w {};
    std::array<uint32_t, kRing> ring {};
    std::array<uint8_t, kSysexBytes> sysex {};
    uint32_t tail = 0;
};
static_assert(sizeof(Vc2MidiReceive) == 0x2244, "rom.4's layout, a receive object");

// What the receive object gives its task: an event set from an interrupt (0x89301840) or from a task (0x89301830)
struct Vc2MidiReceiveParts {
    virtual ~Vc2MidiReceiveParts() = default;
    virtual void raise(uint32_t task, uint32_t event) = 0;
    virtual void post(uint32_t task, uint32_t event) = 0;
};

// The object built as its row builds it (0x89312260 on it): at `at`, its source, task, event and class, its ring empty
void vc2ReceiveBuild(Vc2MidiReceive& r, uint32_t at, uint8_t source, uint32_t task, uint16_t event, uint32_t klass);
void vc2ReceiveByte(Vc2MidiReceive& r, uint8_t byte, Vc2MidiReceiveParts& parts);      // 0x893123b0's channel way
void vc2ReceiveMarker(Vc2MidiReceive& r, uint8_t code, bool fromTask, Vc2MidiReceiveParts& parts);
// The task's take: false when the ring is empty; a marker as the reset, its word's first byte 0
bool vc2ReceiveTake(Vc2MidiReceive& r, uint32_t& message, uint32_t& word);
void vc2ReceiveSensing(Vc2MidiReceive& r, Vc2MidiReceiveParts& parts);                 // 0x893125e0
