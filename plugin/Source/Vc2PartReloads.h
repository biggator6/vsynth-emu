#pragma once
// The VC-2 part's reload objects (v300, step 4.3's twenty-eighth version, item 10's fifth part): rom.4's 16 objects of
// 24 bytes from 0x89996120 word for word, one a part, each an OS timer node on the queue 0x8994fc24 that reads its
// part's settings when it fires, and the three floats after them; their static initializer 0x8939b350 (each object made
// in place by 0x8939b260 on the OS base 0x893002a0's words), the cancel 0x8939ae50 (the held flags set around the
// queue's remove), the run 0x8939af10, the request 0x8939af40, the timer's fire 0x8939b020 (the vtable's +0x14) and the
// notes' message 0x8939b050 (v278's Vc2AssignParts::readParameters).  Not ported, as nothing in rom.4 reaches them:
// 0x8939ae90, 0x8939aea0 and the init 0x8939af00.  The queue's add and remove and the part's read (v292's 0x8939b170)
// through Vc2ReloadParts; the floats read by Vc2PartSettingsTables.  Not in the sound yet.  Bit for bit:
// xt_vc2settings' fourteenth part.  HARDWARE_FIRMWARE.md, "The VC-2 part's reload objects".  The notes' task.
#include <array>
#include <cstdint>

// rom.4's flag bytes 0x8993d0c0…c4, each task's hold on the part's tables, which Vc2PartParams::held reads
using Vc2HeldFlags = std::array<uint8_t, 5>;

// one-off: the node and the object are rom.4's own layouts, each checked by its own size; declarations, not a job to
// template
// [FW] an OS timer node, as 0x893002a0 makes one: its links (itself, off the queue), a word of the queue's, its vtable
struct Vc2TimerNode {
    uint32_t next = 0, prev = 0;   // +0x00, +0x04
    uint32_t word8 = 0;            // +0x08: the queue's
    uint32_t vtable = 0;           // +0x0c
};
static_assert(sizeof(Vc2TimerNode) == 16, "rom.4's layout, an OS timer node");

// one-off: rom.4's own layout, checked by its own size; a declaration, not a job to template
// [FW] a reload object
struct Vc2Reload {
    Vc2TimerNode node;             // +0x00
    uint8_t part = 0;              // +0x10: the part it reads
    uint8_t state = 0;             // +0x11: 0 idle, 1 asked, 2 reading
    std::array<uint8_t, 2> unread12 {};
    uint32_t posted = 0;           // +0x14: on the queue
};
static_assert(sizeof(Vc2Reload) == 24, "rom.4's layout, a reload object");

// What the reload objects reach: the queue's add and remove, and the part's settings read (v292's dispatcher, its
// answer whether it read them whole)
class Vc2ReloadParts {
public:
    virtual ~Vc2ReloadParts() = default;
    virtual void post(uint32_t object) = 0;     // 0x893004a0 on 0x8994fc24
    virtual void unpost(uint32_t object) = 0;   // 0x89300640 on 0x8994fc24
    virtual bool readPart(int part) = 0;        // 0x8939b170
};

class Vc2PartReloads {
public:
    static constexpr uint32_t kAt = 0x89996120, kFloatsAt = 0x899962a0;   // [FW]
    static constexpr int kObjects = 16;                                   // [FW] one a part
    Vc2PartReloads(Vc2HeldFlags& held, Vc2ReloadParts& parts) : held_(held), parts_(parts) {}

    void staticInit();                    // 0x8939b350
    void construct(uint32_t object);      // 0x8939b260, in place
    void cancel(uint32_t object);         // 0x8939ae50
    bool run(uint32_t object);            // 0x8939af10: the read's answer
    void request(uint32_t object);        // 0x8939af40
    int32_t fire(uint32_t object);        // 0x8939b020: 0
    void message(uint32_t object);        // 0x8939b050: its answer unread by its callers

    std::array<Vc2Reload, kObjects> objects {};   // 0x89996120
    std::array<float, 3> floats {};               // 0x899962a0

private:
    // A way by the state byte, rom.4's braf tables in the request and the message
    using Way = void (Vc2PartReloads::*)(uint32_t);
    Vc2Reload& at(uint32_t object);
    void unpostHeld(uint32_t object);     // the remove with every flag held, as the cancel and the request make it
    void stopReading(uint32_t object);    // reading: the remove held, as the run and the request make it
    void readNow(uint32_t object);        // 0x8939b0a0: idle, then the read
    void post(uint32_t object);           // asked, and on the queue unless it is
    void repost(uint32_t object);         // the request's way from reading: the remove held, then posted
    void unpostNow(uint32_t object);      // the message's way from reading: the remove alone
    void ignore(uint32_t object);
    Vc2HeldFlags& held_;
    Vc2ReloadParts& parts_;
};
