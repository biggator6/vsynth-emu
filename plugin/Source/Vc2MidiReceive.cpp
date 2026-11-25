// rom.4's MIDI receive objects (Vc2MidiReceive.h, v355).
#include "Vc2MidiReceive.h"
#include <cstring>

namespace {
// [FW] the parser's: a status byte from 0x80, the system's from 0xf0; the length table 0x8950cff8's for the channel
// statuses (3 bytes but 0xc0's and 0xd0's 2), its running-status flag 0x10 set for them all; a status byte's phase 4
constexpr uint8_t kStatusFrom = 0x80, kSystemFrom = 0xf0, kProgram = 0xc0, kPressure = 0xd0, kClassBits = 0xf0;
constexpr uint8_t kPhase = 4;
// [FW] the marker's status, a reset's (0x89312338: 0xfd to 0x70)
constexpr uint8_t kMarkerStatus = 0xfd, kReset = 0x70;

uint32_t after(const Vc2MidiReceiveWords& w, uint32_t a) { return a + 4 >= w.end ? w.start : a + 4; }

uint32_t& slot(Vc2MidiReceive& r, uint32_t a) { return r.ring[(a - r.w.start) / 4]; }

uint8_t lengthOf(uint8_t status) {
    const uint8_t cls = status & kClassBits;
    return cls == kProgram || cls == kPressure ? 2 : 3;
}

void giveTask(Vc2MidiReceive& r, bool fromTask, Vc2MidiReceiveParts& parts) {
    if (fromTask) parts.post(r.w.task, r.w.event);
    else parts.raise(r.w.task, r.w.event);
}

// The built message to the ring while the object is on (0x89312540…0x89312576), or the marker with the ring full;
// whether the parser goes on (not after a marker: 0x89312576 ends it there)
bool put(Vc2MidiReceive& r, Vc2MidiReceiveParts& parts) {
    if (r.w.enabled == 0) return true;
    const uint32_t a = after(r.w, r.w.write);
    if (a == r.w.read) {
        vc2ReceiveMarker(r, 2, false, parts);
        return false;
    }
    std::memcpy(&slot(r, a), r.w.built.data(), 4);
    r.w.write = a;
    giveTask(r, false, parts);
    return true;
}
} // namespace

void vc2ReceiveBuild(Vc2MidiReceive& r, uint32_t at, uint8_t source, uint32_t task, uint16_t event, uint32_t klass) {
    r = Vc2MidiReceive {};
    r.w.source = source;
    r.w.task = task;
    r.w.event = event;
    r.w.start = r.w.read = r.w.write = at + (uint32_t)sizeof(Vc2MidiReceiveWords);
    r.w.end = r.w.start + Vc2MidiReceive::kRing * 4;
    r.w.sysex = r.w.sysexAt = r.w.end;
    r.w.sysexBytes = Vc2MidiReceive::kSysexBytes;
    r.w.klass = klass;
}

void vc2ReceiveByte(Vc2MidiReceive& r, uint8_t byte, Vc2MidiReceiveParts& parts) {
    r.w.mark = 1;
    if (byte >= kSystemFrom) return;   // not ported: SysEx, system common, real-time
    if (byte >= kStatusFrom) {   // 0x89312450: a status; its length; on to store it as a data byte
        r.w.count = 0;
        r.w.running = byte;
        r.w.built[3] = r.w.source;
        r.w.length = lengthOf(byte);
    } else if (r.w.running == 0) {
        return;   // a data byte with no status
    }
    r.w.phase = kPhase;   // 0x89312514; the class's +0x18 (a bare return) not called
    r.w.built[r.w.count] = byte;
    ++r.w.count;
    if (r.w.count < r.w.length || !put(r, parts)) return;
    r.w.count = 1;   // the running status kept: its flag 0x10
}

void vc2ReceiveMarker(Vc2MidiReceive& r, uint8_t code, bool fromTask, Vc2MidiReceiveParts& parts) {
    if (r.w.enabled == 0) return;
    uint32_t a = after(r.w, r.w.write);
    while (a == r.w.read) a = after(r.w, a);
    auto* bytes = reinterpret_cast<uint8_t*>(&slot(r, a));
    bytes[0] = kMarkerStatus;
    bytes[1] = code;
    bytes[3] = r.w.source;
    r.w.write = a;
    r.w.running = r.w.sensing = r.w.mark = 0;
    giveTask(r, fromTask, parts);
}

bool vc2ReceiveTake(Vc2MidiReceive& r, uint32_t& message, uint32_t& word) {
    if (r.w.read == r.w.write) return false;
    r.w.read = after(r.w, r.w.read);
    message = slot(r, r.w.read);
    if ((uint8_t)message == kMarkerStatus) {
        message = (message & ~0xffu) | kReset;
        word &= ~0xffu;
    }
    return true;
}

void vc2ReceiveSensing(Vc2MidiReceive& r, Vc2MidiReceiveParts& parts) {
    if (r.w.mark != 0) {
        r.w.mark = 0;
        return;
    }
    if (r.w.sensing != 0) vc2ReceiveMarker(r, 1, false, parts);
}
