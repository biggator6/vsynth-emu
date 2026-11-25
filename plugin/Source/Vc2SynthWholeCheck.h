#pragma once
// xt_vc2synth's whole part (v355): what its two files share (v360). The part, its pairs, cases and compares in
// Vc2SynthWholeCheck.cpp; a pair's events, their draws, the machine given them and a step in
// Vc2SynthWholeEventsCheck.cpp (moved there as they stood), with v360's: the compare at MIDI IN's full ring, a pair's
// lead-in and a case's pitch. XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2MachineRig.h"
#include "Vc2Synth.h"
#include "Vc2SynthCheck.h"
#include "Vc2SynthOsTasks.h"
#include <array>
#include <deque>
#include <random>
#include <string>

// What the pairs did, and (tallied) the machines' counts, every pair's
struct Vc2SynthWholeCounts {
    long events = 0, blocks = 0, messages = 0, ticks = 0, nested = 0, sounding = 0, under = 0, kept = 0;
    long cuts = 0, bursts = 0, floods = 0, pairs = 0;
    // the machines', every pair's
    long loads = 0, suspends = 0, semaphoreWaits = 0, roomWaits = 0, emptyWaits = 0;
    std::array<long, 5> markers {};
    // v360: the lead-ins' events, the compares at MIDI IN's full ring, the cases on a pitch
    long leads = 0, fulls = 0, pitched = 0;
    void tally(const Vc2MachineRig& rig);
};

// Every region of the part, each tone's words, then the kernel's, the services', whether the port's model is stuck,
// and the senders' waits (Vc2SynthWholeCheck.cpp)
std::string vc2SynthWholeDifference(Vc2MachineRig& rig, Vc2Synth& s);

// The pair and the events: drawn as the port's model takes them, the machine given them after
struct Vc2SynthWhole {
    Vc2MachineRig& rig;
    Vc2Synth& s;
    std::mt19937& rng;
    Vc2SynthWholeCounts& n;
    std::mt19937& own;                  // v360: the lead-in's, the pitch's and its patch's steer, a generator of their own
    std::deque<Vc2PartEvent> events;
    std::deque<Vc2SynthInput> inputs;   // the blocks' frames, where their events point
    size_t taken = 0;                   // the events the port's model has taken
    size_t given = 0;                   // and the machine been given
    size_t stepFrom = 0;                // the step's first
    uint32_t msAcc = 0;                 // the machine's ms against its frames, as the port's model counts them
    uint32_t burst = 0;                 // a burst's messages still to come
    bool flooding = false, flooded = false;
    bool cutting = false;               // a wait cut: the pair can't go on
    std::string cut;                    // what differed where it was cut
    // v360
    bool leading = false;               // the lead-in's events, drawn by `own`
    uint32_t fullWrite = 0;             // MIDI IN's ring's write at the last look
    std::string atFull;                 // what differed inside a wait, at MIDI IN's full ring
    bool pitched = false;               // the case's blocks a pitch, `period` frames
    uint32_t period = 0, phase = 0;

    Vc2PartEvent burstMessage();
    Vc2PartEvent drawn();
    void burstDrawn();
    bool next(Vc2PartEvent& e);
    std::string give(const Vc2PartEvent& e);
    void waitCut();
    std::string step();
    void fullCompare();
    Vc2PartEvent leadDrawn();
    std::string leadIn(uint32_t ticks);
    void pitchOver(Vc2SynthInput& frames, uint32_t shift);
};
#endif
