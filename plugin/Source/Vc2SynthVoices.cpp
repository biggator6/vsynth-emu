// The part's voices' and notes' parts: see Vc2SynthVoices.h (v345).  A voice's virtuals by its class, the plain
// voice's here as rom.4 has them (its vtable 0x898cd1a8's six); a note's tone calls on the pool's tone and its host.
#include "Vc2SynthVoices.h"

uint32_t Vc2SynthNotes::make(uint32_t id) { return s_.makeTone(id); }

// The tone's note-on on the note's own input, where rom.4 writes its fix-up back
void Vc2SynthNotes::noteOn(uint32_t tone, const Vc2NoteWords& note) {
    Vc2NoteWords& w = s_.notes.at(s_.notes.addressOf(&note));
    s_.tones.at(tone).noteOn(w.input, s_.tones, *this, s_.hostOf(tone));
}

void Vc2SynthNotes::setBlock(uint32_t tone, const Vc2NoteBlockRow& row, const uint32_t* block) {
    Vc2Tone& t = s_.tones.at(tone);
    Vc2SynthTone& h = s_.hostOf(tone);
    switch (row.which) {
    case Vc2NoteBlock::oscSet: t.setLegato(block, h); break;
    case Vc2NoteBlock::osc: t.setOscBlock(row.n, block, h); break;
    case Vc2NoteBlock::mix: t.setMixBlock(block, h); break;
    case Vc2NoteBlock::cosm: t.setCosmBlock(row.n, block, h); break;
    case Vc2NoteBlock::envelope: t.setEnvelopeBlock(row.n, block, h); break;
    case Vc2NoteBlock::lfo: t.setLfoBlock(row.n, block, h); break;
    case Vc2NoteBlock::tva: t.setTvaBlock(block, h); break;
    }
}

// [FW] the hook word's routine: the notes' task's (0x893901a0) posts it the ended scan's event; the notes' own first
// (0x893964c0) does nothing
void Vc2SynthNotes::call(uint32_t routine, uint32_t) {
    if (routine == kVc2SynthEndedHook) s_.post(kVc2SynthNotesTask, kVc2SynthEndedEvent);
}

// [FW] 0x893916d0, the plain voice's: its note ended, then its +0x4c and +0x30 cleared
void Vc2SynthVoices::destroy(uint32_t voice) {
    if (noteClass(voice)) {
        s_.classes.destroy(voice, s_.voices, *this);
        return;
    }
    Vc2VoiceNode& n = s_.voices.at(voice);
    s_.notes.end(n.note, *s_.noteParts);
    n.note = 0;
    n.updated = 0;
}

// [FW] the assigner's legato struct (rom.4's on its stack, the part's at its own word), then the class's +0x3c; the plain
// voice's (0x89391710) its +0x30 cleared
void Vc2SynthVoices::prepare(uint32_t voice, const Vc2AssignCall& call, const Vc2Legato& legato) {
    s_.legato = { legato.found, legato.marked, legato.onByte, 0 };
    if (!noteClass(voice)) {
        s_.voices.at(voice).updated = 0;
        return;
    }
    const Vc2PrepareCall c { call.message, call.part, call.onByte, kVc2SynthLegatoAt, call.passed, call.kind };
    s_.classes.prepare(voice, c, s_.voices, s_.keys, *this);
}

// [FW] the class's +0x18 given the value as its byte (the part unread); the plain voice's (0x893916c0) a bare return
void Vc2SynthVoices::retune(uint32_t voice, uint32_t message, uint32_t, uint32_t value) {
    if (noteClass(voice)) s_.classes.retune(voice, message, value, s_.voices, s_.keys, *this);
}

// [FW] the class's +0x30; the plain voice's (0x893916f0) its note's release when it has one
void Vc2SynthVoices::keyOff(uint32_t voice) {
    if (noteClass(voice)) {
        s_.classes.keyOff(voice, s_.voices, *this);
        return;
    }
    const uint32_t note = s_.voices.at(voice).note;
    if (note != 0) s_.notes.release(note, *s_.noteParts);
}

// [FW] the class's +0x48; the plain voice's (0x89391720) 0
uint32_t Vc2SynthVoices::changed(uint32_t voice, uint32_t) {
    return noteClass(voice) ? s_.classes.changed(voice, s_.voices, *this) : 0;
}

// A message, a portamento key: its +0 its key table
uint32_t Vc2SynthVoices::tableOf(uint32_t message) {
    return s_.keys.glides[(message - Vc2Keys::kGlidesAt) / sizeof(Vc2KeyGlide)].table;
}

void Vc2SynthVoices::readParameters(uint32_t part) {
    s_.reloads.message(Vc2PartReloads::kAt + (uint32_t)sizeof(Vc2Reload) * part);
}

uint8_t Vc2SynthVoices::firstByte(uint32_t object) { return s_.reads->firstByte(object); }
