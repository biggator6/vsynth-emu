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
inline constexpr uint32_t kClassStaticNodeClass = 0x898cd460;               // [FW] the static M's node's class (v295)

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
    void releaseSet(uint32_t set, uint32_t velocity, uint32_t note) override;
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
    long passes = 0, others = 0, onQueue = 0, taskCalls = 0;   // the seventh part's (v312)
};

// The fourth part's sides: the release part's, the port's queue and class, their parts, the counts
struct ClassSides {
    explicit ClassSides(VoicesSides& s);
    ReleaseSides r;
    Vc2UpdateQueue queue {};
    Vc2VoiceClass voices {};
    ClassPortParts parts;
    ClassCounts n {};

    // The one node besides the voices' on the port's queue, the static M's (v312): the seventh part's alone, given by
    // giveNode with its class; a node's class, the node given's or a voice's
    bool hasNode = false;
    Vc2QueueNode node {};
    void giveNode();
    uint32_t classOf(uint32_t at);
};

// The queue's words and the node given written into rom.4's memory; what differs in them after (v312's file)
void classQueueWrite(ClassSides& c);
std::string classQueueDiffer(ClassSides& c);

// rom.4's routine on `args` after the queue's words are written into its memory too, then the port's `mine()` giving
// its answer; what differs in the voices' or keys' span, the queue's words, the calls or the answer
std::string classOnBoth(ClassSides& c, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                        const std::function<uint32_t()>& mine);

using ClassEvent = std::string (*)(ClassSides&, std::mt19937&);
std::span<const ClassEvent> classEvents();
// The update queue's own routines' events alone, the last of classEvents (v312: the seventh part's, whose hooks on the
// classes' +0x0c would shadow rom.4's update virtual)
std::span<const ClassEvent> classQueueEvents();

// The fourth part's rom.4 hooks, case start, queue draw and draws before an event (all at a case's first): shared
// with the sixth part from v310
void classInstallRom(ClassSides& c);
std::string classStart(ClassSides& c, std::mt19937& rng);
void drawQueue(ClassSides& c, std::mt19937& rng);
void classDraw(ClassSides& c, std::mt19937& rng, bool first);
