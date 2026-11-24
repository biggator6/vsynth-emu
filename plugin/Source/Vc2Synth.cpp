// The VC-2's part as one object: see Vc2Synth.h (v314).  The start in rom.4's order: C startup's words, then the static
// initializers by the table's rows.
#include "Vc2Synth.h"
#include "Vc2SynthControls.h"
#include "Vc2SynthReads.h"
#include "Vc2SynthSettings.h"
#include "Vc2SynthVoices.h"
#include "Vc2ToneLfo.h"
#include "Vc2WaveBoot.h"
#include <bit>
#include <cstring>

namespace {
// [FW] .data words no port's defaults give
constexpr std::array<int16_t, 4> kVoiceCounts = { -1, -1, -1, 0 };   // 0x8993d02c: the counts as last read
constexpr uint32_t kTvaStep = 0x3bf7adf6;                            // 0x8993eb08: C's TVA step
constexpr int16_t kElapsed = 1;                                      // 0x8993d084: the update task's count
constexpr uint8_t kReceiveOn = 1;                                    // 0x8992e3e8, 0x8992e3e9: both Receive flags
// [FW] what the initializers no port runs write
constexpr Vc2PitchSource kNoSource = { -1, 0, 0.0f };   // row 12, 0x89394a90
constexpr uint32_t kNotesUnread = 0x3c199999;           // row 17, 0x89396970: 0.009375
constexpr float kRobotStart = 1.0f;                     // row 42, 0x893a7650

// What the static M's initializers reach: its own channel state's set-up, v313's on its bytes, and (the part's init's,
// v345) its node onto the update queue
class StaticParts final : public Vc2StaticMatrixParts {
public:
    explicit StaticParts(Vc2Synth& s) : s_(s) {}
    void channelStateInit(uint32_t, int32_t channel) override {
        Vc2ChannelState c;
        std::memcpy(&c, s_.staticM.state.data(), sizeof c);
        vc2ChannelSetUp(c, (uint8_t)channel);
        std::memcpy(s_.staticM.state.data(), &c, sizeof c);
    }
    void enqueue(uint32_t, uint32_t node) override { s_.queue.enqueue(node, s_.voices, *s_.voiceParts); }

private:
    Vc2Synth& s_;
};
} // namespace

// v340: the tones' hosts on the part's words: the pool, the lists, the mixes' rings, the free LFOs, the chaos generator,
// C's TVA step, the COSM pools, the part mixer's frames after buffer c, the shared routines
Vc2Synth::Vc2Synth() {
    queue.setNode(kVc2StaticNodeAt, staticM.node);
    const Vc2ToneCommon common { tones, lists, rings, freeLfos, chaos, tvas.step, pools, frames.data(), math };
    for (int tone = 0; tone < Vc2TonePool::kTones; ++tone) hosts.emplace_back(tone, common, *this);
    reads = std::make_unique<Vc2SynthReads>(*this);   // v343: on every member, so last
    noteParts = std::make_unique<Vc2SynthNotes>(*this);   // v345
    voiceParts = std::make_unique<Vc2SynthVoices>(*this);
    controls = std::make_unique<Vc2SynthControls>(*this);
    settings = std::make_unique<Vc2SynthSettings>(*this);
}

Vc2Synth::~Vc2Synth() = default;

// [FW] the tone the pool's take gives: its head, the first when the pool was never taken from (any host past its end)
uint32_t Vc2Synth::makeTone(uint32_t id) {
    const uint32_t taken = tones.head == 0 ? tones.addressOf(0) : tones.head;
    Vc2SynthTone& h = hosts[tones.holds(taken) ? (size_t)tones.indexOf(taken) : 0];
    return tones.make(id, h) != nullptr ? taken : 0;
}

// [FW] the tail's two routines, the notes' (0x893964d0, 0x893964e0) as a note's constructor gives them
void Vc2Synth::ended(int tone) {
    if (tones.tones[(size_t)tone].tail.ended == Vc2Notes::kEndedRoutine) notes.ended(tones.addressOf(tone), *noteParts);
}

void Vc2Synth::truncated(int tone) {
    if (tones.tones[(size_t)tone].tail.truncated == Vc2Notes::kTruncatedRoutine) {
        notes.truncated(tones.addressOf(tone), *noteParts);
    }
}

void Vc2Synth::taskStart() {
    notes.staticInit();
    notes.hook = kVc2SynthEndedHook;
    Vc2PartInitWorld init { rows, nodes, keys, numbers };
    StaticParts parts { *this };
    Vc2PartStartWorld w { init, startWords, voices, reloads, staticM, parts };
    vc2PartStartUp(w);
    noteGate = 0;
}

bool Vc2SynthReloads::readPart(int part) { return s_.settings->readPart(part); }

void Vc2Synth::giveWaves(const Vc2LoadedWaves& w) {
    waves = &w;
    store = &w.store;
    waveSets = w.sets;
}

void Vc2Synth::start() {
    for (uint32_t k = 0; k < partTable.size(); ++k) {   // [FW] .data: each part's trip record
        partTable[k] = Vc2TripRecords::kAt + (uint32_t)sizeof(Vc2TripRecord) * k;
    }
    rows.voiceCounts = kVoiceCounts;
    tvas.step = std::bit_cast<float>(kTvaStep);
    time.elapsed = kElapsed;
    midi.receiveProgram = midi.receiveBank = kReceiveOn;
    for (Vc2PartNode& n : nodes.nodes) n.channel = 0;   // .bss: the part's init gives them none

    midi.staticInitPorts();   // the whole table's 0x893179d0
    midi.staticInitStates();  // and 0x89317db0, before the part's rows
    voices.staticInit();      // row 4, 0x89391450 (row 6's age word the default 0)
    keys.constructGlides();   // row 7, 0x89392560
    trips.staticInit();       // row 11, 0x89394570
    pitch = kNoSource;        // row 12
    queue.staticInit();       // row 14, 0x89395280
    notes.unread = kNotesUnread;   // row 17
    reloads.staticInit();     // row 26, 0x8939b350
    StaticParts parts { *this };
    staticM.staticInit(parts);     // row 37, 0x893a3020
    rows.robot = kRobotStart;      // row 42
    pools.init();             // row 45, 0x893ab550
    chaos = Vc2ToneLfo::kChaosStart;   // row 47, 0x893ad8d0
    freeLfos.construct();     // row 54, 0x893b1fb0's (its lists' heads the defaults, the vocoder built with this)
    autoNote.init();          // row 83, 0x893ebe50
    ring.setGlobals();        // row 85, 0x893ecef0
}
