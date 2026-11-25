#pragma once
// The VC-2 mode's audio on the processor (v455; HARDWARE_FIRMWARE.md, "The VC-2 in the plugin"): which engine plays (the
// session's mode, PluginMode.h), the VC-2's part (Vc2PartPlayer), made on the control thread once rom.4 is in and handed
// to the audio thread, and the patch it plays.  That patch is a store: the .p02 loaded last, or a session's (PluginVc2Patch),
// or with neither, every ID its default; the VC-2's parameters written over it as they move (vc2PatchPut), on the control
// thread, and the store given to the part at the next block's edge.  The store written is kept as the session's, so a
// session holds what the part played.  The system's parameters don't reach the part yet: it plays rom.4's system defaults.
#include "PluginMode.h"
#include "PluginVc2Patch.h"
#include "Vc2PartPlayer.h"
#include "Vc2PatchStore.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class PluginVc2Play final : private juce::ValueTree::Listener {
public:
    PluginVc2Play(juce::AudioProcessorValueTreeState& apvts, PluginVc2Patch& patch);
    ~PluginVc2Play() override;
    PluginVc2Play(const PluginVc2Play&) = delete;
    PluginVc2Play& operator=(const PluginVc2Play&) = delete;
    // Message thread (prepareToPlay): the host's rate and its longest block
    void prepare(double hostRate, int maxBlock);
    // The control thread's (every 10 ms beside fxTick; a test's by hand): in the VC-2 mode with rom.4 and rom.2 in (the
    // part's ports read rom.2's tables as well: its OSCs, envelopes, LFOs, COSMs), the patch's
    // store written and posted when it moved, and the part made (again when it stopped, or once the waves came)
    void tick();
    // The audio thread's block: the part held while it lasts; false outside the VC-2 mode
    class Block {
    public:
        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
        ~Block();
        explicit operator bool() const { return on_; }
        // A channel message from the editor or the host, before the frames rendered after it
        void message(const juce::MidiMessage& m);
        // The block: the host's MIDI at its frames, the input in the buffer's first `inputs` channels, the part's sound
        // into the buffer (silence while there's no part)
        void process(juce::AudioBuffer<float>& buffer, const juce::MidiBuffer& midi, int inputs);

    private:
        friend class PluginVc2Play;
        Block(PluginVc2Play& owner, bool on);
        void span(int from, int to);
        PluginVc2Play& owner_;
        bool on_;
        Vc2PartPlayer* p_ = nullptr;
        const float *inL_ = nullptr, *inR_ = nullptr;
        float *L_ = nullptr, *R_ = nullptr;
    };
    Block block();
    // For the checks: whether the mode is the VC-2's, the part playing (none until a tick made one), how many made
    bool vc2() const { return vc2_.load(); }
    Vc2PartPlayer* player() const { return live_.load(); }
    int built() const { return built_; }
    // v456: the part as the editor sees it, any thread, every part made writing this one (Vc2PartView.h)
    const Vc2PartView& view() const { return view_; }

private:
    // A VC-2 parameter as the store takes it: its ID, its value, its default
    struct Param {
        const Vc2HostParam* h;
        std::atomic<float>* value;
        int def;
    };
    // The store as the control thread writes it: the part's patch, none for every ID's default; each parameter as last
    // written or taken; the patch's generation it started from; its number
    struct Keeper {
        std::vector<uint8_t> work;
        bool none = true;
        std::vector<int> last;
        long gen = -1;
        uint64_t seq = 0;
    };
    // A store on its way to the audio thread, three of them (the control thread's, the newest, the audio thread's)
    struct Posted {
        uint64_t seq = 0;
        bool none = true;
        std::array<uint8_t, kVc2StoreBytes> bytes {};
    };
    static constexpr int kFresh = 4, kSlot = 3;
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& property) override;
    void valueTreeRedirected(juce::ValueTree&) override { readMode(); }
    void readMode();
    void keepStore();
    void post();
    const Posted* taken();   // audio thread: the newest posted since, or none
    void made();             // the part made again
    juce::AudioProcessorValueTreeState& apvts_;
    PluginVc2Patch& patch_;
    std::vector<Param> params_;
    Keeper keeper_;
    std::mutex controlLock_;   // the control thread's and prepare's
    double rate_ = 44100.0;
    std::vector<float> scratch_ = std::vector<float>(512);   // the right side for a mono output
    std::array<Posted, 3> posted_ {};
    std::atomic<int> newest_ { 1 };
    int back_ = 0, front_ = 2;
    std::atomic<bool> vc2_ { false }, inUse_ { false };
    std::atomic<Vc2PartPlayer*> live_ { nullptr };
    std::unique_ptr<Vc2PartPlayer> owned_;
    int built_ = 0;
    Vc2PartView view_;
};
