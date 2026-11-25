#pragma once
// The VC-2's part played in the plugin (v455; HARDWARE_FIRMWARE.md, "The VC-2 in the plugin"): Vc2Synth on its OS model,
// made and started as xt_vc2synth's whole part makes its own (rom.4's start, rom.4's system defaults, so Part 1 Receive
// Switch on, or the plugin's system over them (v457), the waves loaded, the tasks on their OS), fed the host's MIDI and
// blocks in time order, its bus heard at the host's rate.
// - The front's stand-in: at the start, the parameters' messages a front sends for the system (kFrontStart: the
//   calibration's, the system's with part 1's receive channel, the assignments', the routing's reset), each the front's
//   to send through the OS MIDI task as xt_vc2synth's whole part sends them; then a patch as Vc2Synth::patchGiven.  Which
//   messages rom.4's own front sends, and in what order, isn't traced (VC2_MODE_PLAN.md, step 4.5's "The front").
// - The host's channel messages go in as MIDI IN's (source 1), as their bytes, before the block they come in front of.  A
//   block is the 32 frames the read needs next (XtRateOut), so a message waits a block at most.
// - A block's input: the host's through XtHostInput, 32 frames at 44.1 kHz, a block late at 44.1 kHz, read at the block's
//   own place (the nth block since the input's start reads its frames from 32n), zeros where the host hasn't given them;
//   each frame's words 1 (the mic, kind 6's ring) and 5 (the tracker) the mean of its L and R (vc2InputWord), the rest 0.
//   Which slots the two are on an XT waits on a capture (VC2_MODE_PLAN.md, step 5's items 2 and 4).
// - The bus out: MAIN and MULTI as rom.4's unit sums them, a word at the XT's full scale heard at 2^-30 of it (the unit's
//   +6 dB on rom.4: HARDWARE_FIRMWARE.md, "rom.4's frame through the unit"), DIR and the sends nowhere.
// - A wait inside a pass (the notes' task's on a fade) takes the blocks after the one asked for, running ahead of the
//   host: their input is what the host has given at their places, mostly none, and the host's input there, when it
//   comes, is passed over, so the input keeps its latency.  Their frames wait in XtRateOut ahead of the host, a second's at
//   most, and the next reads hear them first; meanwhile the host's messages wait for the next block asked for.  A wait
//   that would run past the second stops the part (stuck), silent from then; the plugin makes another (PluginVc2Play).
// Audio thread, but the constructor and prepare.
#include "Vc2PartView.h"
#include "Vc2Synth.h"
#include "XtHostInput.h"
#include "XtRateOut.h"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

struct Vc2LoadedWaves;

// A word of the part's input frames from a host frame's two sides as XtHostInput gives them: their mean, into words
// kMicWord and kTrackerWord (below), the rest 0, silence exactly 0.  UNSOURCED: a stand-in, the coordinator's call
// (2026-11-25), until step 5's captures say which slot the mic comes back on and which inputs the frame's +4 and +0x14
// are (VC2_MODE_PLAN.md, step 5's items 2 and 4; step 4.5's [gap] "The part's input frames"): this rule, here alone
inline int32_t vc2InputWord(int32_t l, int32_t r) { return (int32_t)(((int64_t)l + (int64_t)r) / 2); }

// [FW] a parameters' message (its class, 0x40) and its word: a kind 0…13 and a second byte (Vc2MidiDispatch.cpp)
inline constexpr uint8_t kVc2ParametersMessage = 0x40;
inline constexpr uint32_t kVc2RoutingOn = 0x80 << 8;   // kind 0's second byte: the routing's reset
// [FW] the kinds: 3 the calibration's dispatcher and the voices' counts, 4 the system's dispatcher (part 1's receive
// channel, Patch Remain), 5 the assignments' (Vc2PartSettingsMessages.cpp)
inline constexpr uint32_t kVc2Calibration = 3, kVc2SystemRead = 4, kVc2AssignsRead = 5;
// The front's stand-in's messages at the part's start, in their order: the calibration's, the system's, the
// assignments', and kind 0 with the routing's reset
inline constexpr uint32_t kVc2FrontStart[] = { kVc2Calibration, kVc2SystemRead, kVc2AssignsRead, 0 | kVc2RoutingOn };

class Vc2PartPlayer final : private Vc2BlockSink {
public:
    static constexpr int kBlock = 32, kWords = 8;
    static constexpr int kMicWord = 1, kTrackerWord = 5;              // [FW] the part mixer's +4 and +0x14; UNSOURCED inputs
    static constexpr int kMainL = 0, kMainR = 1, kMultiL = 4, kMultiR = 5;   // [FW] the bus frame's words
    static constexpr float kLevel = 0x1p-30f;                         // a word's level out, the unit's on rom.4
    static constexpr size_t kMostAhead = 44100;                       // frames a wait may run ahead: a second
    static constexpr size_t kMessages = 1024;                         // the host's messages waiting for a block
    // Not on the audio thread, rom.4's tables in: the part made and started on its OS, with `waves` if loaded, at the
    // host's rate, on `system` (v457), rom.4's defaults when none
    Vc2PartPlayer(const Vc2LoadedWaves* waves, double hostRate, const Vc2System* system = nullptr);
    ~Vc2PartPlayer() override;
    Vc2PartPlayer(const Vc2PartPlayer&) = delete;
    Vc2PartPlayer& operator=(const Vc2PartPlayer&) = delete;
    // Not on the audio thread: the host's rate, for the output and the input
    void prepare(double hostRate);
    // A patch as Vc2Synth::patchGiven gives it, at a block's edge: `store`'s kVc2StoreBytes copied, or none for every ID's
    // default
    void patchGiven(const uint8_t* store);
    uint64_t patchSeq = 0;   // the plugin's number of the patch given last (PluginVc2Play's)
    // v457: the system as a front's edit of it gives it, at a block's edge: copied into the part, then the system's
    // message (kind 4) when system B's area moved, the assignments' (kind 5) when the system's did, both through the OS
    // MIDI task before the next block.  Which messages rom.4's front sends on a system edit isn't traced (the stand-in's)
    void systemGiven(const Vc2System& system, bool systemB, bool assigns);
    uint64_t systemSeq = 0;   // v457: the plugin's number of the system given last
    Vc2PartView* view = nullptr;   // v456: written after each block heard (PluginVc2Play's); none, nothing written
    // A channel message, before the frames rendered after it; false when the queue is full and it's dropped
    bool message(uint8_t status, uint8_t data1, uint8_t data2);
    // A piece of the host's input, `n` frames, most() at the most (inL and inR, both nullptr for none; inR nullptr with
    // inL a mono input), before the frames out that it covers are rendered: a block's input is what the pieces given
    // so far hold at its place
    void input(const float* inL, const float* inR, int n);
    int most() const { return in_.most(); }
    // `n` host frames of the part out
    void render(float* left, float* right, int n);
    bool stuck() const;
    const Vc2LoadedWaves* waves() const { return waves_; }
    Vc2Synth& part() { return *part_; }
    // What happened: blocks given (ahead of the host's ask among them), messages given and dropped, input frames dropped,
    // blocks whose frames found no room
    struct Counts {
        long blocks = 0, ahead = 0, messages = 0, dropped = 0, inputDropped = 0, lost = 0;
    };
    const Counts& counts() const { return counts_; }

private:
    // A block's input outlives the waits inside it: each block given takes the next of these, and gives it back when
    // mixed, so a block nested in another's wait never writes over the outer one's (a stack by depth, kInputs deep)
    static constexpr size_t kInputs = 16;
    void mixed(const float* frames) override;
    void viewed(const float* lr);   // v456: the view, after a block's frames `lr` (L, R) are out
    bool next(Vc2PartEvent& e);
    void produce();
    const int32_t* inputOf(long n);
    const Vc2LoadedWaves* waves_;
    std::unique_ptr<Vc2Synth> part_;
    XtRateOut out_;
    XtHostInput in_;
    std::vector<uint8_t> store_;
    std::array<Vc2MidiMessage, kMessages> queue_ {};
    std::array<uint32_t, kMessages> words_ {};   // each message's word: a parameters' message's kind, else 0
    bool queue(const Vc2MidiMessage& m, uint32_t word);
    size_t head_ = 0, tail_ = 0;
    std::array<std::array<int32_t, kBlock * kWords>, kInputs> inputs_ {};
    size_t inputDepth_ = 0;   // the blocks given and not yet mixed
    bool wanted_ = false;    // the block the read asked for not yet given
    long inputBlocks_ = 0;   // the blocks given since the input's start (prepare)
    Counts counts_;
};
