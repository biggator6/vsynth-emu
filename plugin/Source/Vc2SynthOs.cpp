// The part's tasks on rom.1's kernel (Vc2SynthOs.h, v355).
#include "Vc2SynthOs.h"
#include <algorithm>

void Vc2SynthOs::start() {
    kernel = Vc2Kernel {};
    took_ = {};
    started_ = {};
    inPass_ = {};
    gaveWay_ = {};
    stuck_ = false;
    took(kernel.start({ kLevels.begin(), kLevels.end() }));
    runUntil(kIdle);
}

int Vc2SynthOs::taskOf(uint32_t object) const {
    const auto at = std::ranges::find(kObjects, object);
    return at == kObjects.end() ? -1 : (int)(at - kObjects.begin());
}

// What a switch's task took, or a wait's own when it didn't switch
void Vc2SynthOs::took(const Vc2Kernel::Switch& s) {
    if (s.to != Vc2Kernel::kNone) took_[(size_t)s.to] = s.answer;
}

// A task's first run, its part of its prologue, else a pass; then the wait the pass says (its loop's after a prologue),
// none where a sender gave way (it's ready, the task it woke running)
void Vc2SynthOs::run(int task) {
    const auto t = (size_t)task;
    inPass_[t] = true;
    uint16_t mask = kMasks[t];
    if (started_[t]) {
        mask = parts_.pass(task, took_[t]);
    } else {
        started_[t] = true;
        parts_.prologue(task);
    }
    inPass_[t] = false;
    if (std::exchange(gaveWay_[t], false) || stuck_) return;
    took(kernel.wait(mask));
}

// The tasks the kernel runs until `task` is running again: with the idle running, time on (a wait inside a pass). A
// task whose pass is below on this thread is resumed by going back to its frame: an interrupt's run (`task` the idle)
// gives way to it; a post's or a wait's, for a task not its own, can't, and stops
void Vc2SynthOs::runUntil(int task) {
    while (kernel.current != task && !stuck_) {
        const int t = kernel.current;
        if (inPass_[(size_t)t]) {
            stuck_ = task != kIdle;
            return;
        }
        if (t == kIdle) {
            parts_.advance();
        } else {
            run(t);
        }
    }
}

void Vc2SynthOs::post(uint32_t object, uint32_t flags) {
    const int t = taskOf(object);
    if (t < 0 || stuck_) return;
    const int poster = kernel.current;
    const Vc2Kernel::Switch s = kernel.set(t, (uint16_t)flags);
    took(s);
    if (!s.switched) return;
    if (kGivesWay[(size_t)poster]) gaveWay_[(size_t)poster] = true;
    else runUntil(poster);
}

void Vc2SynthOs::raise(uint32_t object, uint32_t flags) {
    const int t = taskOf(object);
    if (t >= 0) kernel.raise(t, (uint16_t)flags);
}

void Vc2SynthOs::tick() {
    kernel.tick([this](int handler) { parts_.cyclic(handler); });
    parts_.appTick();
}

void Vc2SynthOs::interruptEnd() {
    if (kernel.watchdog) took(kernel.interruptEnd());
    runUntil(kIdle);
}

void Vc2SynthOs::unlockOut() {
    if (stuck_) return;
    const int me = kernel.current;
    const Vc2Kernel::Switch s = kernel.unlockOut();
    took(s);
    if (s.switched) runUntil(me);
}

void Vc2SynthOs::waitInPass(uint16_t mask) {
    if (stuck_) return;
    const int me = kernel.current;
    took(kernel.wait(mask));
    runUntil(me);
}
