// The part's memory by rom.4's address: see Vc2SynthMemory.h (v342; the table xt_vc2synth's start compares, v314,
// moved from Vc2SynthCheck.cpp as it stood, with v340's buffers and frames).
#include "Vc2SynthMemory.h"
#include <algorithm>
#include <cstring>

// [FW] every member's place: the whole objects and the tables a field at a time
std::vector<Vc2SynthRegion> vc2SynthRegions(Vc2Synth& s) {
    Vc2PartRows& r = s.rows;
    std::vector<Vc2SynthRegion> out = {
        vc2SynthRegion(Vc2OsMidi::kPortsAt, s.midi.ports, "the OS MIDI's port objects"),
        vc2SynthRegion(Vc2OsMidi::kStatesAt, s.midi.states, "the channel states"),
        vc2SynthRegion(Vc2OsMidi::kFlagsAt, s.midi.receiveProgram, "Receive Program Change"),
        vc2SynthRegion(Vc2OsMidi::kFlagsAt + 1, s.midi.receiveBank, "Receive Bank Select"),
        vc2SynthRegion(0x89981b6c, s.voices.voices, "the voices"),
        vc2SynthRegion(Vc2PartVoices::kHeadsAt, s.voices.heads, "the heads"),
        vc2SynthRegion(0x899826d4, s.voices.g, "the voices' globals"),
        vc2SynthRegion(0x8998270c, s.voices.ages, "the next age"),
        vc2SynthRegion(Vc2Keys::kTablesAt, s.keys, "the keys"),
        vc2SynthRegion(Vc2Notes::kHookAt, s.notes.hook, "the notes' hook"),
        vc2SynthRegion(0x8998a518, s.notes.unread, "its float"),
        vc2SynthRegion(Vc2Notes::kNotesAt, s.notes.notes, "the notes"),
        vc2SynthRegion(Vc2UpdateQueue::kAt, s.queue.q, "the update queue"),
        vc2SynthRegion(Vc2Sets::kAt, s.sets.items, "C"),
        vc2SynthRegion(Vc2NoteMatrices::kAt, s.sets.matrices.items, "the notes' M"),
        vc2SynthRegion(Vc2SetCommons::kAt, s.commons.items, "C's commons"),
        vc2SynthRegion(Vc2SetEnvelopes::kAt, s.envelopes.items, "C's envelopes"),
        vc2SynthRegion(Vc2SetLfos::kAt, s.lfos.items, "C's LFOs"),
        vc2SynthRegion(Vc2SetMixes::kAt, s.mixes.items, "C's mixes"),
        vc2SynthRegion(Vc2SetTvas::kAt, s.tvas.items, "C's TVAs"),
        vc2SynthRegion(Vc2SetTvas::kStepAt, s.tvas.step, "the TVA's step"),
        vc2SynthRegion(Vc2SetOscBlocks::kAt, s.oscBlocks.items, "C's OSC blocks"),               // v343
        vc2SynthRegion(Vc2SetOscBlocks::kSeedAt, s.oscBlocks.seed, "Random Pitch's seed"),       // v343
        vc2SynthRegion(Vc2SetCosmInputs::kAt, s.cosmInputs.items, "C's COSM inputs"),            // v343
        vc2SynthRegion(0x8993d040, s.partTable, "the part table"),                               // v343
        vc2SynthRegion(0x8995cb40, s.clockObject, "the clock object"),                           // v343
        vc2SynthRegion(Vc2WaveSets::kCountsAt, s.waveSets.counts, "the wave sets' counts"),      // v343
        vc2SynthRegion(Vc2WaveSets::kTablesAt, s.waveSets.tables, "their tables"),               // v343
        vc2SynthRegion(Vc2WaveSets::kBuffersAt, s.waveSets.buffers, "their buffers"),            // v343
        vc2SynthRegion(kVc2SynthLegatoAt, s.legato, "the legato's struct"),                     // v345
        vc2SynthRegion(0x89997090, r.commons, "the commons' table"),
        vc2SynthRegion(0x89e2765c, r.envelopes, "the envelopes' table"),
        vc2SynthRegion(0x89e2f85c, r.lfos, "the LFOs' table"), vc2SynthRegion(0x89e32a60, r.mixes, "the mixes' table"),
        vc2SynthRegion(0x89e4987c, r.tvas, "the TVAs' table"),
        vc2SynthRegion(0x89997fcc, r.controls, "the controls' flag"),
        vc2SynthRegion(0x89998a14, r.bytes, "the part's bytes"),
        vc2SynthRegion(0x89e2003c, r.cosms, "the COSM inputs' table"),
        vc2SynthRegion(0x89e371e0, r.oscs, "the OSC table"), vc2SynthRegion(0x89989c84, r.steps, "the step counts"),
        vc2SynthRegion(0x89997fdc, r.robot, "robot human depth"), vc2SynthRegion(0x89995390, r.modes, "the mode rows"),
        vc2SynthRegion(0x899891d0, r.heldNote, "the held note"),
        vc2SynthRegion(0x89989404, r.tripAssigns, "Time Trip's assigns"),
        vc2SynthRegion(0x899826fd, r.patchRemain, "Patch Remain"),
        vc2SynthRegion(0x8993d02c, r.voiceCounts, "the voices' counts"),
        vc2SynthRegion(0x8993d03c, r.chordMemory, "ChordMemory Switch"),
        vc2SynthRegion(0x899893e0, r.holdPresses, "the Hold bytes"),
        vc2SynthRegion(0x89997398, s.g, "G"), vc2SynthRegion(Vc2StaticMatrix::kAt, s.staticM, "the static M"),
        vc2SynthRegion(Vc2PartNodes::kNodesAt, s.nodes.nodes, "the nodes"),
        vc2SynthRegion(Vc2PartNodes::kListsAt, s.nodes.lists, "the nodes' lists"),
        vc2SynthRegion(Vc2PartNodes::kClassesAt, s.nodes.classes, "the class bytes"),
        vc2SynthRegion(Vc2PartNodes::kObjectsAt, s.nodes.objects, "the part objects"),
        vc2SynthRegion(Vc2PartNumbers::kObjectsAt, s.numbers.objects, "the number objects"),
        vc2SynthRegion(Vc2PartNumbers::kTableAt, s.numbers.table, "the numbers' table"),
        vc2SynthRegion(0x8993d0c0, s.held, "the held flags"),
        vc2SynthRegion(Vc2PartReloads::kAt, s.reloads.objects, "the reload objects"),
        vc2SynthRegion(Vc2PartReloads::kFloatsAt, s.reloads.floats, "their floats"),
        vc2SynthRegion(Vc2TripRecords::kAt, s.trips.records, "Time Trip's records"),
        vc2SynthRegion(Vc2TripRecords::kScratchAt, s.trips.scratch, "its scratch"),
        vc2SynthRegion(Vc2PartStartWords::kRoutingAt, s.startWords.routing, "the routing object's bytes"),
        vc2SynthRegion(Vc2PartStartWords::kFloatsAt, s.startWords.floats, "its floats"),
        vc2SynthRegion(Vc2PartStartWords::kAfterAt, s.startWords.after, "the bytes after the assigns"),
        vc2SynthRegion(Vc2PartStartWords::kRemainAt, s.startWords.remain, "the settings object's word"),
        vc2SynthRegion(0x89989748, s.pitch, "the pitch source"),
        vc2SynthRegion(0x89981904, s.noteGate, "the note-on gate"),
        vc2SynthRegion(0x89981b60, s.scheduled, "the schedule's count"),
        vc2SynthRegion(0x8993d034, s.autoNoteKeys, "Auto Note's keys"),
        vc2SynthRegion(0x8993d080, s.ticks, "the ticks"),
        vc2SynthRegion(0x8993d084, s.time, "the time struct"),
        vc2SynthRegion(0x8993fb48, s.tones.head, "the tone pool's head"),
        vc2SynthRegion(0x8993fb4c, s.tones.serial, "its serial"),
        vc2SynthRegion(Vc2ToneLists::kNewestAt, s.lists.newest, "the newest mark"),
        vc2SynthRegion(Vc2ToneLists::kNewestAt + 4, s.lists.joined, "the joined mark"),
        vc2SynthRegion(Vc2ToneLists::kSoundingAt, s.lists.sounding, "the sounding list"),
        vc2SynthRegion(Vc2ToneLists::kNewAt, s.lists.fresh, "the new list"),
        vc2SynthRegion(Vc2FreeLfos::kAt, s.freeLfos.w, "the free LFOs"),
        vc2SynthRegion(0x89999290, s.pools.w, "the COSM pools"),
        vc2SynthRegion(0x8993eac4, s.mixer.w, "the mixer's words"),
        vc2SynthRegion(0x8999f1d0, s.mixer.b, "its buffers"),
        vc2SynthRegion(0x89934550, s.mixer.bypass, "its bypass"),
        // v343: its offsets (0x89997290) are the static M's levels (+0x188), the static M's region holds them
        vc2SynthRegion(kVc2SynthVocoderAt, s.vocoder.w, "the vocoder's words"),
        vc2SynthRegion(kVc2SynthEditAt, s.vocoder.edit, "its edit words"),
        vc2SynthRegion(0x899d9b70, s.tracker.s, "the tracker"),
        vc2SynthRegion(0x8993fe3c, s.autoNote.w, "Auto Note's words"),
        vc2SynthRegion(0x899d994c, s.autoNote.g, "its globals"),
        vc2SynthRegion(0x8a0f31cc, s.ring.w, "the mic ring"), vc2SynthRegion(0x899d9968, s.ring.g, "its globals"),
        vc2SynthRegion(0x8993fe54, s.ring.mode, "its mode"), vc2SynthRegion(0x89999668, s.chaos, "the chaos generator"),
        vc2SynthRegion(0x89998b20, s.buffers, "the tones' buffers"),       // v340
        vc2SynthRegion(0x89998e20, s.frames, "the part mixer's frames"),   // v340
    };
    Vc2ToneMixRings& rings = s.rings;
    out.push_back({ Vc2ToneMixRings::kAAt, rings.a(Vc2ToneMixRings::kAAt).data(),
                    Vc2ToneMixRings::kTones * Vc2ToneMixRings::kBytes, "the mix's rings a" });
    out.push_back({ Vc2ToneMixRings::kBAt, rings.b(Vc2ToneMixRings::kBAt).data(),
                    Vc2ToneMixRings::kTones * Vc2ToneMixRings::kBytes, "the mix's rings b" });
    for (int k = 0; k < Vc2TonePool::kTones; ++k) {
        const uint32_t at = s.tones.addressOf(k);
        out.push_back(vc2SynthRegion(at, s.tones.tones[(size_t)k].head, "a tone's head"));
        out.push_back(vc2SynthRegion(at + kVc2ToneTailAt, s.tones.tones[(size_t)k].tail, "a tone's tail"));
    }
    return out;
}

Vc2SynthMemory::Vc2SynthMemory(Vc2Synth& s) : regions_(vc2SynthRegions(s)) {
    std::ranges::sort(regions_, {}, &Vc2SynthRegion::at);
}

// The region whose bytes hold `at`, none when no region does
const Vc2SynthRegion* Vc2SynthMemory::holding(uint32_t at) const {
    const auto after = std::ranges::upper_bound(regions_, at, {}, &Vc2SynthRegion::at);
    if (after == regions_.begin()) return nullptr;
    const Vc2SynthRegion& r = *(after - 1);
    return at - r.at < r.bytes ? &r : nullptr;
}

uint8_t* Vc2SynthMemory::byteAt(uint32_t at) {
    const Vc2SynthRegion* r = holding(at);
    if (r == nullptr) {
        ++unheld_;
        return nullptr;
    }
    return static_cast<uint8_t*>(r->mine) + (at - r->at);
}

void Vc2SynthMemory::read(uint32_t at, void* to, size_t bytes) {
    auto* out = static_cast<uint8_t*>(to);
    for (size_t k = 0; k < bytes; ++k) {
        const uint8_t* b = byteAt(at + (uint32_t)k);
        out[k] = b != nullptr ? *b : 0;
    }
}

void Vc2SynthMemory::write(uint32_t at, const void* from, size_t bytes) {
    const auto* in = static_cast<const uint8_t*>(from);
    for (size_t k = 0; k < bytes; ++k) {
        if (uint8_t* b = byteAt(at + (uint32_t)k)) *b = in[k];
    }
}
