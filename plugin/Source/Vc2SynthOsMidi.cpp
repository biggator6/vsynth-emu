// The XT's MIDI input path on the part's OS model (Vc2SynthOsMidi.h, v355): the tells, sends, waits and drains, and the
// three tasks' passes. Every address is rom.4's.
#include "Vc2SynthOsMidi.h"
#include "Vc2Synth.h"
#include "Vc2SynthMidi.h"
#include "Vc2SynthOs.h"
#include <algorithm>
#include <cstring>

namespace {
using Path = Vc2MidiPath;
using Os = Vc2SynthOs;
constexpr uint32_t kInputTcb = 0x8994fd90u, kNotesTcb = 0x8994fec8u;   // [FW]
constexpr uint32_t kListenerEvent = 2;     // [FW] the OS MIDI's and the notes' listeners' sends (0x89314e6a, 0x8939020e)
constexpr uint32_t kInputSettings = 8;     // [FW] the MIDI out listener's post (0x89311c7a)
constexpr uint8_t kParameters = 0x40;      // [FW] the parameters' message, and its kind's bits in the word
constexpr uint32_t kKindBits = 0xff;
constexpr uint8_t kOutKinds[] = { 4, 5 };  // [FW] the MIDI out listener's (0x89311c30, its table 0x89311c70)
// [FW] the OS MIDI listener's (0x89314db0): its classes, the parameters' kinds it takes (its table 0x89314e5c)
constexpr uint8_t kOsClasses[] = { 0x01, 0x70, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0 };
constexpr uint8_t kOsKinds[] = { 0, 4, 5 };
// [FW] the notes' listener's (0x893901b0)
constexpr uint8_t kNotesClasses[] = { 0x01, 0x40, 0x41, 0x70, 0x72, 0x80, 0x90, 0xa0, 0xb0, 0xc0, 0xd0, 0xe0 };
constexpr uint8_t kThruOn = 2;             // [FW] 0x8992e344's: the messages to the other side's MIDI out instead
constexpr uint16_t kInputMask = 0xe, kOsMidiMask = 2, kFrontMask = 1;   // [FW] their waits'; the front's the rig's
constexpr uint32_t kInputFirst = 2, kInputSecond = 4;                  // [FW] the receive objects' events

Vc2MidiMessage unpacked(uint32_t w) {
    Vc2MidiMessage m;
    std::memcpy(&m, &w, sizeof m);
    return m;
}

uint32_t packed(const Vc2MidiMessage& m) {
    uint32_t w = 0;
    std::memcpy(&w, &m, sizeof w);
    return w;
}

template <size_t N>
bool in(const uint8_t (&from)[N], uint8_t v) {
    return std::ranges::find(from, v) != std::end(from);
}
} // namespace

const std::array<bool (Vc2SynthOsMidi::*)(Vc2SynthOsMidi::Sender&), (size_t)Vc2SynthOsMidi::Stage::count>
    Vc2SynthOsMidi::kStages = { nullptr, &Vc2SynthOsMidi::ringNode, &Vc2SynthOsMidi::emptyWait,
                                &Vc2SynthOsMidi::hubListener, &Vc2SynthOsMidi::roomWait };

Vc2OsQueueRef Vc2SynthOsMidi::queue(int k) {
    return k == 0 ? Vc2OsQueueRef(s_.midiPath.osMidi) : Vc2OsQueueRef(s_.midiPath.notes);
}

Vc2MidiReceive& Vc2SynthOsMidi::receiveAt(uint32_t at) {
    Path& p = s_.midiPath;
    return at == Path::kUsbAt ? p.usb : at == Path::kUsb2At ? p.usb2 : p.midiIn;
}

// 0x89310e70: its record on the queue's list, then 0x8000 until a drain's end clears it
void Vc2SynthOsMidi::wait(Sender& t, int k) {
    Waiters& w = waiters_[(size_t)k];
    if (w.count < w.list.size()) w.list[w.count++] = &t;
    t.waiting = true;
}

// A drain's end (0x89311140…): the dispatch locked out, every waiter given 0x8000 and its record cleared, the latest
// first; then let in, the woken run
void Vc2SynthOsMidi::wake(int k) {
    os_.lockOut();
    Waiters& w = waiters_[(size_t)k];
    for (size_t i = w.count; i-- > 0;) {
        w.list[i]->waiting = false;
        os_.post(Os::kObjects[(size_t)w.list[i]->task], kWaitEvent);
    }
    w.count = 0;
    os_.unlockOut();
}

// 0x89310fb0: the message put in a listener's queue, its owner given the event; else a wait for room
bool Vc2SynthOsMidi::send(Sender& t, int k, uint32_t owner) {
    if (!vc2QueuePut(queue(k), t.message, t.word)) {
        ++roomWaits;
        t.queue = k;
        wait(t, k);
        t.stage = Stage::roomWait;
        return false;
    }
    os_.post(owner, kListenerEvent);
    return true;
}

bool Vc2SynthOsMidi::outHeard(Sender& t) {   // 0x89311c30
    const auto kind = (uint8_t)(t.word & kKindBits);
    if (vc2MidiClass(unpacked(t.message).status) == kParameters && in(kOutKinds, kind)) {
        os_.post(kInputTcb, kInputSettings);
    }
    return true;
}

bool Vc2SynthOsMidi::clockHeard(Sender& t) {   // 0x89313cb0
    s_.tempoClock().heard(vc2MidiClass(unpacked(t.message).status), t.word);
    return true;
}

bool Vc2SynthOsMidi::osMidiHeard(Sender& t) {   // 0x89314db0
    const uint8_t cls = vc2MidiClass(unpacked(t.message).status);
    const auto kind = (uint8_t)(t.word & kKindBits);
    if (!in(kOsClasses, cls) && !(cls == kParameters && in(kOsKinds, kind))) return true;
    return send(t, 0, s_.midiPath.osMidi.w.owner);
}

bool Vc2SynthOsMidi::notesHeard(Sender& t) {   // 0x893901b0
    if (!in(kNotesClasses, vc2MidiClass(unpacked(t.message).status))) return true;
    s_.midiPath.notes.w.owner = kNotesTcb;   // 0x89390216
    return send(t, 1, kNotesTcb);
}

// The MIDI IN ring's node: the hub's (its method, 0x89314d30, waits for the OS MIDI task's queue to empty first); the
// clock object's a bare return
bool Vc2SynthOsMidi::ringNode(Sender& t) {
    if (t.node == Path::kHubNodeAt) {
        t.stage = Stage::emptyWait;
        return true;
    }
    t.node = s_.midiInNode(t.node).next;
    if (t.node == s_.midiPath.ring) t.stage = Stage::idle;
    return true;
}

bool Vc2SynthOsMidi::emptyWait(Sender& t) {   // 0x89311070 on the OS MIDI task's queue
    Path& p = s_.midiPath;
    if (p.osMidi.w.draining != 0 || !vc2QueueEmpty(p.osMidi.w)) {
        ++emptyWaits;
        wait(t, 0);
        return false;
    }
    t.listener = p.hub.head;
    t.stage = Stage::hubListener;
    return true;
}

bool Vc2SynthOsMidi::roomWait(Sender& t) {   // woken: the put again
    t.stage = Stage::hubListener;
    return true;
}

// The hub's listener at t.listener (0x89311250 calls each from the ring's head), then the next; the hub told, the MIDI
// IN ring's next node (the input task's), or done (the front's). False where it waits for room or its post gave way
bool Vc2SynthOsMidi::hubListener(Sender& t) {
    using Heard = bool (Vc2SynthOsMidi::*)(Sender&);
    static constexpr std::pair<uint32_t, Heard> kListeners[] = { { Path::kOutAt, &Vc2SynthOsMidi::outHeard },
                                                                 { Path::kClockAt, &Vc2SynthOsMidi::clockHeard },
                                                                 { Path::kOsMidiAt, &Vc2SynthOsMidi::osMidiHeard },
                                                                 { Path::kNotesAt, &Vc2SynthOsMidi::notesHeard } };
    Path& p = s_.midiPath;
    const auto* l = std::ranges::find(kListeners, t.listener, &std::pair<uint32_t, Heard>::first);
    if (l != std::end(kListeners) && !(this->*(l->second))(t)) return false;
    t.listener = s_.hubNode(t.listener).next;
    if (t.listener == p.hub.head) {
        t.node = t.viaRing ? s_.midiInNode(t.node).next : p.ring;
        t.stage = !t.viaRing || t.node == p.ring ? Stage::idle : Stage::ringNode;
    }
    return !os_.gaveWay(t.task);
}

// On until done (true) or a wait or a post that gave way (false), its place kept
bool Vc2SynthOsMidi::tell(Sender& t) {
    while (t.stage != Stage::idle) {
        if (!(this->*kStages[(size_t)t.stage])(t)) return false;
    }
    return !os_.gaveWay(t.task);
}

// The input task's loop over a receive object (0x89311a72…, 0x89311abc…): each message told on the MIDI IN ring. With
// USB-MIDI Thru on, MIDI IN's go to USB out (0x89312790 on 0x8995bb10) instead; USB's are told and go to MIDI out as
// well (0x89958870): the MIDI out's, not the part's
bool Vc2SynthOsMidi::takeInput(Vc2MidiReceive& r, bool thruDiverts) {
    Path& p = s_.midiPath;
    uint32_t message = 0;
    while (vc2ReceiveTake(r, message, inputWord_)) {
        if ((thruDiverts && p.words.thru == kThruOn) || p.ring == 0) continue;
        input_ = Sender { Os::kInput, Stage::ringNode, true, message, inputWord_, p.ring };
        if (!tell(input_)) return false;
    }
    return true;
}

bool Vc2SynthOsMidi::firstPort() {
    return (inputEvents_ & kInputFirst) == 0 || takeInput(s_.midiPath.midiIn, true);
}

bool Vc2SynthOsMidi::secondPort() {
    return (inputEvents_ & kInputSecond) == 0 || takeInput(s_.midiPath.usb, false);
}

bool Vc2SynthOsMidi::settingsPhase() {
    if ((inputEvents_ & kInputSettings) != 0) settings();
    return true;
}

// A pass on what its wait took, or on from where it waited (woken by a drain's end) or gave way
uint16_t Vc2SynthOsMidi::inputPass(uint32_t events) {
    static constexpr std::array<bool (Vc2SynthOsMidi::*)(), kInputPhases> kPhases {
        &Vc2SynthOsMidi::firstPort, &Vc2SynthOsMidi::secondPort, &Vc2SynthOsMidi::settingsPhase
    };
    if (inputPhase_ == kInputPhases) {
        inputEvents_ = events;
        inputPhase_ = 0;
    } else if (input_.waiting || (input_.stage != Stage::idle && !tell(input_))) {
        return kWaitEvent;
    }
    for (; inputPhase_ < kInputPhases; ++inputPhase_) {
        if (!(this->*kPhases[inputPhase_])()) return kWaitEvent;
    }
    return kInputMask;
}

// The front's (the rig's stand-in on entry 12): each message given it sent with its word (0x89314d60), on from where
// it waited or gave way
uint16_t Vc2SynthOsMidi::frontPass(uint32_t) {
    if (front_.waiting || (front_.stage != Stage::idle && !tell(front_))) return kWaitEvent;
    while (frontCount_ > 0) {
        const auto [m, w] = frontQueue_[frontHead_];
        frontHead_ = (frontHead_ + 1) % kFrontQueue;
        --frontCount_;
        front_ = Sender { Os::kFront, Stage::emptyWait, false, packed(m), w };
        if (!tell(front_)) return kWaitEvent;
    }
    return kFrontMask;
}

// 0x893110b0: the queue marked draining, each message taken (two words below 0x70) to the handler, then its end
void Vc2SynthOsMidi::drain(int k, const std::function<void(const Vc2MidiMessage&, uint32_t)>& handler) {
    Vc2OsQueueRef q = queue(k);
    q.w.draining = 1;
    uint32_t message = 0, word = 0;
    while (vc2QueueTake(q, message)) {
        if (vc2QueueMessageWords((uint8_t)message) == 2 && !vc2QueueTake(q, word)) break;
        handler(unpacked(message), word);
    }
    q.w.draining = 0;
    wake(k);
}

uint16_t Vc2SynthOsMidi::osMidiPass(uint32_t events) {
    if ((events & kOsMidiMask) != 0)
        drain(0, [this](const Vc2MidiMessage& m, uint32_t word) { s_.midiParts->handled(m, word); });
    return kOsMidiMask;
}

void Vc2SynthOsMidi::notesDrain(const std::function<void(const Vc2MidiMessage&, uint32_t)>& dispatch) {
    drain(1, dispatch);
}
