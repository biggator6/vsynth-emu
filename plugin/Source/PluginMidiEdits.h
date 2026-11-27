#pragma once
// The parameters MIDI edits (v462; research/NEXT_ISSUES.md items 1 and 2, the user's d-5): the firmware's six edits of
// the patch from a control change (XtMidiPort) and, from v463, a learned controller's.  An edit is the synth's at once:
// the audio thread keeps it, and the parameters' reads for the synth (VSynthEmuProcessor::raw) take it ahead of the
// parameter until the message thread has handed it to the parameter, which tells the host and the controls, a gesture
// each (flush, on a timer here; a test calls it).  An edit whose parameter someone else moved meanwhile (a patch, the
// host, a control) is dropped there: the later setter wins.  Nothing on the audio thread locks or allocates.
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

class PluginMidiEdits : private juce::Timer {
public:
    explicit PluginMidiEdits(juce::AudioProcessorValueTreeState& state);
    ~PluginMidiEdits() override;
    int count() const { return (int)params_.size(); }
    int indexOf(const juce::String& id) const;                 // a parameter's index here, -1 for none
    juce::RangedAudioParameter* param(int index) const;        // and the parameter, nullptr for none
    // Audio thread: the parameter at `index` to `value` in its own units, within its range
    void edit(int index, float value);
    // Any thread: the parameter's value as the synth reads it, its pending edit's if it has one
    float read(const std::atomic<float>* raw) const;
    // Message thread: each pending edit to its parameter; how many were set
    int flush();
    // Message thread: every pending edit dropped, for a session or a patch that came
    void drop();

private:
    struct Pending {
        std::atomic<uint32_t> gen { 0 };       // the audio thread's: one more at each edit
        std::atomic<uint32_t> flushed { 0 };   // the message thread's: the gen it last took
        std::atomic<float> value { 0 }, before { 0 };   // the edit, and the parameter when a run of edits began
    };
    void timerCallback() override { flush(); }
    // Each pending one taken (the parameter set if `set`); whether none was left pending
    bool settle(bool set, int& count);
    std::vector<juce::RangedAudioParameter*> params_;
    std::vector<std::atomic<float>*> raws_;
    std::vector<std::pair<const std::atomic<float>*, int>> byRaw_;   // sorted by address, for read
    std::unique_ptr<Pending[]> pending_;
    std::vector<float> lastSet_;              // the message thread's: what flush left each parameter at
    std::atomic<uint32_t> edits_ { 0 };       // edits made
    std::atomic<uint32_t> settled_ { 0 };     // edits_ as a flush or drop that left none pending saw it
};
