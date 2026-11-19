#pragma once
// xt_vc2voices' fourth part's shared parts (v279): the release part's sides with the port's update queue and voice
// class beside them; their parts (the note's, C's and the OS's calls logged into the voices' log, the queue's run on
// the port's queue); the counts and the events.  XT_VC2VOICES_MAIN only.
#include "Vc2AssignReleaseCheck.h"
#include "Vc2VoiceClass.h"
#include "Vc2VoiceQueue.h"

inline constexpr uint32_t kClassSetsAt = 0x89997fe8, kClassSetBytes = 0x6c;   // [FW] the conversion sets C, by number
inline constexpr uint32_t kClassLockAt = 0x8994fec8;                        // [FW] the OS object the update locks
inline constexpr uint32_t kClassRetuneWordAt = 0x8953fb2c;                  // [FW] the retune's word
inline constexpr uint32_t kClassQueueByte = 0x0c;                           // [FW] a voice's queue, by number

class ClassPortParts final : public Vc2VoiceClassParts, public Vc2QueueParts {
public:
    ClassPortParts(VoicesSides& s, Vc2UpdateQueue& queue) : s_(s), queue_(queue) {}
    void constructNote(uint32_t note, uint32_t part, uint32_t kind) override;
    void endNote(uint32_t note) override;
    void noteBlocks(uint32_t note) override;
    void noteUpdate(uint32_t note) override;
    void noteRelease(uint32_t note) override;
    uint32_t placeSet(uint32_t number) override;
    void makeSet(uint32_t set, const Vc2SetCall& call, uint32_t note) override;
    void endSet(uint32_t set) override;
    void convertSet(uint32_t set, const Vc2SetCall& call, uint32_t note) override;
    void releaseSet(uint32_t set, uint32_t velocity) override;
    void updateSet(uint32_t set, const Vc2SetCall& call) override;
    void copySet(uint32_t set, uint32_t note) override;
    uint32_t refreshSet(uint32_t set, const Vc2SetCall& call) override;
    void lock() override;
    void unlock() override;
    void enqueue(uint32_t queue, uint32_t voice) override;
    void takeOff(uint32_t queue, uint32_t voice) override;
    void waitQueue(uint32_t queue, uint32_t voice) override;
    uint8_t firstByte(uint32_t object) override { return s_.fw.cpu.rd8(object); }
    uint32_t retuneWord() override { return s_.fw.cpu.rd32(kClassRetuneWordAt); }
    void wait(uint32_t object) override;
    void signal(uint32_t object) override;
    void post(uint32_t object) override;

private:
    uint32_t logged(int32_t kind, uint32_t routine, uint32_t object, std::array<uint32_t, 7> more);
    // The port's queue, for the one rom.4 has; another address is logged as a call rom.4 never makes
    Vc2UpdateQueue& queueAt(uint32_t queue);
    VoicesSides& s_;
    Vc2UpdateQueue& queue_;
};

struct ClassCounts {
    long events = 0;
    std::array<long, 19> byEvent {};
    long setCalls = 0, osCalls = 0;
};

// The fourth part's sides: the release part's, the port's queue and class, their parts, the counts
struct ClassSides {
    explicit ClassSides(VoicesSides& s);
    ReleaseSides r;
    Vc2UpdateQueue queue {};
    Vc2VoiceClass voices {};
    ClassPortParts parts;
    ClassCounts n {};
};

// rom.4's routine on `args` after the queue's words are written into its memory too, then the port's `mine()` giving
// its answer; what differs in the voices' or keys' span, the queue's words, the calls or the answer
std::string classOnBoth(ClassSides& c, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                        const std::function<uint32_t()>& mine);

using ClassEvent = std::string (*)(ClassSides&, std::mt19937&);
std::span<const ClassEvent> classEvents();
