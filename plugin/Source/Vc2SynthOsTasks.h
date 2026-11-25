#pragma once
// The part on its OS model (v355): Vc2SynthOs's parts on Vc2Synth and the driver that feeds it the outside's events in
// time order. A task's first run is the part's of its prologue (the audio task's the mixer's reset; the clock task's,
// the OS MIDI task's and the notes' task's listeners on the hub, the notes' start with its own alarm off; the MIDI input
// task's listener and its two cyclic records; the update task's cyclic handler and 10-tick alarm); a pass is the part's
// pass by its events. A block is its ms ticks (frames × 1000 against 44,100, as XtSynth's), then the input's and the
// output's DMA interrupts, each its handler's part (the buffers' indices, the block's count) then the watchdog's
// dispatch. A MIDI message on source 1 or 2 is its bytes, each the receive interrupt's into its receive object; any
// other the front's to send (Vc2SynthOsMidi). HARDWARE_FIRMWARE.md, "The part's tasks on the kernel".
#include "Vc2MidiDispatch.h"
#include "Vc2SynthOs.h"
#include "Vc2SynthOsMidi.h"
#include <cstdint>
#include <functional>
#include <utility>

struct Vc2Synth;

// An event from outside the part: a MIDI message and its word, or a block with its input's 32 frames of 8 words
struct Vc2PartEvent {
    bool block = true;
    Vc2MidiMessage message {};
    uint32_t word = 0;
    const int32_t* input = nullptr;
};

class Vc2SynthOsTasks final : public Vc2OsParts {
public:
    using Source = std::function<bool(Vc2PartEvent&)>;   // the next event; false when there is none
    // [FW] the input's and the output's DMA buffers (0x8cc3f800, 0x8cc3f000), a buffer's bytes (0x400)
    static constexpr uint32_t kInputBuffers = 0x8cc3f800u, kOutputBuffers = 0x8cc3f000u, kBufferShift = 10;
    static constexpr uint32_t kFadeEvent = 0x1000;   // [FW] a fade's wait (0x893b3be4)

    Vc2SynthOsTasks(Vc2Synth& s, Source source) : os(*this), midi(s, os), s_(s), source_(std::move(source)) {}
    Vc2SynthOs os;
    Vc2SynthOsMidi midi;
    // The tasks started, then the front's MIDI on
    void start();
    // The next event, the tasks it wakes run to their waits; false when there is none (and the model's stuck, said)
    bool deliverNext();
    void post(uint32_t object, uint32_t flags) { os.post(object, flags); }
    void raise(uint32_t object, uint32_t flags) { os.raise(object, flags); }
    void waitFade() { os.waitInPass(kFadeEvent); }
    // The notes' task's queue drained through `dispatch` (its waiters woken at its end)
    void drain(const XtFn<void(const Vc2MidiMessage&, uint32_t)>& dispatch) { midi.notesDrain(dispatch); }
    long blocks = 0, messages = 0, ticks = 0;   // what was delivered

    void prologue(int task) override;
    uint16_t pass(int task, uint32_t events) override;
    void cyclic(int handler) override;
    void appTick() override;
    void advance() override;

private:
    // Each task's start and pass, by Vc2SynthOs::Task
    struct Ways {
        void (Vc2SynthOsTasks::*start)();
        uint16_t (Vc2SynthOsTasks::*pass)(uint32_t);
    };
    static const Ways kWays[Vc2SynthOs::kIdle];
    void audioStart();
    void clockStart() { midi.clockStart(); }   // its listener on the hub; its handler and the OS's not the part's
    void notesStart();
    void updateStart();
    void inputStart() { midi.inputStart(); }
    void osMidiStart() { midi.osMidiStart(); }
    void frontStart() {}
    uint16_t audioPass(uint32_t events);
    uint16_t clockPass(uint32_t events);
    uint16_t notesPass(uint32_t events);
    uint16_t updatePass(uint32_t events);
    uint16_t inputPass(uint32_t events) { return midi.inputPass(events); }
    uint16_t osMidiPass(uint32_t events) { return midi.osMidiPass(events); }
    uint16_t frontPass(uint32_t events) { return midi.frontPass(events); }
    Vc2Synth& s_;
    Source source_;
    uint32_t msAcc_ = 0;
    const int32_t* input_ = nullptr;   // the block's input, the audio task's on its event 4
    void deliver(const Vc2PartEvent& e);
    void block(const int32_t* input);
    void message(const Vc2MidiMessage& m, uint32_t word);
    // An interrupt: its handler, then its end (v455: a template, so a handler's captures never allocate, the plugin's
    // audio thread running it)
    template <class Handler>
    void interrupt(Handler&& handler) {
        handler();
        os.interruptEnd();
    }
    void inputDone();    // 0x89395cf0's part
    void outputDone();   // 0x89395d50's part
};
