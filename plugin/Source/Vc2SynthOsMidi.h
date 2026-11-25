#pragma once
// The XT's MIDI input path on the part's OS model (v355; the coordinator's call, HARDWARE_FIRMWARE.md, "The part's tasks
// on the kernel"). Its memory as rom.4 lays it out (Vc2MidiPath): the receive objects of MIDI IN, USB MIDI and a second
// USB port, the MIDI IN ring of listeners (the hub's node, the clock object while its clocks come from outside), the
// hub with its ring of listeners (the MIDI out side's, the clock task's, the OS MIDI task's and the notes' task's, the
// last two each with a queue), and the library's words. Its tasks' parts:
// - the MIDI input task (entry 1, level 1): on 2 and 4 each message its receive object holds told on the MIDI IN ring
//   (the hub's node waits for the OS MIDI task's queue to empty, then tells the hub's listeners); on 8 the MIDI
//   settings (the receive object the sensing check watches, USB-MIDI Thru, each change a marker);
// - the OS MIDI task (entry 6, level 12): its queue drained into the OS MIDI's handler;
// - the front (entry 12's level, 27): each message given it sent as the front's are (0x89314d60);
// - the notes' task's drain of its queue into the dispatcher.
// A sender (the input task, the front) waits where rom.4's waits: for the OS MIDI task's queue to empty, for room in a
// listener's queue (0x89310e70: on 0x8000 until a drain's end wakes it); its place is kept, so its pass ends at the
// wait and goes on from there when its task runs again, as it does at a post that switches to a task above it
// (Vc2SynthOs::kGivesWay). A drain's end wakes every waiter with the dispatch locked out.
// MIDI held for the input task waits in the receive objects' rings (2048 messages; full, the backlog makes way for a
// marker: Vc2MidiReceive). Nothing here blocks or allocates.
#include "Vc2MidiDispatch.h"
#include "Vc2MidiReceive.h"
#include "Vc2OsQueue.h"
#include <array>
#include <cstdint>
#include <functional>

struct Vc2Synth;
class Vc2SynthOs;

// The hub (0x89961238, row 30, 0x8931a5d0): its listeners' ring's head, its own node on the MIDI IN ring and the
// node's class (whose method, 0x89314d30, waits for the OS MIDI task's queue to empty and tells the hub), whether the
// node is on the ring, its class. The system's store object follows it (0x89961250, Vc2System's)
struct Vc2MidiHub {
    uint32_t head = 0;
    Vc2RingNode node {};                   // +0x04
    uint32_t nodeClass = 0;                // +0x0c
    uint8_t linked = 0;                    // +0x10
    std::array<uint8_t, 3> pad11 {};
    uint32_t klass = 0;                    // +0x14
};
static_assert(sizeof(Vc2MidiHub) == 0x18, "rom.4's layout, the hub");

// The library's words at 0x8992e338: the receive object the sensing check watches, the settings' USB MIDI flags
struct Vc2MidiPathWords {
    uint32_t unread338 = 0;
    uint32_t active = 0;                   // 0x8992e33c
    uint8_t usbFlag = 0;                   // 0x8992e340: System B's ID 0x80 set
    std::array<uint8_t, 3> pad341 {};
    uint8_t thru = 0;                      // 0x8992e344: USB-MIDI Thru, 2 on, 3 off, 0 before the settings' first
    std::array<uint8_t, 3> pad345 {};
};

struct Vc2MidiPath {
    // [FW] each one's address
    static constexpr uint32_t kMidiInAt = 0x8995662cu, kUsbAt = 0x899598b0u, kUsb2At = 0x899543e8u;
    static constexpr uint32_t kRingAt = 0x899598a0u, kOutAt = 0x899598a4u, kHubAt = 0x89961238u;
    static constexpr uint32_t kHubNodeAt = kHubAt + 4, kClockAt = 0x8995cc08u, kOsMidiAt = 0x8995cca0u;
    static constexpr uint32_t kNotesAt = 0x8998190cu, kWordsAt = 0x8992e338u;
    Vc2MidiReceive midiIn {}, usb {}, usb2 {};
    uint32_t ring = 0;                     // the MIDI IN ring's head
    Vc2RingListener out {};                // the MIDI out side's listener
    Vc2MidiHub hub {};
    Vc2RingListener clock {};              // the clock task's listener
    Vc2OsQueue<0x400> osMidi {};           // the OS MIDI task's listener and queue
    Vc2OsQueue<0x80> notes {};             // the notes' task's
    Vc2MidiPathWords words {};
    // The part's start's of it: the notes' listener and the clock's, which the part's rows and row 21 build
    void buildPart();
    // The rest, the OS's rows 14…30 (the receive objects, the MIDI IN ring, the out listener, the hub, the OS MIDI's)
    void buildOs();
};

class Vc2SynthOsMidi final : public Vc2MidiReceiveParts {
public:
    static constexpr uint16_t kWaitEvent = 0x8000;   // [FW] a sender's wait (0x89310ed4)
    Vc2SynthOsMidi(Vc2Synth& s, Vc2SynthOs& os) : s_(s), os_(os) {}
    // The prologues' parts: each listener on the hub as its task's start puts it; the input task's two cyclic records
    void inputStart();
    void clockStart();
    void osMidiStart();
    void notesStart();
    void midiOn();   // the front's (0x8931a590): the receive objects on, the hub's node on the MIDI IN ring
    // The passes, each the mask its task waits on after it
    uint16_t inputPass(uint32_t events);
    uint16_t osMidiPass(uint32_t events);
    uint16_t frontPass(uint32_t events);
    // The notes' task's drain (0x893110b0 on its queue): each message to `dispatch`, then its waiters woken
    void notesDrain(const std::function<void(const Vc2MidiMessage&, uint32_t)>& dispatch);
    // Outside: a byte of a channel message on source 1 or 2, as the receive interrupt gives it; a message given the
    // front (false, dropped, with its queue full)
    void receiveByte(int source, uint8_t byte);
    bool give(const Vc2MidiMessage& m, uint32_t word);
    void sensing();   // cyclic record 7's handler (0x89311330)
    void raise(uint32_t task, uint32_t event) override;
    void post(uint32_t task, uint32_t event) override;
    long roomWaits = 0, emptyWaits = 0, frontDrops = 0;

private:
    // A sender's tell, its place kept across a wait
    enum class Stage : int { idle, ringNode, emptyWait, hubListener, roomWait, count };
    struct Sender {
        int task = 0;
        Stage stage = Stage::idle;
        bool viaRing = false;     // told on the MIDI IN ring (the input task's) or the hub at once (the front's)
        uint32_t message = 0, word = 0;
        uint32_t node = 0;        // the MIDI IN ring's node being told
        uint32_t listener = 0;    // the hub's listener being told
        int queue = 0;            // the queue its room wait is on: 0 the OS MIDI task's, 1 the notes'
        bool waiting = false;     // its wait's record on a queue's list, until a drain's end clears it
    };
    struct Waiters {
        std::array<Sender*, 4> list {};
        size_t count = 0;
    };
    Vc2Synth& s_;
    Vc2SynthOs& os_;
    Sender input_ {}, front_ {};
    std::array<Waiters, 2> waiters_ {};   // the OS MIDI task's queue's, the notes'
    // the events its pass took; the take's word, a word on rom.4's input task's stack the take writes only for a marker
    // (its low byte 0), which no listener reads for a channel message or a reset
    uint32_t inputEvents_ = 0, inputWord_ = 0;
    static constexpr size_t kInputPhases = 3;
    size_t inputPhase_ = kInputPhases;    // where the input task's loop is: its event 2's, 4's, 8's, then done
    static constexpr size_t kFrontQueue = 2048;
    std::array<std::pair<Vc2MidiMessage, uint32_t>, kFrontQueue> frontQueue_ {};
    size_t frontHead_ = 0, frontCount_ = 0;

    bool tell(Sender& t);                 // on until done (true) or a wait (false)
    // A tell's steps, by its stage (kStages): each false where it waits
    bool ringNode(Sender& t);
    bool emptyWait(Sender& t);
    bool hubListener(Sender& t);
    bool roomWait(Sender& t);
    static const std::array<bool (Vc2SynthOsMidi::*)(Sender&), (size_t)Stage::count> kStages;
    // The hub's listeners' methods, by their address (kListeners): each false waiting for room
    bool outHeard(Sender& t);
    bool clockHeard(Sender& t);
    bool osMidiHeard(Sender& t);
    bool notesHeard(Sender& t);
    // The input task's loop's three parts, in order (kPhases): each false where it waits
    bool firstPort();
    bool secondPort();
    bool settingsPhase();
    bool send(Sender& t, int queue, uint32_t owner);
    void wait(Sender& t, int queue);
    void wake(int queue);
    Vc2OsQueueRef queue(int k);
    Vc2MidiReceive& receiveAt(uint32_t at);
    // the input task's loop over one object, its messages not told with USB-MIDI Thru on where `thruDiverts`: false
    // waiting
    bool takeInput(Vc2MidiReceive& r, bool thruDiverts);
    void settings();                      // the input task's 8: 0x89311ca0, 0x89311d10
    void drain(int queue, const std::function<void(const Vc2MidiMessage&, uint32_t)>& handler);
};
