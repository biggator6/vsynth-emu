// The part's update task's parts: see Vc2SynthUpdate.h (v349).  Every address is rom.4's.
#include "Vc2SynthUpdate.h"
#include "Vc2Synth.h"
#include "Vc2SynthReads.h"
#include "Vc2SynthSettings.h"
#include "Vc2SynthTones.h"
#include "Vc2SynthVoices.h"

namespace {
constexpr uint32_t kStays = 1;               // [FW] M's init's answer: the static M's node stays on the queue
constexpr uint32_t kPitchEvent = 0x100;      // [FW] 0x89391e40's post to the notes' task

// The refresh M's init's levels read with: the Matrix core's on the static M, its state as M's links then stand
class StaticRefresh final : public Vc2MatrixRefresh {
public:
    explicit StaticRefresh(Vc2SynthReads& reads) : reads_(reads) {}
    void refresh(Vc2MatrixWords&) override { reads_.refreshMatrix(Vc2StaticMatrix::kAt); }

private:
    Vc2SynthReads& reads_;
};
} // namespace

// [FW] the static M's node M's init on the static M (kStays); a voice's its class's +0x0c, the plain one's a bare 0
uint32_t Vc2SynthUpdate::nodeUpdate(uint32_t node, uint32_t) {
    if (node == kVc2StaticNodeAt) {
        StaticRefresh refresh { *s_.reads };
        s_.settings->setup.init(s_.staticM.m, s_.staticM.links, s_.g, refresh);
        return kStays;
    }
    if (s_.voices.at(node).vtable == Vc2Assigner::kNoteVtable) s_.classes.update(node, s_.voices, *s_.voiceParts);
    return 0;
}

void Vc2SynthUpdate::pitchSource() {
    s_.pitch.depth = s_.rows.robot;
    if (s_.autoNote.w.onSwitch != 0 || s_.rows.robot > 0.0f) s_.post(kVc2SynthNotesTask, kPitchEvent);
}

void Vc2SynthUpdate::voicesUpdate() { s_.mixer.updateTones(s_.toneParts->tasksTones(), *this); }

void Vc2SynthUpdate::schedule() { s_.mixer.scheduleTones(s_.toneParts->tasksTones(), *this); }   // v350

void Vc2SynthUpdate::freeLfosClock(float seconds) { s_.freeLfos.clock(seconds); }

void Vc2SynthUpdate::glue(float carried) { s_.vocoder.glue(carried, s_.tracker.period()); }
