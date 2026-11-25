// The VC-2's part played in the plugin: see Vc2PartPlayer.h (v455).  Checked by VSynthEmuVc2PlayTest against the part
// driven directly.
#include "Vc2PartPlayer.h"
#include "Vc2PatchStore.h"
#include "Vc2SynthOsTasks.h"
#include "Vc2SynthVoices.h"
#include "Vc2SystemStore.h"
#include "Vc2WaveBoot.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace {
constexpr uint8_t kMidiIn = 1;   // [FW] a message's source: MIDI IN
constexpr size_t kRoomSlack = 4 * Vc2PartPlayer::kBlock;   // past a second, the blocks in flight as the room fills
} // namespace

Vc2PartPlayer::Vc2PartPlayer(const Vc2LoadedWaves* waves, double hostRate, const Vc2System* system)
    : waves_(waves), part_(std::make_unique<Vc2Synth>()) {
    store_.assign((size_t)kVc2StoreBytes, 0);
    prepare(hostRate);   // first: the start's blocks, if a wait in it takes any, count from the input's start
    Vc2Synth& s = *part_;
    s.start();
    if (const Vc2System* given = system != nullptr ? system : vc2SystemDefaults()) s.giveSystem(*given);
    if (waves_ != nullptr) s.giveWaves(*waves_);
    s.sink = this;
    s.runOnOs([this](Vc2PartEvent& e) { return next(e); });
    for (const uint32_t word : kVc2FrontStart) queue({ kVc2ParametersMessage, 0, 0, kMidiIn }, word);
    while (head_ != tail_ && !stuck()) s.osTasks->deliverNext();   // the front's start, before any patch
}

Vc2PartPlayer::~Vc2PartPlayer() = default;

void Vc2PartPlayer::prepare(double hostRate) {
    out_.prepare(hostRate, kMostAhead + kRoomSlack);
    in_.prepare(hostRate);
    inputBlocks_ = 0;
}

void Vc2PartPlayer::patchGiven(const uint8_t* store) {
    if (store == nullptr) {
        part_->patchGiven(nullptr);
        return;
    }
    std::memcpy(store_.data(), store, store_.size());
    part_->patchGiven(&store_);
}

void Vc2PartPlayer::systemGiven(const Vc2System& system, bool systemB, bool assigns) {   // v457
    part_->giveSystem(system);
    if (systemB) queue({ kVc2ParametersMessage, 0, 0, kMidiIn }, kVc2SystemRead);
    if (assigns) queue({ kVc2ParametersMessage, 0, 0, kMidiIn }, kVc2AssignsRead);
}

bool Vc2PartPlayer::message(uint8_t status, uint8_t data1, uint8_t data2) {
    return queue({ status, data1, data2, kMidiIn }, 0);
}

// A message in the queue, its word beside it (a parameters' message's kind); with the queue full, dropped and counted
bool Vc2PartPlayer::queue(const Vc2MidiMessage& m, uint32_t word) {
    if (tail_ - head_ >= kMessages) {
        ++counts_.dropped;
        return false;
    }
    words_[tail_ % kMessages] = word;
    queue_[tail_++ % kMessages] = m;
    return true;
}

bool Vc2PartPlayer::stuck() const { return part_->osTasks->os.stuck(); }

void Vc2PartPlayer::input(const float* inL, const float* inR, int n) { in_.push(inL, inR != nullptr ? inR : inL, n); }

void Vc2PartPlayer::render(float* left, float* right, int n) {
    out_.render(left, right, n, [this] { produce(); });
}

// The block the read needs: the messages waiting go in first, then it; a wait inside a pass may take more
void Vc2PartPlayer::produce() {
    if (stuck()) return;
    wanted_ = true;
    Vc2SynthOsTasks& os = *part_->osTasks;
    while (wanted_ && os.deliverNext()) {}
    wanted_ = false;
}

// The part's next event: a message waiting, else the block asked for, else (in a wait) a block ahead, while there's room
bool Vc2PartPlayer::next(Vc2PartEvent& e) {
    e = Vc2PartEvent {};
    if (head_ != tail_) {
        e.block = false;
        e.word = words_[head_ % kMessages];
        e.message = queue_[head_++ % kMessages];
        ++counts_.messages;
        return true;
    }
    if (!wanted_ && out_.ahead() + kBlock > (double)kMostAhead) return false;
    counts_.ahead += wanted_ ? 0 : 1;
    wanted_ = false;
    e.input = inputOf(inputBlocks_++);
    ++counts_.blocks;
    return true;
}

// The input of the input's `n`th block since its start: its 32 frames at their own place, what's held before them passed
// over (the frames of blocks that ran ahead on none), zeros past what the host has given
const int32_t* Vc2PartPlayer::inputOf(long n) {
    const int32_t behind = (int32_t)((uint32_t)(kBlock * n) - in_.taken());
    if (behind > 0) counts_.inputDropped += in_.drop(behind);
    const int32_t* lr = in_.block();
    std::array<int32_t, kBlock * kWords>& frames = inputs_[std::min(inputDepth_, kInputs - 1)];
    if (inputDepth_ < kInputs) ++inputDepth_;
    frames.fill(0);
    for (int i = 0; i < kBlock; ++i) {
        const int32_t w = vc2InputWord(lr[2 * i], lr[2 * i + 1]);
        frames[(size_t)(kWords * i + kMicWord)] = w;
        frames[(size_t)(kWords * i + kTrackerWord)] = w;
    }
    return frames.data();
}

// A block's frames, as the part mixed them: MAIN and MULTI out, at the unit's level
void Vc2PartPlayer::mixed(const float* frames) {
    if (inputDepth_ > 0) --inputDepth_;   // its input read: its buffer free for the next block given
    float* at = out_.append(kBlock);
    if (at == nullptr) {
        ++counts_.lost;
        return;
    }
    for (int i = 0; i < kBlock; ++i) {
        const float* f = frames + kWords * i;
        at[2 * i] = (f[kMainL] + f[kMultiL]) * kLevel;
        at[2 * i + 1] = (f[kMainR] + f[kMultiR]) * kLevel;
    }
    viewed(at);
}

// v456: what the editor sees of the part after a block (Vc2PartView.h), each word relaxed
void Vc2PartPlayer::viewed(const float* lr) {
    if (view == nullptr) return;
    Vc2Synth& s = *part_;
    constexpr auto relaxed = std::memory_order_relaxed;
    float out = 0.0f;
    for (int i = 0; i < 2 * kBlock; ++i) out = std::max(out, std::fabs(lr[i]));
    const float in = s.vocoder.stage.peak();
    view->inputPeak.store(in, relaxed);
    view->voiced.store(s.vocoder.stage.voiced(), relaxed);
    view->period.store(s.tracker.period(), relaxed);
    view->lock.store(s.tracker.s.lock, relaxed);
    view->autoNote.store(s.autoNote.w.note, relaxed);
    view->sounding.store(s.voices.sounding(*s.voiceParts).count, relaxed);
    view->outputPeak.store(out, relaxed);
    const int64_t quiet = view->quietFrames.load(relaxed);
    view->quietFrames.store(in >= Vc2PartView::kQuiet ? 0 : quiet + kBlock, relaxed);
    view->playing.store(true, relaxed);
}
