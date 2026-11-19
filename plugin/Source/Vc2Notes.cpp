// The VC-2 part's notes W: see Vc2Notes.h (v275).  Each routine's reads and calls in rom.4's order.
#include "Vc2Notes.h"
#include "Vc2Tone.h"

namespace {
using In = Vc2ToneInput;
using B = Vc2NoteBlock;
constexpr uint32_t kFirstOwn = 1;   // [FW] the passes set every envelope and LFO but the tone's first
constexpr size_t kEnvelopesFrom = 6, kEnvelopeRows = Vc2Tone::kEnvelopes - kFirstOwn;
constexpr size_t kRows = kEnvelopesFrom + kEnvelopeRows + (Vc2Tone::kLfos - kFirstOwn) + 1;

// [FW] the block pass's calls in rom.4's order; the release's are its envelopes'
constexpr std::array<Vc2NoteBlockRow, kRows> passRows() {
    std::array<Vc2NoteBlockRow, kRows> rows {};
    rows[0] = { B::oscSet, 0, In::kOscSet };
    rows[1] = { B::osc, 0, In::kOscs[0] };
    rows[2] = { B::osc, 1, In::kOscs[1] };
    rows[3] = { B::mix, 0, In::kMix };
    rows[4] = { B::cosm, 0, In::kCosms[0] };
    rows[5] = { B::cosm, 1, In::kCosms[1] };
    size_t k = kEnvelopesFrom;
    for (uint32_t e = kFirstOwn; e < Vc2Tone::kEnvelopes; ++e)
        rows[k++] = { B::envelope, e, In::kEnvelopes + In::kEnvelopeStride * e };
    for (uint32_t l = kFirstOwn; l < Vc2Tone::kLfos; ++l) rows[k++] = { B::lfo, l, In::kLfos + In::kLfoStride * l };
    rows[k] = { B::tva, 0, In::kTva };
    return rows;
}
constexpr std::array<Vc2NoteBlockRow, kRows> kPass = passRows();
} // namespace

uint32_t Vc2Notes::addressOf(uint32_t n) const { return kNotesAt + (uint32_t)sizeof(Vc2NoteWords) * n; }

uint32_t Vc2Notes::addressOf(const void* word) const {
    return kNotesAt + (uint32_t)(static_cast<const char*>(word) - reinterpret_cast<const char*>(notes.data()));
}

void Vc2Notes::staticInit() { hook = kNoHook; }

void Vc2Notes::construct(uint32_t note, uint32_t part, uint32_t kind, Vc2NoteParts& parts) {
    Vc2NoteWords& w = at(note);
    w.part = (uint8_t)part;
    w.kind = (uint8_t)kind;
    w.tone = parts.make(kind << 16 | part);
    parts.giveRoutines(w.tone, kEndedRoutine, kTruncatedRoutine);
    w.partner = 0;
    w.truncated = 0;
}

void Vc2Notes::noteOn(uint32_t note, uint32_t partner, uint32_t onByte, Vc2NoteParts& parts) {
    Vc2NoteWords& w = at(note);
    w.partner = partner;
    w.onByte = (uint8_t)onByte;
    w.input.at(In::kPrevious) = partner != 0 ? (uint32_t)parts.serial(at(partner).tone) : kNoSerial;
    parts.noteOn(w.tone, w);
}

void Vc2Notes::end(uint32_t note, Vc2NoteParts& parts) {
    if (note == 0) return;
    const uint32_t tone = at(note).tone;
    if (parts.endTest(tone))
        parts.unmake(tone);
    else
        parts.truncateAndWait(tone);
}

void Vc2Notes::start(Vc2NoteParts& parts) { parts.start(); }

void Vc2Notes::pass(uint32_t note, std::span<const Vc2NoteBlockRow> rows, Vc2NoteParts& parts) {
    for (const Vc2NoteBlockRow& row : rows) parts.setBlock(at(note).tone, row, at(note).input.block(row.offset));
}

void Vc2Notes::setBlocks(uint32_t note, Vc2NoteParts& parts) { pass(note, kPass, parts); }

void Vc2Notes::release(uint32_t note, Vc2NoteParts& parts) {
    pass(note, std::span(kPass).subspan(kEnvelopesFrom, kEnvelopeRows), parts);
    parts.keyOff(at(note).tone);
}

void Vc2Notes::truncateOnce(uint32_t note, Vc2NoteParts& parts) {
    Vc2NoteWords& w = at(note);
    if (w.truncated != 0) return;
    w.truncated = 1;
    parts.truncateAtOnce(w.tone);
}

uint32_t Vc2Notes::position(uint32_t note, uint32_t osc, Vc2NoteParts& parts) {
    return parts.position(at(note).tone, osc);
}

void Vc2Notes::ended(uint32_t tone, Vc2NoteParts& parts) { parts.call(hook, tone); }

void Vc2Notes::truncated(uint32_t tone, Vc2NoteParts& parts) { parts.unmake(tone); }
