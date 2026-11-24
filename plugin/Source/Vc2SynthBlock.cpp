// The part's block's parts: see Vc2SynthBlock.h (v351).  Every address is rom.4's.
#include "Vc2SynthBlock.h"
#include "Vc2Synth.h"

namespace {
constexpr int kBlockSamples = Vc2PartMixer::kBlock, kStereo = 2;
// [FW] Auto Note's: the key table's source (source 3's, 0-based), the keys a held one is sought in, its notes' step
// up, the events its note-on and note-off post the notes' task
constexpr uint32_t kKeysSource = 2;
constexpr int32_t kLowKey = 0, kHighKey = 127, kOctave = 12;
constexpr uint32_t kNoteOnEvent = 0x020, kNoteOffEvent = 0x040;
constexpr int kAutoNoteNode = Vc2PartNodes::kParts * 3;   // [FW] source 4's part 0 node (0x89988bd0)
} // namespace

void Vc2SynthBlock::inputStage(const float* second) {
    s_.autoNote.block(s_.tracker, second, kBlockSamples, s_.pitch, *this);
}

void Vc2SynthBlock::micRing(const float* mic) { s_.ring.write(mic, s_.tracker.period()); }

bool Vc2SynthBlock::engine() { return s_.vocoder.engine != nullptr; }

void Vc2SynthBlock::unvoiced(const float* mic) { s_.vocoder.stage.block(mic); }

float* Vc2SynthBlock::detectorRing() { return s_.vocoder.stage.micBlock(); }

void Vc2SynthBlock::vocoder(const float* ring, Vc2MixerFrame* frames) {
    s_.vocoder.block(ring, &frames->left, kStereo, s_.mixer.offsets.level);
}

void Vc2SynthBlock::resetTracker() { s_.tracker.reset(); }

void Vc2SynthBlock::resetUnvoiced() { s_.vocoder.stage.reset(); }

void Vc2SynthBlock::resetRing() { s_.ring.reset(); }

// [FW] 0x89391dc0: the level unread
bool Vc2SynthBlock::noteOn(int32_t note, float) {
    const auto channel = (uint32_t)(int32_t)s_.nodes.nodes[kAutoNoteNode].channel;
    const Vc2KeyTable& keys = s_.keys.table(Vc2Keys::tableAt(kKeysSource, channel));
    if (vc2KeysHighestIn(keys.held, kLowKey, kHighKey) >= 0) return false;
    s_.autoNoteKeys[0] = (uint32_t)(note + kOctave);
    s_.post(kVc2SynthNotesTask, kNoteOnEvent);
    return true;
}

// [FW] 0x89391e20
void Vc2SynthBlock::noteOff(int32_t note) {
    s_.autoNoteKeys[1] = (uint32_t)(note + kOctave);
    s_.post(kVc2SynthNotesTask, kNoteOffEvent);
}
