// The processor's encodes on their pool (v168).  An encode runs the firmware in the interpreter for up to about a
// third of its wave's length (a 30 s BACKING wave: 10.9 s, measured 2026-11-09), and a lambda on the pool never heard
// removeAllJobs(true, ...) ask it to stop.  So the destructor waited its 10 s and went on, and the job then wrote into
// members already destroyed while its thread was killed under it.  Each encode is now a job that hears that signal
// and stops its firmware (Sh4StopScope): the call returns within a few ms, and the job runs to its end while the
// destructor still waits.  What it then does is what a failed encode does.
#include "PluginProcessor.h"
#include "Sh4Interp.h"

namespace {

class EncodeJob final : public juce::ThreadPoolJob, private juce::Thread::Listener {
public:
    explicit EncodeJob(std::function<void()> work) : juce::ThreadPoolJob("encode"), work_(std::move(work)) {
        addListener(this);
    }
    ~EncodeJob() override { removeListener(this); }
    JobStatus runJob() override {
        const Sh4StopScope stop(stop_);
        work_();
        return jobHasFinished;
    }

private:
    void exitSignalSent() override { stop_.store(true); }   // the pool's signalJobShouldExit, on the thread that asks
    std::function<void()> work_;
    std::atomic<bool> stop_ { false };
};

} // namespace

void VSynthEmuProcessor::addEncode(std::function<void()> job) {
    encodePool_.addJob(new EncodeJob(std::move(job)), true);   // the pool deletes it when it's done
}
