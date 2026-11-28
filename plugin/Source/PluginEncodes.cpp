// The processor's encodes on their pool (v168).  An encode runs the firmware in the interpreter for up to about a
// third of its wave's length (a 30 s BACKING wave: 10.9 s, measured 2026-11-09), and a lambda on the pool never heard
// removeAllJobs(true, ...) ask it to stop.  So the destructor waited its 10 s and went on, and the job then wrote into
// members already destroyed while its thread was killed under it.  Each encode is now a job that hears that signal
// and stops its firmware (Sh4StopScope): the call returns within a few ms, and the job runs to its end while the
// destructor still waits.  What it then does is what a failed encode does.  The work is given the stop too, for the
// part that isn't firmware: Auto's classifier ran 360 s of audio on for over 15 s after a close, past the destructor's
// 10 s, until the pool killed the thread (v185).  The destructor now waits with no deadline.
#include "ContentClassifier.h"
#include "PluginProcessor.h"
#include "Sh4Interp.h"
#include "XtPatch.h"
#include <optional>

namespace {

using Work = std::function<void(const std::atomic<bool>& stop)>;

class EncodeJob final : public juce::ThreadPoolJob, private juce::Thread::Listener {
public:
    explicit EncodeJob(Work work) : juce::ThreadPoolJob("encode"), work_(std::move(work)) {
        addListener(this);
    }
    ~EncodeJob() override { removeListener(this); }
    JobStatus runJob() override {
        const Sh4StopScope stop(stop_);
        work_(stop_);
        return jobHasFinished;
    }

private:
    void exitSignalSent() override { stop_.store(true); }   // the pool's signalJobShouldExit, on the thread that asks
    Work work_;
    std::atomic<bool> stop_ { false };
};

} // namespace

void VSynthEmuProcessor::addEncode(Work job) {
    encodePool_.addJob(new EncodeJob(std::move(job)), true);   // the pool deletes it when it's done
}

namespace {
constexpr int kTypeOfChoice[] = { 1, 3, 5, 0 };   // the Encode Type's choices 1..4 (SOLO, BACKING, ENSEMBLE, LITE); 0 Auto
} // namespace

// An OSC's wave encoded as Encode Type `choice` at `depth`, on the pool; a newer encode of the OSC discards this one's
// result.  Moved whole from PluginProcessor.cpp's reencode (v185), which keeps the parameter reads.  A job that has been
// asked to stop by the time its encode would start, Auto's classifier stopped part way or a close before it, ends there.
void VSynthEmuProcessor::encodeWave(int osc, int choice, int depth) {
    LoadedWave& w = wave_[osc];
    if (!w.loaded.load()) return;
    if (!xtSynthRomReady()) {
        xtActive_.store(false);
        return;
    }
    std::vector<std::vector<float>> chans;
    double rate;
    int bits, own;
    XtWaveFile ownWave;
    bool isOwn = false;
    PluginWaveEdit edit;   // v480: Truncate's cut, made in the job on the XT's import
    {
        juce::CriticalSection::ScopedLockType lock(bufferLock_);
        edit = w.edit;
        own = w.ownType == 0 || w.ownType == 1 || w.ownType == 3 || w.ownType == 5 ? w.ownType : -1;
        if (w.isOwn) {
            ownWave = w.own;
            isOwn = true;
        }
        for (int c = 0; c < std::min(2, w.buffer.getNumChannels()); ++c)
            chans.emplace_back(w.buffer.getReadPointer(c), w.buffer.getReadPointer(c) + w.buffer.getNumSamples());
        rate = w.rate;
        bits = w.bits;
    }
    if (!edit.cut) publishWaveView(osc, chans, rate);   // the editor's picture of it (PluginWaveView.cpp, v200)
    const int gen = ++w.gen;
    const juce::String who = osc ? "OSC 2: " : "";
    setStatus(who + "Encoding...");
    addEncode([=, this, chans = std::move(chans), ownWave = std::move(ownWave)](const std::atomic<bool>& stop) mutable {
        waveCutOnto(edit, chans, rate, bits);   // v480: the kept frames, at 44.1 kHz, and the wave's own LITE
        waveCutOnto(edit, ownWave, own, (long)chans[0].size());
        int type = choice >= 1 && choice <= 4 ? kTypeOfChoice[choice - 1] : own;   // Auto: what the wave says it is, or
        if (0 > type) {   // Auto: the plugin's content classifier (ContentClassifier.cpp)
            std::vector<float> mono(chans[0].size());
            for (size_t i = 0; i < mono.size(); ++i) {
                float s = 0;
                for (const auto& c : chans) s += c[i];
                mono[i] = s / (float)chans.size();
            }
            using CT = VSE::VariphraseAnalysis::ContentType;
            const auto a = VSE::analyzeContent(mono.data(), (int)mono.size(), rate, &stop);
            // the classifier's LITE as SOLO: Auto doesn't choose LITE
            type = a.contentType == CT::BACKING ? 3 : a.contentType == CT::ENSEMBLE ? 5 : 1;
        }
        if (stop) return;   // closing: no encode is started for a processor that won't keep it
        std::string err;
        // A V-Synth SOLO wave as its own type keeps its own events (as encoded, or edited in V-Producer), which its vpno
        // names; otherwise the detection at the Encode Depth finds them (xtSynthEncode)
        // v481: the Encode screen's adds and deletes, at this depth, on the detection's markers, so not the wave's own
        std::vector<int> results;
        std::optional<XtFwEventScope> screen;
        if (waveEventsStand(edit, depth)) screen.emplace(edit.events, results);
        const bool ownEvents = isOwn && type == own && !screen;
        auto e = xtSynthEncode(chans, rate, type, depth, &err, bits, ownEvents ? &ownWave.events : nullptr,
                               ownEvents ? &ownWave.vpdt : nullptr);
        screen.reset();
        if (gen != wave_[osc].gen.load()) return;
        if (!e) {
            setStatus(who + "Encode failed: " + juce::String(err));
            return;
        }
        if (isOwn) xtWaveOntoEncoded(ownWave, *e);
        {   // v479: the session's edits after the wave's own, and the wave as the synth now plays it, for the WAVE page
            const juce::ScopedLock lock(bufferLock_);
            waveEditOnto(wave_[osc].edit, *e);
            wave_[osc].encoded = e;
        }
        publishWaveView(osc, chans, rate, e->frames);
        synth_.setSample(e, osc);
        xtActive_.store(true);
        static const char* names[] = { "LITE", "SOLO", "", "BACKING", "", "ENSEMBLE" };
        const juce::String refused = PluginWaveEdits::eventsDone(*this, osc, edit, depth, results);   // the XT's word
        if (refused.isNotEmpty()) return setStatus(who + refused);
        // v492: "its own", short enough that a band type's fits the header's status line, as "auto" does
        setStatus(who + "Encoded as " + names[type] + (choice != 0 ? "" : own >= 0 ? " (its own)" : " (auto)"));
    });
}
