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
#include "Vc2SetEnvelope.h"
#include "Vc2SetLfo.h"
#include "Vc2SetMix.h"
#include "Vc2SetTva.h"
#include "Vc2Sets.h"
#include "Vc2StaticMatrix.h"
#include "Vc2SynthTone.h"
#include "Vc2TimeTrip.h"
#include "Vc2Tone.h"
#include "Vc2ToneLists.h"
#include "Vc2ToneMath.h"
#include "Vc2ToneMix.h"
#include "Vc2Tracker.h"
#include "Vc2TrackerAnalysis.h"
#include "Vc2UpdateTask.h"
#include "Vc2Vocoder.h"
#include <deque>

// What the reload objects reach: the OS's timer queue (0x8994fc24) and the part's reader.  Nothing reaches them at the
// start; the tasks' wiring gives them their work
class Vc2SynthReloads final : public Vc2ReloadParts {
public:
    void post(uint32_t) override {}
    void unpost(uint32_t) override {}
    bool readPart(int) override { return true; }
};

struct Vc2Synth final : Vc2SynthToneWorld {
    Vc2Synth();
    Vc2Synth(const Vc2Synth&) = delete;   // its tones' hosts hold the part
    Vc2Synth& operator=(const Vc2Synth&) = delete;
    // rom.4's start: C startup's .data and .bss, then the static initializers in its table's order
    void start();
    // v340: the tones' world.  The tail's routines, the truncation's wait and the OS's event are the tasks' version's:
    // until it, the events are kept and a wait ends the fade
    const Vc2ToneHost& host(int tone) override { return hosts[(size_t)tone]; }
    void osEvent(uint32_t event, uint32_t) override { events |= event; }
    void ended(int) override {}
    void truncated(int) override {}
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
    Vc2PartRows rows {};                   // the part's settings' tables, each at its own address (v286…)
    Vc2MatrixSettings g {};                // 0x89997398 (v247)
    Vc2StaticMatrix staticM {};            // 0x89997108 (v295)
    Vc2PartNodes nodes {};                 // 0x899888d0, 0x899890d0, 0x899888bd, 0x8998246c (v293)
    Vc2PartNumbers numbers {};             // 0x89988cd0, 0x899891e0 (v302)
    Vc2HeldFlags held {};                  // 0x8993d0c0 (v300)
    Vc2SynthReloads reloadParts {};
    Vc2PartReloads reloads { held, reloadParts };   // 0x89996120 (v300)
    Vc2TripRecords trips {};               // 0x89989408, 0x899996a4 (v306)
    Vc2PartStartWords startWords {};       // 0x899893f8… (v304)
    Vc2PitchSource pitch {};               // 0x89989748 (v226)
    int32_t noteGate = 0;                  // 0x89981904: no note-on while set (v308)
    uint32_t scheduled = 0;                // 0x89981b60: the schedule's count (v309)
    std::array<int32_t, 2> autoNoteKeys {};   // 0x8993d034, 0x8993d038: Auto Note's note-on and note-off keys (v309)
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
    uint32_t events = 0;
    std::deque<Vc2SynthTone> hosts;
};
