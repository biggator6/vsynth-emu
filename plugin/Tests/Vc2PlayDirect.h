#pragma once
// The VC-2's part driven directly, the reference VSynthEmuVc2PlayTest holds the processor's VC-2 mode to (v455): a Vc2Synth
// started as xt_vc2synth's whole part starts its own, given a patch through its own steps (Vc2Synth::patchGiven), and fed
// a host's frames by the player's rules as Vc2PartPlayer.h states them, written out again here at 44.1 kHz, where a host
// frame is a part's frame (v457: and its system moved as Vc2PartPlayer::systemGiven gives it, `systemEdit`): the host's
// channel messages go in before the first block made once the host has reached their
// frame, on channel 1 (system and real-time bytes not taken); the nth block takes the input's frames from 32n, the input a
// block late (32 frames of silence, then the host's), zeros where the host hasn't given them (a block in a wait, run ahead
// of the host), each frame's words 1 and 5 the mean of its sides.  The bus: MAIN and MULTI × 2^-30.
#include "../Source/PluginVc2Fx.h"
#include "../Source/PluginVc2Patch.h"
#include "../Source/Vc2PartPlayer.h"
#include "../Source/Vc2SynthOsTasks.h"
#include "../Source/Vc2SystemStore.h"
#include "../Source/Vc2WavesInUse.h"
#include "../Source/XtHostInput.h"
#include <algorithm>
#include <array>
#include <deque>
#include <memory>
#include <string>
#include <vector>

class Vc2PlayDirect final : public Vc2BlockSink {
public:
    static constexpr int kBlock = 32, kWords = 8;
    static constexpr uint8_t kRealTimeFirst = 0xf8, kSystemFirst = 0xf0, kChannelBits = 0x0f;   // MIDI's status bytes
    static constexpr uint8_t kControlChange = 0xb0;
    // A host message at its frame
    struct Message {
        int frame;
        uint8_t status, data1, data2;
    };
    // v458: with `fxRom`, rom.4's effects of its own (Vc2Fx), started after the patch, ticked by hand (fxTick)
    explicit Vc2PlayDirect(const std::vector<uint8_t>* store, const std::string* fxRom = nullptr) {
        s_.start();
        if (const Vc2System* system = vc2SystemDefaults()) {
            system_ = *system;
            s_.giveSystem(system_);
        }
        if (const Vc2LoadedWaves* w = vc2Waves()) s_.giveWaves(*w);
        s_.sink = this;
        s_.runOnOs([this](Vc2PartEvent& e) { return next(e); });
        for (const uint32_t word : kVc2FrontStart) {   // the front's start, the parameters' messages, before any patch
            queue_.push_back({ kVc2ParametersMessage, 0, 0, 1 });
            words_.push_back(word);
        }
        while (!queue_.empty() && !s_.osTasks->os.stuck()) s_.osTasks->deliverNext();
        stream_.assign(2 * kBlock, 0);   // the input's latency at 44.1 kHz: a block of silence
        if (store != nullptr) store_ = *store;
        s_.patchGiven(store != nullptr ? &store_ : nullptr);
        if (fxRom == nullptr) return;
        fx_ = std::make_unique<Vc2Fx>(*fxRom);
        fx_->start(store_.empty() ? nullptr : &store_, system_);
    }
    // v458: the effects' tick, as the control thread's between two host blocks: the IDs (given each time: the driver
    // takes only what moved), the driver's tick, the control changes it keeps heard since the last
    void fxTick() {
        if (!fx_) return;
        fx_->tick(store_.empty() ? nullptr : &store_, system_, true, false, controls_);
        controls_.clear();
    }
    bool fxOk() const { return fx_ && fx_->ok(); }
    void fxHeard(bool on) { fxHeard_ = on; }   // v458: the Effects switch: off, the copy without the chip, as the player
    // A parameter moved, at a block's edge: written over the store, the patch given again
    void edit(const Vc2HostParam& h, int value) {
        if (store_.empty()) store_ = vc2DefaultStore();
        vc2PatchPut(store_, h, value);
        s_.patchGiven(&store_);
    }
    // v457: system parameters moved together, at a block's edge (ID, value): written over the system, the system given
    // again, then the front's messages: the system's (kind 4) if one was system B's, the assignments' (5) if one was the
    // system's, in that order, before the next block
    void systemEdit(const std::vector<std::pair<int, int>>& moves) {
        bool systemB = false, assigns = false;
        for (const auto& [id, value] : moves) {
            Vc2SystemArea area {};
            if (!vc2SystemAreaOf(id, area)) continue;
            vc2SystemPut(system_, area, id, value);
            systemB = systemB || area == Vc2SystemArea::kB;
            assigns = assigns || area == Vc2SystemArea::kSystem;
        }
        s_.giveSystem(system_);
        for (const auto& [on, kind] : { std::pair { systemB, kVc2SystemRead }, std::pair { assigns, kVc2AssignsRead } })
            if (on) {
                queue_.push_back({ kVc2ParametersMessage, 0, 0, 1 });
                words_.push_back(kind);
            }
    }
    // The host's `n` frames: its input in (l, r), its messages at their frames, the part's frames out into left, right
    void host(const float* l, const float* r, int n, const std::vector<Message>& messages, float* left, float* right) {
        for (int i = 0; i < n; ++i) {
            stream_.push_back(XtHostInput::wordOf(l[i]));
            stream_.push_back(XtHostInput::wordOf(r[i]));
        }
        for (int i = 0; i < n; ++i, ++at_) {
            for (const Message& m : messages)
                if (m.frame == at_ && m.status < kSystemFirst && m.status < kRealTimeFirst) {
                    queue_.push_back({ (uint8_t)(m.status & ~kChannelBits), m.data1, m.data2, 1 });
                    words_.push_back(0);
                    const bool control = (m.status & ~kChannelBits) == kControlChange;   // v458: the effects' too
                    const bool kept = m.data1 == kVc2FxReverbSend || m.data1 == kVc2FxChorusSend;
                    if (fx_ && control && kept) controls_.push_back({ m.data1, m.data2 });
                }
            while ((long)out_.size() / 2 <= at_ && !s_.osTasks->os.stuck()) produce();
            const bool made = (long)out_.size() / 2 > at_;
            left[i] = made ? out_[(size_t)(2 * at_)] : 0.0f;
            right[i] = made ? out_[(size_t)(2 * at_ + 1)] : 0.0f;
        }
    }
    long ahead = 0, blocks = 0;   // the blocks made ahead in waits, and in all
    const Vc2Synth& part() const { return s_; }   // v457: for the checks of what its readers wrote

private:
    void produce() {
        wanted_ = true;
        const size_t had = out_.size();
        while (wanted_ && s_.osTasks->deliverNext()) {}
        wanted_ = false;
        if (out_.size() == had) out_.resize(had + 2 * kBlock, 0.0f);   // no block heard: silence, as the read's
    }
    bool next(Vc2PartEvent& e) {
        e = Vc2PartEvent {};
        if (!queue_.empty()) {
            e.block = false;
            e.message = queue_.front();
            e.word = words_.front();
            queue_.pop_front();
            words_.pop_front();
            return true;
        }
        ahead += wanted_ ? 0 : 1;
        wanted_ = false;
        inputs_.emplace_back(kBlock * kWords, 0);   // the input at the block's own place, zeros where not yet given
        std::vector<int32_t>& f = inputs_.back();
        const long from = kBlock * blocks++, made = (long)stream_.size() / 2;
        for (long i = 0; i < kBlock && from + i < made; ++i) {
            const int32_t w = vc2InputWord(stream_[(size_t)(2 * (from + i))], stream_[(size_t)(2 * (from + i) + 1)]);
            f[(size_t)(kWords * i + Vc2PartPlayer::kMicWord)] = w;
            f[(size_t)(kWords * i + Vc2PartPlayer::kTrackerWord)] = w;
        }
        e.input = f.data();
        return true;
    }
    void mixed(const float* frames) override {
        if (fx_ && fxHeard_) {   // v458: through the effects
            std::array<float, 2 * kBlock> lr {};
            fx_->unit().process(frames, lr.data(), kBlock);
            out_.insert(out_.end(), lr.begin(), lr.end());
            return;
        }
        for (int i = 0; i < kBlock; ++i) {
            const float* w = frames + kWords * i;
            out_.push_back((w[0] + w[4]) * 0x1p-30f);
            out_.push_back((w[1] + w[5]) * 0x1p-30f);
        }
    }
    Vc2Synth s_;
    Vc2System system_;   // v457: rom.4's defaults, the system parameters moved written over them
    std::vector<uint8_t> store_;
    std::deque<Vc2MidiMessage> queue_;
    std::deque<uint32_t> words_;                // each message's word
    std::deque<std::vector<int32_t>> inputs_;   // each block's input, kept: a block's outlives the waits inside it
    std::vector<int32_t> stream_;               // the input as the part takes it, L and R a frame
    std::vector<float> out_;
    long at_ = 0;
    bool wanted_ = false;
    std::unique_ptr<Vc2Fx> fx_;              // v458
    std::vector<Vc2FxControl> controls_;     // v458: the control changes for the next tick
    bool fxHeard_ = true;
};
