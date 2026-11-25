// The XT's MIDI input path on the part's OS model (Vc2SynthOsMidi.h, v355): its memory built as rom.4's rows build it,
// the tasks' prologues' parts, the front's MIDI on, the input task's settings, and the outside's bytes and messages.
// Every address and class is rom.4's.
#include "Vc2SynthOs.h"
#include "Vc2SynthOsMidi.h"
#include "Vc2Synth.h"
#include "XtRing.h"

namespace {
using Path = Vc2MidiPath;
// [FW] the tasks the receive objects give their events (the MIDI input task; entry 5's for the second USB port), the
// events, sources and classes rows 14, 15 and 18 give them
constexpr uint32_t kInputTcb = 0x8994fd90u, kUsb2Tcb = 0x8994fe60u, kOsMidiTcb = 0x8994fe94u;
constexpr uint32_t kMidiInClass = 0x898c809cu, kUsbClass = 0x898c81a4u, kUsb2Class = 0x898c7e20u;
constexpr uint8_t kMidiInSource = 1, kUsbSource = 2, kUsb2Source = 6;
constexpr uint16_t kMidiInEvent = 2, kUsbEvent = 4, kUsb2Event = 0x400;
// [FW] the listeners' classes (rows 17, 21, 22, the notes' row), the hub's (row 30: its node's, its own)
constexpr uint32_t kOutClass = 0x898c8114u, kClockClass = 0x898c82b8u;
constexpr uint32_t kOsMidiClass = 0x898c8318u, kOsMidiHandler = 0x898c8348u;
constexpr uint32_t kNotesClass = 0x898cd13cu, kNotesHandler = 0x898cd16cu;
constexpr uint32_t kHubNodeClass = 0x898c82f4u, kHubClass = 0x898c82dcu;
// [FW] the input task's cyclic records (its prologue's, from 0x8992e24c + 0x1c and + 0x20): the sensing check every
// 512 ticks (0x89311330, the port's 1), MIDI out A's flush every 256 (0x893115e0, 2: the MIDI out's)
constexpr int kSensingRecord = 7, kSensingHandler = 1, kFlushRecord = 8, kFlushHandler = 2;
constexpr uint16_t kOn = 1;
constexpr uint32_t kSensingTicks = 0x200, kFlushTicks = 0x100;
// [FW] the settings' (0x89311ca0): USB-MIDI Thru and the dummy ID after it, System B's; the marker's code; thru's
constexpr int16_t kUsbThru = 0x7d, kUsbDummy = 0x80;
constexpr uint8_t kSettingsMarker = 4, kThruOn = 2, kThruOff = 3;
constexpr uint32_t kFrontEvent = 1;   // the front's stand-in's event
} // namespace

void Vc2MidiPath::buildPart() {
    vc2QueueBuild(notes, kNotesAt, kNotesClass, 0, kNotesHandler);
    clock = Vc2RingListener { {}, kClockClass };
    words = Vc2MidiPathWords {};
    words.active = kMidiInAt;   // .data
}

void Vc2MidiPath::buildOs() {
    vc2ReceiveBuild(usb2, kUsb2At, kUsb2Source, kUsb2Tcb, kUsb2Event, kUsb2Class);   // row 14
    vc2ReceiveBuild(midiIn, kMidiInAt, kMidiInSource, kInputTcb, kMidiInEvent, kMidiInClass);   // row 15
    ring = 0;                                                                        // row 17
    out = Vc2RingListener { {}, kOutClass };
    vc2ReceiveBuild(usb, kUsbAt, kUsbSource, kInputTcb, kUsbEvent, kUsbClass);       // row 18
    vc2QueueBuild(osMidi, kOsMidiAt, kOsMidiClass, kOsMidiTcb, kOsMidiHandler);       // row 22
    hub = Vc2MidiHub {};                                                             // row 30
    hub.nodeClass = kHubNodeClass;
    hub.klass = kHubClass;
}

namespace {
// `at` on the hub's ring (0x893111b0), at its end
void hubLink(Vc2Synth& s, uint32_t at) {
    xtRingLink(s.midiPath.hub.head, 0u, at, [&s](uint32_t a) -> Vc2RingNode& { return s.hubNode(a); });
}
} // namespace

void Vc2SynthOsMidi::inputStart() {   // 0x89311970
    hubLink(s_, Path::kOutAt);
    os_.kernel.cyclicMake(kSensingRecord, kSensingHandler, kOn, kSensingTicks);
    os_.kernel.cyclicMake(kFlushRecord, kFlushHandler, kOn, kFlushTicks);
}

void Vc2SynthOsMidi::clockStart() { hubLink(s_, Path::kClockAt); }   // 0x89313c1e

void Vc2SynthOsMidi::osMidiStart() { hubLink(s_, Path::kOsMidiAt); }   // 0x89314cfa

void Vc2SynthOsMidi::notesStart() { hubLink(s_, Path::kNotesAt); }   // 0x8938fc40

void Vc2SynthOsMidi::midiOn() {   // 0x8931a590
    Path& p = s_.midiPath;
    p.usb2.w.enabled = p.midiIn.w.enabled = p.usb.w.enabled = 1;
    if (p.hub.linked != 0) return;
    p.hub.linked = 1;
    xtRingLink(p.ring, 0u, Path::kHubNodeAt, [this](uint32_t a) -> Vc2RingNode& { return s_.midiInNode(a); });
}

// The input task's 8 (0x89311ca0): the sensing check on the second USB port's object (0x89311430(1)), the one it watched
// given the marker 4 the first time; USB-MIDI Thru, a change of it a marker 4 to both USB ports; the dummy ID's flag.
// Then 0x89311d10's, System's 0x9e to MIDI out A's +6, the MIDI out's
void Vc2SynthOsMidi::settings() {
    Path& p = s_.midiPath;
    if (p.words.active != Path::kUsb2At) {
        vc2ReceiveMarker(receiveAt(p.words.active), kSettingsMarker, true, *this);
        p.words.active = Path::kUsb2At;
    }
    const uint8_t thru = s_.systemBValue(kUsbThru) != 0 ? kThruOn : kThruOff;
    if (thru != p.words.thru) {
        p.words.thru = thru;
        vc2ReceiveMarker(p.usb2, kSettingsMarker, true, *this);
        vc2ReceiveMarker(p.usb, kSettingsMarker, true, *this);
    }
    const uint8_t flag = s_.systemBValue(kUsbDummy) != 0 ? 1 : 0;
    if (flag != p.words.usbFlag) p.words.usbFlag = flag;
}

void Vc2SynthOsMidi::receiveByte(int source, uint8_t byte) {
    vc2ReceiveByte(source == kUsbSource ? s_.midiPath.usb : s_.midiPath.midiIn, byte, *this);
}

bool Vc2SynthOsMidi::give(const Vc2MidiMessage& m, uint32_t word) {
    if (frontCount_ == kFrontQueue) {
        ++frontDrops;
        return false;
    }
    frontQueue_[(frontHead_ + frontCount_) % kFrontQueue] = { m, word };
    ++frontCount_;
    os_.raise(Vc2SynthOs::kObjects[Vc2SynthOs::kFront], kFrontEvent);
    return true;
}

void Vc2SynthOsMidi::sensing() { vc2ReceiveSensing(receiveAt(s_.midiPath.words.active), *this); }

void Vc2SynthOsMidi::raise(uint32_t task, uint32_t event) { os_.raise(task, event); }

void Vc2SynthOsMidi::post(uint32_t task, uint32_t event) { os_.post(task, event); }
