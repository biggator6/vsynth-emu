// xt_vc2sets' third part (v282): C's envelopes, Vc2SetEnvelopes, against rom.4's own routines on its own envelopes
// (0x89e2543c) and notes W, after the common's part on the seed after its; the Matrix group's five readers hooked to log
// the M they're given and answer one run of drawn sums (the sets' part's hooks on the envelope's routines taken off);
// powf, the helpers and the curves rom.4's own.  The voices, the parts, the parts' envelope words (0x89e2765c) and the
// clock object (0x8995cb40) are drawn in rom.4's memory, which the port reads too.  Each case noise in the envelopes,
// then 400 events: the placement, the constructor, the end, the conversion, the update, the copy, the release and the
// block writer alone; after each the envelopes, W, the calls and the answer.  Not drawn: a clock divisor of 0, a tempo
// of 0 in an envelope (the note value's 0 / 0 would be a NaN whose payload is the host's).  XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetEnvelope.h"
#include "Vc2SetsChildCheck.h"

namespace {
using Envelopes = Vc2SetEnvelopes;
// [FW] the envelope's routines, which the sets' part hooks; the Matrix group's readers, which this part hooks
constexpr uint32_t kRoutines[] = { 0x8939a620, 0x8939a690, 0x8939a780, 0x8939a790, 0x8939a7b0, 0x8939a800,
                                   0x8939a810 };
constexpr uint32_t kReaders[] = { 0x893a2550, 0x893a2750, 0x893a2960, 0x893a2aa0, 0x893a2bf0 };

struct EnvelopeCounts : SetChildCounts<8> {
    static constexpr const char* kPool = "the envelopes";
    static constexpr const char* kSaid = "envelopes: %ld events: %ld placements, %ld constructors, %ld ends, %ld "
                                         "conversions, %ld updates, %ld copies, %ld releases, %ld block writes (%ld "
                                         "Matrix sums)\n";
};

using EnvelopeSides = SetChildSides<Envelopes, EnvelopeCounts>;
using EnvelopeEvent = std::string (*)(EnvelopeSides&, std::mt19937&);

uint32_t anyEnvelopeBlock(std::mt19937& rng) {
    return setsNote(rng) + Vc2ToneInput::kEnvelopes + Vc2ToneInput::kEnvelopeStride * (1 + rng() % 13);
}

// A time's byte (a value at its ends or any, a note value now and then), a depth, a key follow
uint8_t envelopeTime(std::mt19937& rng) {
    const auto times = std::to_array<uint8_t>({ 0, 1, 64, 127, (uint8_t)(128 + rng() % 128), (uint8_t)(rng() % 128) });
    return setsOneOf(times, rng);
}

int8_t envelopeSens(std::mt19937& rng) {
    return setsOneOf(std::to_array<int8_t>({ 0, 1, -1, 63, -63, 127, -127, (int8_t)rng() }), rng);
}

constexpr auto envelopeFollow = setsFollowDrawn;

uint16_t pairOf(uint8_t low, uint8_t high) { return (uint16_t)(low | high << 8); }

// The part's five words for an envelope: on (usually), the times, the sustain, the depths, the key follow
std::array<uint16_t, 5> envelopeWordsDrawn(std::mt19937& rng) {
    const uint8_t on = rng() % 4 ? 1 : rng() % 2 ? 0 : (uint8_t)rng();
    return { pairOf(on, envelopeTime(rng)), pairOf(envelopeTime(rng), (uint8_t)rng()),
             pairOf(envelopeTime(rng), (uint8_t)envelopeSens(rng)),
             pairOf((uint8_t)envelopeSens(rng), (uint8_t)envelopeSens(rng)), (uint16_t)envelopeFollow(rng) };
}

void envelopeWordsOf(Vc2EnvelopeWords& w, std::mt19937& rng) {
    setsNoise(w, rng);
    w.index = (uint8_t)(rng() % 4 ? rng() % 13 : rng());
    w.attack = envelopeTime(rng);
    w.decay = envelopeTime(rng);
    w.release = envelopeTime(rng);
    w.attackSens = envelopeSens(rng);
    w.decaySens = envelopeSens(rng);
    w.releaseSens = envelopeSens(rng);
    w.keyfollow = envelopeFollow(rng);
    if (w.tempo == 0) w.tempo = 1;
}

void envelopeDraw(EnvelopeSides& e, std::mt19937& rng, bool all) {
    setsDraw(e.s, rng, all);
    setsPartWordsDrawn(e.s.fw, rng, all, kSetsEnvelopeTable, envelopeWordsDrawn);
    if (all || rng() % 8 == 0) setsClockDrawn(e.s.fw, rng);
    setsSumsDrawn(e.s.fw, rng, all);
    for (Vc2EnvelopeWords& w : e.port.items)
        if (all || rng() % 8 == 0) envelopeWordsOf(w, rng);
}

std::string envelopeReleaseEvent(EnvelopeSides& e, std::mt19937& rng) {
    const uint32_t envelope = e.any(rng), block = anyEnvelopeBlock(rng), velocity = (uint32_t)rng();
    ++e.n.byEvent[6];
    const Vc2CheckRoutine r { 0x8939a810, "the envelope's release" };
    return setsChildOnBoth(e.s, e.region(EnvelopeCounts::kPool), r, { envelope, velocity, block }, [&] {
        e.port.release(envelope, velocity, setsPortWords(e.s, block), e.s.parts);
        return 0u;
    });
}

// [FW] the events by routine; the copy and the block writer 0x8939a3d0 alone, the same work
const EnvelopeEvent kEvents[] = {
    setsPlaceEvent<EnvelopeSides, 0x8939a620, 28, 0>,
    setsMakeEvent<EnvelopeSides, anyEnvelopeBlock, 0x8939a690, 1, Envelopes::kPerVoice>,
    setsMakeEvent<EnvelopeSides, anyEnvelopeBlock, 0x8939a690, 1, Envelopes::kPerVoice>,
    setsEndEvent<EnvelopeSides, 0x8939a780, 2>,
    setsConvertEvent<EnvelopeSides, &Envelopes::convert, anyEnvelopeBlock, 0x8939a790, 3>,
    setsConvertEvent<EnvelopeSides, &Envelopes::convert, anyEnvelopeBlock, 0x8939a790, 3>,
    setsUpdateEvent<EnvelopeSides, &Envelopes::update, 0x8939a7b0, 4>,
    setsCopyEvent<EnvelopeSides, &Envelopes::copy, anyEnvelopeBlock, 0x8939a800, 5>,
    envelopeReleaseEvent,
    envelopeReleaseEvent,
    setsCopyEvent<EnvelopeSides, &Envelopes::copy, anyEnvelopeBlock, 0x8939a3d0, 7>,
};

} // namespace

long vc2SetEnvelopePart(SetSides& s, const Vc2Run& run) {
    return setsChildPart<EnvelopeSides, envelopeDraw, kEvents>(s, run, 2, kRoutines, kReaders);
}
#endif
