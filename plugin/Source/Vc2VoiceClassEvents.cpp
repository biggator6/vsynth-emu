// xt_vc2voices' fourth part's events (v279): each of the voice class's virtuals and its two word setters, and each of
// the update queue's routines, on both sides, rom.4's then the port's; what differs after: the voices' and keys' spans,
// the queue's words, the calls and the answer.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2VoiceClassCheck.h"

namespace {
using V = Vc2PartVoices;
using Q = Vc2UpdateQueue;
using OnVoice = void (Vc2VoiceClass::*)(uint32_t, Vc2PartVoices&, Vc2VoiceClassParts&);
using OnNode = void (Vc2UpdateQueue::*)(uint32_t, Vc2PartVoices&, Vc2QueueParts&);

uint32_t classVoice(ClassSides& c, std::mt19937& rng) { return c.r.a.s.port.addressOf((int)(rng() % V::kVoices)); }

// A node for the queue's own routines, which don't read its queue byte: a voice, or one time in four the node given
// when there is one (v312); half the time that byte drawn, on both sides
uint32_t queueNode(ClassSides& c, std::mt19937& rng) {
    const uint32_t node = c.hasNode && rng() % 4 == 0 ? kVc2StaticNodeAt : classVoice(c, rng);
    if (rng() % 2) return node;
    const uint8_t queue = (uint8_t)rng();
    c.queue.slotsOf(node, c.r.a.s.port).queue = queue;
    c.r.a.s.fw.cpu.wr8(node + kClassQueueByte, queue);
    return node;
}

uint32_t classMessage(std::mt19937& rng) { return kAssignMessagesAt + kAssignMessageBytes * (rng() % kAssignMessages); }

uint32_t classByte(std::mt19937& rng) { return rng() % 2 ? rng() % 128 : (uint32_t)rng(); }

// A routine that answers nothing, on both
std::string classPlain(ClassSides& c, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                       const std::function<void()>& mine) {
    return classOnBoth(c, r, args, [&] {
        mine();
        return 0u;
    });
}

// [FW] the virtuals given a voice alone
const std::pair<Vc2CheckRoutine, OnVoice> kOnVoice[] = {
    { { 0x893970b0, "the update" }, &Vc2VoiceClass::update },
    { { 0x893973a0, "the destroy" }, &Vc2VoiceClass::destroy },
    { { 0x89397280, "the key-off" }, &Vc2VoiceClass::keyOff },
};
constexpr size_t kOnVoiceEvent[3] = { 0, 2, 3 };

template <size_t I>
std::string classVoiceEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t voice = classVoice(c, rng);
    ++c.n.byEvent[kOnVoiceEvent[I]];
    return classPlain(c, kOnVoice[I].first, { voice },
                      [&] { (c.voices.*kOnVoice[I].second)(voice, c.r.a.s.port, c.parts); });
}

// A retune's voice, message, part (which rom.4 leaves unread) and byte
std::array<uint32_t, 4> classRetuneArgs(ClassSides& c, std::mt19937& rng) {
    return { classVoice(c, rng), classMessage(rng), releaseMessage(rng), classByte(rng) };
}

std::string classRetuneEvent(ClassSides& c, std::mt19937& rng) {
    const auto [voice, message, part, onByte] = classRetuneArgs(c, rng);
    ++c.n.byEvent[1];
    return classPlain(c, { 0x893971f0, "the retune" }, { voice, message, part, onByte },
                      [&] { c.voices.retune(voice, message, onByte, c.r.a.s.port, c.r.keys, c.parts); });
}

// A prepare's call: a message, a part, a byte, and on the stack a legato's address, a word passed on and a kind
Vc2PrepareCall classCall(ClassSides& c, std::mt19937& rng) {
    Vc2PrepareCall p;
    p.message = classMessage(rng);
    p.part = releaseMessage(rng);
    p.onByte = classByte(rng);
    p.legatoAt = (uint32_t)rng();
    p.passed = (uint32_t)rng();
    p.kind = rng() % 2 ? rng() % 8 : (uint32_t)rng();
    assignStack(c.r.a, { p.legatoAt, p.passed, p.kind });
    return p;
}

std::string classPrepareEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t voice = classVoice(c, rng);
    const Vc2PrepareCall p = classCall(c, rng);
    ++c.n.byEvent[4];
    return classPlain(c, { 0x89397130, "the prepare" }, { voice, p.message, p.part, p.onByte },
                      [&] { c.voices.prepare(voice, p, c.r.a.s.port, c.r.keys, c.parts); });
}

std::string classChangedEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t voice = classVoice(c, rng);
    ++c.n.byEvent[5];
    return classOnBoth(c, { 0x893972d0, "the change test", vc2R0 }, { voice },
                       [&] { return c.voices.changed(voice, c.r.a.s.port, c.parts); });
}

std::string classPrepareWordsEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t voice = classVoice(c, rng);
    const Vc2PrepareCall p = classCall(c, rng);
    assignStack(c.r.a, { p.passed });
    ++c.n.byEvent[6];
    return classPlain(c, { 0x89391ae0, "the prepare's words" }, { voice, p.message, p.part, p.onByte },
                      [&] { c.voices.prepareWords(voice, p, c.r.a.s.port, c.r.keys); });
}

std::string classRetuneWordsEvent(ClassSides& c, std::mt19937& rng) {
    const auto [voice, message, part, onByte] = classRetuneArgs(c, rng);
    ++c.n.byEvent[7];
    return classPlain(c, { 0x89391b40, "the retune's words" }, { voice, message, part, onByte },
                      [&] { c.voices.retuneWords(voice, message, onByte, c.r.a.s.port, c.r.keys); });
}

std::string queueConstructEvent(ClassSides& c, std::mt19937&) {
    ++c.n.byEvent[8];
    return classPlain(c, { 0x89394f60, "the queue's constructor" }, { Q::kAt }, [&] { c.queue.construct(); });
}

std::string queueStaticEvent(ClassSides& c, std::mt19937&) {
    ++c.n.byEvent[9];
    return classPlain(c, { 0x89395280, "the queue's initializer" }, {}, [&] { c.queue.staticInit(); });
}

std::string queueLinkEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t node = queueNode(c, rng);
    const uint32_t after = rng() % 3 ? Q::kHeadAt : classVoice(c, rng);
    ++c.n.byEvent[10];
    return classPlain(c, { 0x89394f10, "the link" }, { node, after },
                      [&] { c.queue.link(node, after, c.r.a.s.port); });
}

std::string queueUnlinkEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t node = queueNode(c, rng);
    ++c.n.byEvent[11];
    return classPlain(c, { 0x89394f20, "the unlink" }, { node }, [&] { c.queue.unlink(node, c.r.a.s.port); });
}

// [FW] the queue's routines on a node and the queue
const std::pair<Vc2CheckRoutine, OnNode> kOnNode[] = {
    { { 0x89394fa0, "the enqueue" }, &Vc2UpdateQueue::enqueue },
    { { 0x89395040, "the move on" }, &Vc2UpdateQueue::moveOn },
    { { 0x893950b0, "the pass on" }, &Vc2UpdateQueue::passOn },
    { { 0x89395120, "the take off" }, &Vc2UpdateQueue::takeOff },
};

template <size_t I>
std::string queueNodeEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t node = queueNode(c, rng);
    ++c.n.byEvent[12 + I];
    return classPlain(c, kOnNode[I].first, { Q::kAt, node },
                      [&] { (c.queue.*kOnNode[I].second)(node, c.r.a.s.port, c.parts); });
}

std::string queueWaitEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t node = queueNode(c, rng);
    ++c.n.byEvent[16];
    return classPlain(c, { 0x893951b0, "the wait" }, { Q::kAt, node }, [&] { c.queue.wait(node, c.parts); });
}

std::string queueWaitAllEvent(ClassSides& c, std::mt19937&) {
    ++c.n.byEvent[17];
    return classPlain(c, { 0x893951d0, "the wait for all" }, { Q::kAt }, [&] { c.queue.waitAll(c.parts); });
}

std::string queueMoveEvent(ClassSides& c, std::mt19937& rng) {
    const uint32_t node = queueNode(c, rng);
    ++c.n.byEvent[18];
    return classPlain(c, { 0x89395210, "the move" }, { node, 0 }, [&] { c.queue.moveTo(node, 0, c.r.a.s.port); });
}

constexpr ClassEvent kEvents[] = {
    classVoiceEvent<0>,   classRetuneEvent,      classVoiceEvent<1>,    classVoiceEvent<2>,  classPrepareEvent,
    classPrepareEvent,    classChangedEvent,     classPrepareWordsEvent, classRetuneWordsEvent, queueConstructEvent,
    queueStaticEvent,     queueLinkEvent,        queueUnlinkEvent,      queueNodeEvent<0>,   queueNodeEvent<1>,
    queueNodeEvent<2>,    queueNodeEvent<3>,     queueWaitEvent,        queueWaitAllEvent,   queueMoveEvent,
};
constexpr size_t kFirstQueueEvent = 9;   // queueConstructEvent's: the queue's own routines from it on
static_assert(kEvents[kFirstQueueEvent] == queueConstructEvent, "the queue's events are the list's last");
} // namespace

std::span<const ClassEvent> classEvents() { return kEvents; }

std::span<const ClassEvent> classQueueEvents() { return std::span(kEvents).subspan(kFirstQueueEvent); }
#endif
