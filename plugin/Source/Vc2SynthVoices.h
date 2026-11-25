#pragma once
// The part's voices' and notes' parts, the part's own (v345, Vc2Synth whole's sixth version): what the voices, the
// assigner, the note voice's class and the update queue call beyond themselves, and what a note's routines call on its
// tone, each to the part's port at rom.4's call.  A voice's virtuals by its class (its +0x10): the note voice's
// (0x898cd3dc) Vc2VoiceClass's, the plain voice's (0x898cd1a8) six small routines (Vc2SynthVoices.cpp).  The OS's
// lock, unlock, wait, signal and post do nothing: the part runs on one thread.  The legato's struct, which rom.4 keeps
// on the notes' task's stack and C reads by its address, is the part's own at a word of its own.  A tone by its
// address in the pool, its host the part's.  Bit for bit: xt_vc2synth's load part (v345) and notes part (v346).
// HARDWARE_FIRMWARE.md, "The part's settings and voices wired".  The notes' task.
#include "Vc2PartVoicesTables.h"
#include "Vc2Synth.h"
#include "Vc2SynthReads.h"

// The tones a note's routines reach: the pool's, by address, each with its host; and the pool's owner a note-on asks
class Vc2SynthNotes final : public Vc2NoteParts, public Vc2ToneOwner {
public:
    explicit Vc2SynthNotes(Vc2Synth& s) : s_(s) {}
    uint32_t make(uint32_t id) override;
    void giveRoutines(uint32_t tone, uint32_t ended, uint32_t truncated) override {
        s_.tones.at(tone).tail.ended = ended;
        s_.tones.at(tone).tail.truncated = truncated;
    }
    int32_t serial(uint32_t tone) override { return s_.tones.at(tone).head.serial; }
    bool endTest(uint32_t tone) override { return s_.tones.at(tone).held(host(tone)); }
    void unmake(uint32_t tone) override { s_.tones.unmake(s_.tones.at(tone), true, host(tone)); }
    void truncateAndWait(uint32_t tone) override { s_.tones.at(tone).truncateAndWait(host(tone)); }
    void truncateAtOnce(uint32_t tone) override { s_.tones.at(tone).truncateAtOnce(host(tone)); }
    void noteOn(uint32_t tone, const Vc2NoteWords& note) override;
    void start() override { s_.lists.start(s_.tones); }
    void setBlock(uint32_t tone, const Vc2NoteBlockRow& row, const uint32_t* block) override;
    void keyOff(uint32_t tone) override { s_.tones.at(tone).keyOff(host(tone)); }
    uint32_t position(uint32_t tone, uint32_t osc) override { return s_.tones.at(tone).oscPosition(osc, host(tone)); }
    void call(uint32_t routine, uint32_t tone) override;
    std::array<uint32_t, 2> lists() override { return s_.lists.marks(); }
    Vc2ToneParts& partsOf(int tone) override { return s_.hosts[(size_t)tone]; }

private:
    Vc2SynthTone& host(uint32_t tone) { return s_.hostOf(tone); }
    Vc2Synth& s_;
};

class Vc2SynthVoices final : public Vc2AssignParts, public Vc2VoiceClassParts, public Vc2QueueParts {
public:
    explicit Vc2SynthVoices(Vc2Synth& s) : s_(s) {}
    // The voices' (Vc2VoiceParts): the destroy by the voice's class, a note's tone and kind, a tone's end and cost
    void destroy(uint32_t voice) override;
    uint32_t toneOf(uint32_t note) override { return s_.notes.at(note).tone; }
    int8_t kindOf(uint32_t note) override { return static_cast<int8_t>(s_.notes.at(note).kind); }
    bool ended(uint32_t tone) override { return s_.tones.at(tone).held(s_.hostOf(tone)); }
    float cost(uint32_t tone) override { return s_.tones.at(tone).cost(s_.hostOf(tone)); }
    // The assigner's: the voice's virtuals by its class, the note's routines, the reads
    void prepare(uint32_t voice, const Vc2AssignCall& call, const Vc2Legato& legato) override;
    void retune(uint32_t voice, uint32_t message, uint32_t part, uint32_t value) override;
    void noteOn(uint32_t note, uint32_t partner, int32_t onByte) override {
        s_.notes.noteOn(note, partner, (uint32_t)onByte, *s_.noteParts);
    }
    void startNote(uint32_t) override { s_.notes.start(*s_.noteParts); }
    void truncateOnce(uint32_t note) override { s_.notes.truncateOnce(note, *s_.noteParts); }
    int8_t onByteOf(uint32_t note) override { return static_cast<int8_t>(s_.notes.at(note).onByte); }
    uint32_t tableOf(uint32_t message) override;
    void waitQueue(uint32_t, uint32_t voice) override { s_.queue.wait(voice, *this); }
    void keyOff(uint32_t voice) override;
    uint8_t partSetting(uint32_t part, uint32_t offset) override { return s_.rows.modes[32 * part + offset]; }
    void readParameters(uint32_t part) override;
    uint8_t firstByte(uint32_t object) override;
    int32_t clock() override { return s_.clock; }
    uint32_t changed(uint32_t voice, uint32_t part) override;
    void allNotesOff() override { s_.post(kVc2SynthNotesTask, kVc2SynthAllNotesOff); }
    // The note voice's class's: the note's routines, C's, the queue's
    void constructNote(uint32_t note, uint32_t part, uint32_t kind) override {
        s_.notes.construct(note, part, kind, *s_.noteParts);
    }
    void endNote(uint32_t note) override { s_.notes.end(note, *s_.noteParts); }
    void noteBlocks(uint32_t note) override { s_.notes.setBlocks(note, *s_.noteParts); }
    void noteUpdate(uint32_t note) override { s_.notes.setBlocks(note, *s_.noteParts); }
    void noteRelease(uint32_t note) override { s_.notes.release(note, *s_.noteParts); }
    uint32_t placeSet(uint32_t number) override { return s_.sets.addressOf(number); }
    void makeSet(uint32_t set, const Vc2SetCall& call, uint32_t note) override {
        s_.sets.make(set, call, note, s_.notes, matrixIn(), *s_.reads);
    }
    void endSet(uint32_t set) override { s_.sets.end(set, *s_.reads); }
    void convertSet(uint32_t set, const Vc2SetCall& call, uint32_t note) override {
        s_.sets.convert(set, call, note, s_.notes, *s_.reads);
    }
    void releaseSet(uint32_t set, uint32_t velocity, uint32_t note) override {
        s_.sets.release(set, velocity, note, *s_.reads);
    }
    void updateSet(uint32_t set, const Vc2SetCall& call) override { s_.sets.update(set, call, matrixIn(), *s_.reads); }
    void copySet(uint32_t set, uint32_t note) override { s_.sets.copy(set, note, s_.notes, *s_.reads); }
    uint32_t refreshSet(uint32_t set, const Vc2SetCall& call) override { return s_.sets.refresh(set, call, *s_.reads); }
    void lock() override {}
    void unlock() override {}
    void enqueue(uint32_t, uint32_t voice) override { s_.queue.enqueue(voice, s_.voices, *this); }
    void takeOff(uint32_t, uint32_t voice) override { s_.queue.takeOff(voice, s_.voices, *this); }
    uint32_t retuneWord() override { return vc2PartVoicesTables().retuneWord; }
    // The queue's: the OS's semaphores, nothing on one thread
    void wait(uint32_t) override {}
    void signal(uint32_t) override {}
    void post(uint32_t) override {}

private:
    bool noteClass(uint32_t voice) { return s_.voices.at(voice).vtable == Vc2Assigner::kNoteVtable; }
    Vc2SetMatrixIn matrixIn() { return { s_.g, s_.staticM.m, s_.staticM.links }; }
    Vc2Synth& s_;
};
