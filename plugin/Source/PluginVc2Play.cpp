// The VC-2 mode's audio on the processor: see PluginVc2Play.h (v455).  Checked by VSynthEmuVc2PlayTest.
#include "PluginVc2Play.h"
#include "Vc2WavesInUse.h"
#include "XtEncoded.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

namespace {
constexpr uint8_t kChannelFirst = 0x80, kSystemFirst = 0xf0, kClassBits = 0xf0;   // [FW] MIDI's status bytes
}

PluginVc2Play::PluginVc2Play(juce::AudioProcessorValueTreeState& apvts, PluginVc2Patch& patch)
    : apvts_(apvts), patch_(patch) {
    for (const Vc2HostParam& h : vc2HostParams()) {
        juce::RangedAudioParameter* r = apvts.getParameter(h.paramId);
        if (r == nullptr || h.id < kVc2FirstStored) continue;   // the system's, which no store holds
        const int def = (int)std::lround(r->convertFrom0to1(r->getDefaultValue()));
        params_.push_back({ &h, apvts.getRawParameterValue(h.paramId), def });
    }
    keeper_.last.assign(params_.size(), 0);
    readMode();
    apvts_.state.addListener(this);
}

PluginVc2Play::~PluginVc2Play() { apvts_.state.removeListener(this); }

void PluginVc2Play::valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) {
    if (property == kPluginModeProperty) readMode();
}

void PluginVc2Play::readMode() { vc2_.store(pluginMode(apvts_.state) == PluginMode::vc2); }

void PluginVc2Play::prepare(double hostRate, int maxBlock) {
    const std::lock_guard<std::mutex> hold(controlLock_);
    rate_ = hostRate;
    scratch_.assign((size_t)std::max(maxBlock, 1), 0.0f);
    if (owned_) owned_->prepare(hostRate);
}

void PluginVc2Play::tick() {
    const std::lock_guard<std::mutex> hold(controlLock_);
    if (!vc2_.load() || !xtSynthVc2RomReady() || !xtSynthRomReady()) return;   // the part's ports read rom.2's too
    keepStore();
    const Vc2LoadedWaves* waves = vc2Waves();
    if (owned_ && !owned_->stuck() && (owned_->waves() != nullptr || waves == nullptr)) return;
    made();
}

// The part made again, the store given it before the audio thread has it; the last one goes once no block holds it
void PluginVc2Play::made() {
    auto next = std::make_unique<Vc2PartPlayer>(vc2Waves(), rate_);
    next->patchGiven(keeper_.none ? nullptr : keeper_.work.data());
    next->patchSeq = keeper_.seq;
    live_.store(next.get());
    while (inUse_.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    owned_ = std::move(next);
    ++built_;
}

// The store kept: from the patch's own when it came (its reads standing, only what differs from them written), then
// each parameter that moved written over it; posted, and kept as the session's, when it moved
void PluginVc2Play::keepStore() {
    Keeper& k = keeper_;
    bool base = false;
    {
        const std::lock_guard<std::mutex> hold(patch_.lock);
        if (patch_.gen != k.gen) {
            k.none = patch_.store.size() != (size_t)kVc2StoreBytes;
            if (!k.none) k.work = patch_.store;
            k.gen = patch_.gen;
            base = true;
        }
    }
    const bool fromNone = k.none;
    bool moved = base;
    for (size_t i = 0; i < params_.size(); ++i) {
        const Param& p = params_[i];
        const int v = (int)std::lround(p.value->load());
        if (!base && v == k.last[i]) continue;
        k.last[i] = v;
        if (base && v == (fromNone ? p.def : vc2PatchValue(k.work, *p.h).value_or(v))) continue;
        if (k.none) {
            k.work = vc2DefaultStore();
            k.none = false;
        }
        vc2PatchPut(k.work, *p.h, v);
        moved = true;
    }
    if (!moved) return;
    ++k.seq;
    post();
    if (k.none) return;
    const std::lock_guard<std::mutex> hold(patch_.lock);
    if (patch_.gen == k.gen) patch_.store = k.work;
}

void PluginVc2Play::post() {
    Posted& p = posted_[(size_t)back_];
    p.seq = keeper_.seq;
    p.none = keeper_.none;
    if (!p.none) std::copy(keeper_.work.begin(), keeper_.work.end(), p.bytes.begin());
    back_ = newest_.exchange(back_ | kFresh, std::memory_order_acq_rel) & kSlot;
}

const PluginVc2Play::Posted* PluginVc2Play::taken() {
    if ((newest_.load(std::memory_order_acquire) & kFresh) == 0) return nullptr;
    front_ = newest_.exchange(front_, std::memory_order_acq_rel) & kSlot;
    return &posted_[(size_t)front_];
}

// ─── The audio thread's block ─────────────────────────────────────────────────

PluginVc2Play::Block PluginVc2Play::block() { return Block(*this, vc2_.load()); }

PluginVc2Play::Block::Block(PluginVc2Play& owner, bool on) : owner_(owner), on_(on) {
    if (!on_) return;
    owner_.inUse_.store(true);
    p_ = owner_.live_.load();
    if (p_ == nullptr) return;
    if (const Posted* s = owner_.taken(); s != nullptr && s->seq > p_->patchSeq) {
        p_->patchGiven(s->none ? nullptr : s->bytes.data());
        p_->patchSeq = s->seq;
    }
}

PluginVc2Play::Block::~Block() {
    if (on_) owner_.inUse_.store(false);
}

// A channel message, on channel 1, part 1's by rom.4's system defaults (the host's channels all reach it, as all reach
// the V-Synth's); the rest (system messages, the real-time bytes) not taken
void PluginVc2Play::Block::message(const juce::MidiMessage& m) {
    const juce::uint8* raw = m.getRawData();
    const int size = m.getRawDataSize();
    if (p_ == nullptr || size < 2 || raw[0] < kChannelFirst || raw[0] >= kSystemFirst) return;
    p_->message((uint8_t)(raw[0] & kClassBits), raw[1], size > 2 ? raw[2] : 0);
}

void PluginVc2Play::Block::process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int inputs) {
    const int n = buffer.getNumSamples(), channels = buffer.getNumChannels();
    if (p_ == nullptr || channels == 0) {
        buffer.clear();
        return;
    }
    inL_ = inputs > 0 ? buffer.getReadPointer(0) : nullptr;
    inR_ = inputs > 1 ? buffer.getReadPointer(1) : nullptr;
    L_ = buffer.getWritePointer(0);
    R_ = channels > 1 ? buffer.getWritePointer(1) : nullptr;
    // The block in pieces the input takes whole (the player's most(), the host's block as a rule): each piece's input in
    // before any of its frames are written over, then its spans between the MIDI's frames
    auto next = midi.cbegin();
    for (int piece = 0; piece < n;) {
        const int end = std::min(n, piece + p_->most());
        p_->input(inL_ != nullptr ? inL_ + piece : nullptr, inR_ != nullptr ? inR_ + piece : nullptr, end - piece);
        int pos = piece;
        for (; next != midi.cend() && (*next).samplePosition < end; ++next) {
            const auto meta = *next;
            const int at = std::clamp(meta.samplePosition, pos, end);
            span(pos, at);
            pos = at;
            message(meta.getMessage());
        }
        span(pos, end);
        piece = end;
    }
    for (; next != midi.cend(); ++next) message((*next).getMessage());   // past the block's end: before the next
    for (int ch = 2; ch < channels; ++ch) buffer.clear(ch, 0, n);
}

// Frames [from, to) of the part out; a mono output the two sides' mean, through the scratch side
void PluginVc2Play::Block::span(int from, int to) {
    std::vector<float>& scratch = owner_.scratch_;
    for (int at = from; at < to;) {
        const int k = R_ != nullptr ? to - at : std::min(to - at, (int)scratch.size());
        float* r = R_ != nullptr ? R_ + at : scratch.data();
        p_->render(L_ + at, r, k);
        if (R_ == nullptr)
            for (int i = 0; i < k; ++i) L_[at + i] = 0.5f * (L_[at + i] + r[i]);
        at += k;
    }
}
