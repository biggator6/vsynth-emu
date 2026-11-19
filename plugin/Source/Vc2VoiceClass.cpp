// The VC-2 part's note voice's class: see Vc2VoiceClass.h (v279).  Each routine's reads, calls and writes in rom.4's
// order.
#include "Vc2VoiceClass.h"
#include "Vc2Keys.h"
#include "Vc2Notes.h"

namespace {
constexpr uint32_t kChannelAt = 0x34, kByteAt = 0x53;      // [FW] the voice's words C's struct points at
constexpr uint32_t kAgeMask = 0x0fffffff, kAgeMarks = 0x70000000, kSecondAge = 0x5fffffff;   // [FW] a note's ages
constexpr uint8_t kAllHeld = 7;                            // [FW] the key and both pedals
constexpr uint16_t kUpdateAsked = 1;                       // [FW] the +0x30 bit the change test leaves alone
} // namespace

Vc2SetCall Vc2VoiceClass::setCall(uint32_t voice, uint32_t word) {
    return { voice + kChannelAt, voice + kByteAt, word };
}

// The words both routines set from the message's key and its table: the age, the velocity, the key's note-on count
void Vc2VoiceClass::keyWords(uint32_t voice, uint32_t message, Vc2PartVoices& v, Vc2Keys& keys) {
    Vc2VoiceNode& n = v.at(voice);
    const Vc2KeyGlide& glide = keys.glide(message);
    v.ages += 1;
    n.order[0] = (v.ages & kAgeMask) | kAgeMarks;
    n.order[1] = kSecondAge;
    n.byte2f = keys.table(glide.table).onVelocity;
    n.byte2e = keys.table(glide.table).ons[(uint8_t)glide.key];
}

void Vc2VoiceClass::prepareWords(uint32_t voice, const Vc2PrepareCall& c, Vc2PartVoices& v, Vc2Keys& keys) {
    Vc2VoiceNode& n = v.at(voice);
    n.channel = keys.glide(c.message).table;
    n.source = c.message;
    n.part = c.part;
    n.key = (uint8_t)c.onByte;
    n.byte42 = (uint8_t)keys.glide(c.message).key;
    n.byte43 = (uint8_t)c.passed;
    keyWords(voice, c.message, v, keys);
    n.held = kAllHeld;
}

void Vc2VoiceClass::retuneWords(uint32_t voice, uint32_t message, uint32_t onByte, Vc2PartVoices& v, Vc2Keys& keys) {
    Vc2VoiceNode& n = v.at(voice);
    const Vc2KeyGlide& glide = keys.glide(message);
    n.key = (uint8_t)onByte;
    n.byte42 = (uint8_t)glide.key;
    n.byte43 = keys.table(glide.table).onVelocity;
    keyWords(voice, message, v, keys);
    n.held |= kAllHeld;
}

void Vc2VoiceClass::update(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts) {
    Vc2Voice& w = v.voice(voice);
    if (w.set == 0 || w.node.note == 0) return;
    parts.lock();
    const Vc2SetCall call = setCall(voice, 0);
    if (parts.refreshSet(w.set, call) != 0) w.word58 = 1;
    parts.updateSet(w.set, call);
    parts.unlock();
    parts.copySet(w.set, w.node.note);
    parts.noteUpdate(w.node.note);
}

void Vc2VoiceClass::retune(uint32_t voice, uint32_t message, uint32_t onByte, Vc2PartVoices& v, Vc2Keys& keys,
                           Vc2VoiceClassParts& parts) {
    Vc2Voice& w = v.voice(voice);
    parts.waitQueue(kQueueAt + kQueueBytes * w.node.queue, voice);
    retuneWords(voice, message, onByte, v, keys);
    w.byte55 = 1;
    if (w.set == 0 || w.node.note == 0) return;
    parts.convertSet(w.set, setCall(voice, parts.retuneWord()), w.node.note);
    parts.noteBlocks(w.node.note);
}

void Vc2VoiceClass::destroy(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts) {
    Vc2Voice& w = v.voice(voice);
    parts.takeOff(kQueueAt + kQueueBytes * w.node.queue, voice);
    if (w.node.note != 0) {
        parts.endNote(w.node.note);
        w.node.note = 0;
    }
    if (w.set != 0) {
        parts.endSet(w.set);
        w.set = 0;
    }
    w.node.updated = 0;
}

void Vc2VoiceClass::keyOff(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts) {
    Vc2Voice& w = v.voice(voice);
    parts.waitQueue(kQueueAt + kQueueBytes * w.node.queue, voice);
    if (w.set == 0 || w.node.note == 0) return;
    parts.releaseSet(w.set, w.node.byte2f, w.node.note);
    parts.noteRelease(w.node.note);
}

void Vc2VoiceClass::prepare(uint32_t voice, const Vc2PrepareCall& c, Vc2PartVoices& v, Vc2Keys& keys,
                            Vc2VoiceClassParts& parts) {
    Vc2Voice& w = v.voice(voice);
    prepareWords(voice, c, v, keys);
    w.node.released = 1;
    w.byte55 = w.byte50;
    w.byte53 = w.byte51;
    w.byte54 = w.byte52;
    w.node.updated = 0;
    w.word58 = 0;
    const uint32_t note = Vc2Notes::kNotesAt + (uint32_t)sizeof(Vc2NoteWords) * w.node.number;
    parts.constructNote(note, parts.firstByte(w.node.part), c.kind);
    w.node.note = note;
    const uint32_t set = parts.placeSet(w.node.number);
    if (set != 0) parts.makeSet(set, setCall(voice, c.legatoAt), w.node.note);
    w.set = set;
}

uint32_t Vc2VoiceClass::changed(uint32_t voice, Vc2PartVoices& v, Vc2VoiceClassParts& parts) {
    Vc2Voice& w = v.voice(voice);
    if ((w.node.updated & kUpdateAsked) != 0) return 0;
    parts.enqueue(kQueueAt, voice);
    if (w.set == 0) return 0;
    if (parts.refreshSet(w.set, setCall(voice, 0)) == 0 && w.word58 == 0) return 0;
    w.word58 = 0;
    return 1;
}
