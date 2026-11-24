#pragma once
// The VC-2's part as one object (v314, step 4.3's forty-second version, item 10's nineteenth part): every port's object
// of the part, each as rom.4 lays it out at its own address, and the part's start, as rom.4's static initializers
// leave them in its table's order (the OS MIDI's two rows, then the part's from 0x898c5cc0), with the start values no
// port's defaults give: the .data words (the voices' counts, C's TVA step, the update task's elapsed count, the two
// Receive flags), the .bss nodes' channel 0 before the part's init, and the initializers no port runs (the pitch
// source, the notes' unread word, robot human depth 1.0, the chaos generator 0.7).  Built after loadVc2: the tracker,
// the vocoder and its engines read their tables when constructed.  The tones' own parts (their OSC voices, COSMs, TVA,
// mix and modulators) and C's OSC-block and COSM-input children come next, then the tasks and the block.  Not in the
// sound yet.  Bit for bit: xt_vc2synth.  HARDWARE_FIRMWARE.md, "The VC-2 part as one object: Vc2Synth's start".
// v340: the 24 tones' hosts (Vc2SynthTone) on the part's words, with the tones' render buffers, the part mixer's
// frames and the shared routines; the part is their world (HARDWARE_FIRMWARE.md, "Vc2Synth's tones").
#include "Vc2Assigner.h"
#include "Vc2AutoNote.h"
#include "Vc2CosmPools.h"
#include "Vc2FreeLfos.h"
#include "Vc2Keys.h"
#include "Vc2Kind6.h"
#include "Vc2MatrixSetup.h"
#include "Vc2Notes.h"
#include "Vc2OsMidi.h"
#include "Vc2PartMixer.h"
#include "Vc2PartNodes.h"
#include "Vc2PartNumbers.h"
#include "Vc2PartReloads.h"
#include "Vc2PartSettings.h"
#include "Vc2PartStartUp.h"
#include "Vc2PartVoices.h"
#include "Vc2SetCommon.h"
#include "Vc2SetCosmInput.h"
#include "Vc2SetEnvelope.h"
#include "Vc2SetLfo.h"
#include "Vc2SetMix.h"
#include "Vc2SetOscBlock.h"
#include "Vc2SetTva.h"
#include "Vc2Sets.h"
#include "Vc2StaticMatrix.h"
#include "Vc2SynthTone.h"
#include "Vc2SystemStore.h"
#include "Vc2TimeTrip.h"
#include "Vc2Tone.h"
#include "Vc2ToneLists.h"
#include "Vc2ToneMath.h"
#include "Vc2ToneMix.h"
#include "Vc2Tracker.h"
#include "Vc2TrackerAnalysis.h"
#include "Vc2UpdateTask.h"
#include "Vc2Vocoder.h"
#include "Vc2VoiceClass.h"
#include "Vc2WaveSets.h"
#include <deque>
#include <memory>
#include <vector>

struct Vc2LoadedWaves;
struct Vc2Synth;
class Vc2SynthReads;
class Vc2SynthNotes;
class Vc2SynthVoices;
class Vc2SynthControls;
class Vc2SynthSettings;
class Vc2SynthMidi;
class Vc2SynthUpdate;

// [FW] v345: the notes' task's event object and what the part posts it through the OS (0x89301830): the ended scan's
// (the notes' hook word's routine 0x893901a0), all notes off (0x89391e30) and the held note's (0x89391e50); where the
// part keeps the legato's struct a voice's prepare gives C (rom.4's on the notes' task's stack)
inline constexpr uint32_t kVc2SynthNotesTask = 0x8994fec8, kVc2SynthEndedHook = 0x893901a0;
inline constexpr uint32_t kVc2SynthEndedEvent = 0x004, kVc2SynthAllNotesOff = 0x080, kVc2SynthHeldNoteEvent = 0x200;
inline constexpr uint32_t kVc2SynthLegatoAt = 0x8C0FFF00;

// What the reload objects reach: the OS's timer queue (0x8994fc24), which the tasks' wiring gives its work, and from
// v345 the part's read (0x8939b170, Vc2SynthSettings)
class Vc2SynthReloads final : public Vc2ReloadParts {
public:
    explicit Vc2SynthReloads(Vc2Synth& s) : s_(s) {}
    void post(uint32_t) override {}
    void unpost(uint32_t) override {}
    bool readPart(int part) override;

private:
    Vc2Synth& s_;
};

struct Vc2Synth final : Vc2SynthToneWorld {
    Vc2Synth();
    ~Vc2Synth();
    Vc2Synth(const Vc2Synth&) = delete;   // its tones' hosts hold the part
    Vc2Synth& operator=(const Vc2Synth&) = delete;
    // rom.4's start: C startup's .data and .bss, then the static initializers in its table's order
    void start();
    // v343: the waves the process has loaded (vc2Waves()): their memory and store, the sets copied, as the part's
    // lookups write their buffers
    void giveWaves(const Vc2LoadedWaves& w);
    // v345: the patch store the part's parameters read (Vc2PatchStore's bytes), none for every ID's default
    void givePatch(const std::vector<uint8_t>* store) { patch = store; }
    // v347: the system's three areas (vc2SystemDefaults(), or a system's own), copied in, as rom.4's store object aims
    // its system read objects at them; until then, every system ID its default
    void giveSystem(const Vc2System& given);
    // v345: the notes' task's start to its loop (0x8938fc00…0x8938fc66) as the part has it: the notes' init
    // (0x89396960), the hook word the notes' task's, the start-up (0x89392ab0), the note-on gate open
    void taskStart();
    // v346: a MIDI message (its four bytes and word) as rom.4 takes it: the OS MIDI's handler, then the dispatcher
    void midiMessage(const Vc2MidiMessage& m, uint32_t word);
    // v349: the update task's pass (vc2UpdateTaskPass) on the events its wait answers (0x2 the timer's, 0x1 the
    // update's), the ticks as `ticks` stands
    void updatePass(uint32_t events);
    // v350: the notes' task's pass (vc2NotesTaskPass) on the events its wait answers (mask 0x3fe: the queue, the ended
    // scan, the reset, the schedule, Auto Note's three, the poly pressure and the held note's)
    void notesPass(uint32_t events);
    // v345: a tone of the pool made (0x893b2db0(0, id)) through the host of the tone it takes, its address, 0 none
    uint32_t makeTone(uint32_t id);
    Vc2SynthTone& hostOf(uint32_t tone) { return hosts[(size_t)tones.indexOf(tone)]; }
    // v345: an OS event set on its object (0x89301830): the notes' task's the part keeps
    void post(uint32_t object, uint32_t flags) {
        if (object == kVc2SynthNotesTask) notesEvents |= flags;
    }
    // v340: the tones' world.  The tail's routines the notes' (v345), the OS's event posted; the truncation's wait the
    // tasks' version's: until it, a wait ends the fade
    const Vc2ToneHost& host(int tone) override { return hosts[(size_t)tone]; }
    void osEvent(uint32_t event, uint32_t flags) override { post(event, flags); }
    void ended(int tone) override;
    void truncated(int tone) override;
    void waitFade(int tone) override { hosts[(size_t)tone].tva.w.fade = 0.0f; }
    int32_t osClock() override { return clock; }
    Vc2MicRing& micRing() override { return ring; }
    const Vc2Unvoiced& inputStage() override { return vocoder.stage; }
    const int32_t* inputFrames() override { return input; }
    const Vc2WaveMemory* waveMemory() override { return waves; }
    Vc2OscSamples waveSamples() override { return samples; }

    Vc2OsMidi midi {};                     // 0x8995eaf4, 0x8995edf8, 0x8992e3e8 (v313)
    Vc2PartVoices voices {};               // 0x89981b6c…0x89982710 (v272)
    Vc2Keys keys {};                       // 0x89982720…0x899888ac (v276)
    Vc2Notes notes {};                     // 0x8998a514… (v275)
    Vc2UpdateQueue queue {};               // 0x89989cc4 (v279, v312)
    Vc2Sets sets {};                       // C 0x89997fe8, the notes' M 0x89e4741c (v280)
    Vc2SetCommons commons {};              // 0x89996f68 (v281)
    Vc2SetEnvelopes envelopes {};          // 0x89e2543c (v282)
    Vc2SetLfos lfos {};                    // 0x899962b0 (v283)
    Vc2SetMixes mixes {};                  // 0x89996c34 (v284)
    Vc2SetTvas tvas {};                    // 0x89997c04, its step 0x8993eb08 (v285)
    Vc2SetOscBlocks oscBlocks {};          // 0x89e34060, Random Pitch's seed 0x89996f4c (v391, v343)
    Vc2SetCosmInputs cosmInputs {};        // 0x899955a8 (v390, v343)
    Vc2PartRows rows {};                   // the part's settings' tables, each at its own address (v286…)
    Vc2MatrixSettings g {};                // 0x89997398 (v247)
    Vc2StaticMatrix staticM {};            // 0x89997108 (v295)
    Vc2PartNodes nodes {};                 // 0x899888d0, 0x899890d0, 0x899888bd, 0x8998246c (v293)
    Vc2PartNumbers numbers {};             // 0x89988cd0, 0x899891e0 (v302)
    Vc2HeldFlags held {};                  // 0x8993d0c0 (v300)
    Vc2SynthReloads reloadParts { *this };
    Vc2PartReloads reloads { held, reloadParts };   // 0x89996120 (v300)
    Vc2TripRecords trips {};               // 0x89989408, 0x899996a4 (v306)
    std::array<uint32_t, 16> partTable {};   // 0x8993d040: a part's trip record by number, .data (v343)
    std::array<uint8_t, 0x4c> clockObject {};   // 0x8995cb40: the OS's clock object, as far as C reads it (v343)
    Vc2PartStartWords startWords {};       // 0x899893f8… (v304)
    Vc2PitchSource pitch {};               // 0x89989748 (v226)
    uint32_t noteGate = 0;                 // 0x89981904: no note-on while set (v308)
    uint32_t scheduled = 0;                // 0x89981b60: the schedule's count (v309)
    std::array<uint32_t, 2> autoNoteKeys {};   // 0x8993d034, 0x8993d038: Auto Note's note-on and note-off keys (v309)
    // v346: the audio task's object (0x8998a480, .bss, row 16's 0x89396470: +8 its class, +0x18 the stereo source's
    // flag, +0x24 Out DC cut for TG; past it, 0x8998a4b4…, the block's), SysEx's flag (0x8992e3d1, .data 1)
    static constexpr size_t kAudioClassWord = 8 / 4, kDcCutWord = 0x24 / 4;
    std::array<uint32_t, 0x34 / 4> audioTask {};
    uint8_t sysexOn = 0;
    uint32_t ticks = 0;                    // 0x8993d080: the update task's timer's ticks (v310)
    Vc2TaskTime time {};                   // 0x8993d084 (v310)
    Vc2TonePool tones {};                  // 0x8999f524, its head and serial 0x8993fb48 (v228)
    Vc2ToneLists lists {};                 // 0x89999678… (v273)
    Vc2ToneMixRings rings {};              // 0x8a0af984, 0x8a0bb384 (v269)
    Vc2FreeLfos freeLfos {};               // 0x8a0c6d84 (v271)
    Vc2CosmPools pools {};                 // 0x89999290 (v250)
    Vc2PartMixer mixer {};                 // 0x8993eac4, 0x8999f1d0, 0x89934550, 0x89997290 (v227)
    Vc2Vocoder vocoder {};                 // 0x89999758, its engines and stages (v220)
    Vc2TrackerAnalysis analysis {};
    Vc2Tracker tracker { analysis };       // 0x899d9b70 (v223, v224)
    Vc2AutoNote autoNote {};               // 0x8993fe3c, 0x899d994c (v226)
    Vc2MicRing ring {};                    // 0x8a0f31cc, 0x899d9968, 0x8993fe54 (v225)
    float chaos = 0.0f;                    // 0x89999668: the LFOs' chaos generator (v268)
    // v340: the tones' render buffers a, b and c (0x89998b20…0x89998e1f), the part mixer's 32 frames of 8 floats after
    // them (0x89998e20), the shared routines, the OS's millisecond word, the chip's frames for the block (none), the
    // waves given (the plugin's wiring takes them from vc2Waves(): their memory, the samples their pool holds), the
    // events posted, then the 24 tones' hosts on all of it (after it: they hold it)
    Vc2ToneBuffers buffers {};
    std::array<float, 32 * 8> frames {};
    Vc2ToneMath math {};
    int32_t clock = 0;
    const int32_t* input = nullptr;
    const Vc2WaveMemory* waves = nullptr;
    Vc2OscSamples samples {};
    uint32_t notesEvents = 0;   // v345: the flags posted the notes' task's event object (0x8994fec8)
    std::deque<Vc2SynthTone> hosts;
    // v343: the wave store given (an empty one, none given) and the part's copy of the sets (0x8993448c, 0x89973084,
    // 0x89989754), then C's reads on all of it
    Vc2WaveStore noStore {};
    const Vc2WaveStore* store = &noStore;
    Vc2WaveSets waveSets {};
    std::unique_ptr<Vc2SynthReads> reads;
    // v345: rom.4's code on the voices (the note voice's class, the assigner), the legato's struct a prepare gives C,
    // the patch store given (none: every ID its default), then the parts on all of it
    Vc2VoiceClass classes {};
    Vc2Assigner assigner {};
    std::array<uint8_t, 4> legato {};
    const std::vector<uint8_t>* patch = nullptr;
    Vc2System system {};      // v347: 0x89add9a8, 0x8996126d, 0x8995cc14 (.bss)
    bool hasSystem = false;   // rom.4's store object built (0x8931a640)
    std::unique_ptr<Vc2SynthNotes> noteParts;
    std::unique_ptr<Vc2SynthVoices> voiceParts;
    std::unique_ptr<Vc2SynthControls> controls;
    std::unique_ptr<Vc2SynthSettings> settings;
    std::unique_ptr<Vc2SynthMidi> midiParts;   // v346
    std::unique_ptr<Vc2SynthUpdate> updateParts;   // v349
};
