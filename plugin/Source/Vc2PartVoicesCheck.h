#pragma once
// xt_vc2voices' shared parts (v272): the calls each side logs (the voices' two destroys, the tone's end test and its
// cost), what those answer and a destroy writes into its voice (drawn once for both sides, taken in turn), where the
// notes and tones the voices read are in rom.4's memory (both sides read them there: rom.4's routines directly, the port
// through its fake parts), and the two sides.  XT_VC2VOICES_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2PartVoices.h"
#include <functional>
#include <span>
#include <vector>

inline constexpr uint32_t kVoicesOutAt = 0x8C020000;   // a getter's two words, in Vc2Fw's RAM
inline constexpr uint32_t kVoicesNotesAt = 0x8998a51c, kVoicesNoteBytes = 0x744;    // [FW] the notes W, by number
inline constexpr uint32_t kVoicesTonesAt = 0x8999f524, kVoicesToneBytes = 0x261c;   // [FW] the tones' pool
inline constexpr uint32_t kVoicesNoteTone = 0x738, kVoicesNoteKind = 0x742;          // [FW] W's tone and kind
inline constexpr uint32_t kVoicesToneVtable = 0x18, kVoicesToneClass = 0x898cd568;  // [FW] a tone's vtable, its word
inline constexpr uint32_t kVoicesNoteClass = 0x898cd3dc;                            // [FW] a note's voice's vtable
inline constexpr uint32_t kVoicesDestroyEntry = 0x24, kVoicesEndedEntry = 0x60;     // [FW] the two vtables' slots
inline constexpr uint32_t kVoicesCostAt = 0x893b4020;                               // [FW] the tone's cost
inline constexpr int kVoicesNotes = 24, kVoicesTones = 24;

// The calls logged; from v277 the assigner's besides (its part, Vc2AssignCheck.cpp), from v278 its release path's and
// note start's (Vc2AssignReleaseCheck.cpp)
enum VoicesKind : int32_t {
    kVoicesDestroy, kVoicesEnded, kVoicesCost, kVoicesPrepare, kVoicesRetune, kVoicesNoteOn, kVoicesStartNote,
    kVoicesTruncateOnce, kVoicesWait, kVoicesKeyOff, kVoicesParameters, kVoicesClock, kVoicesKinds
};

// A call as either side made it: what, the routine it reached and the object it was given, and from v277 the arguments
// after it (the registers past r4, then the stack's words)
struct VoicesCall {
    int32_t kind;
    uint32_t routine, object;
    std::array<uint32_t, 7> more {};
    bool operator==(const VoicesCall&) const = default;
};

// What a hooked call answers, and what a destroy leaves in its voice: its note, state, +0x30, +0x44 and where its age
// is (0 the zero's, 1 its +0x20, 2 its +0x24)
struct VoicesAnswer {
    uint32_t ended = 0;
    float cost = 0.0f;
    uint32_t note = 0, leave = 0;
    uint8_t state = 0;
    uint16_t updated = 0;
    int age = 0;
};

// The answers, drawn in turn by one generator as far as either side has taken them, so each side's n-th call gets the
// same; each side's next
struct VoicesDraws {
    std::mt19937 rng;
    std::vector<VoicesAnswer> drawn {};
    size_t rom = 0, port = 0;
    const VoicesAnswer& next(size_t& at);
    void restart(uint32_t seed);
};

// The call to a vtable's slot as rom.4 makes it: the routine the slot reaches and the object adjusted by its word
VoicesCall voicesSlotCall(Vc2Fw& fw, int32_t kind, uint32_t object, uint32_t vtable, uint32_t slot);

// What a destroy leaves in a voice at `self`, by the answer
void voicesApplyDestroy(Vc2VoiceNode& v, uint32_t self, const VoicesAnswer& a);

// The port's parts: the notes' and tones' words read from rom.4's memory where the draws wrote them, the calls logged,
// a destroy's writes made in the port's voice
class VoicesPortParts final : public Vc2VoiceParts {
public:
    VoicesPortParts(Vc2Fw& fw, Vc2PartVoices& port, VoicesDraws& draws, std::vector<VoicesCall>& log)
        : fw_(fw), port_(port), draws_(draws), log_(log) {}
    void destroy(uint32_t voice) override;
    uint32_t toneOf(uint32_t note) override { return fw_.cpu.rd32(note + kVoicesNoteTone); }
    int8_t kindOf(uint32_t note) override;
    bool ended(uint32_t tone) override;
    float cost(uint32_t tone) override;

private:
    Vc2Fw& fw_;
    Vc2PartVoices& port_;
    VoicesDraws& draws_;
    std::vector<VoicesCall>& log_;
};

struct VoicesCounts {
    long events = 0, inits = 0, takes = 0, fromFree = 0, stolen = 0, none = 0, moves = 0, reserves = 0;
    long scans = 0, ended = 0, stopAlls = 0, stops = 0, costs = 0, getters = 0, splices = 0, redraws = 0;
    long destroys = 0;
};

// The two sides: rom.4's interpreter and its log, the port's voices, parts, draws and log
struct VoicesSides {
    Vc2Fw& fw;
    Vc2PartVoices& port;
    VoicesPortParts& parts;
    VoicesDraws& draws;
    std::vector<VoicesCall>& rom;
    std::vector<VoicesCall>& mine;
    VoicesCounts n {};
    std::vector<uint32_t> around {};   // rom.4's span of the words as the event found them
};

// An event on both sides, after the draws: what differs, or nothing
using VoicesEvent = std::function<std::string(VoicesSides&, std::mt19937&)>;
std::span<const VoicesEvent> voicesEvents();
// The case's first events, rom.4's three inits in its order
std::string voicesInits(VoicesSides& s);
// The port's voices, heads and words against rom.4's, nothing else in their span moved, then the calls; empty when
// nothing differs
std::string voicesDifference(VoicesSides& s);
// Before an event: the port's words into rom.4's memory, the span's words around them kept to see nothing else moves,
// both logs cleared and the answers drawn afresh from `seed`
void voicesPrepare(VoicesSides& s, uint32_t seed);
// Each voice of the port's `list` given a note when it has none (a sounding voice always has one)
void voicesGiveNotes(VoicesSides& s, uint32_t list, std::mt19937& rng);
// The voices of the port's `list` by address, in order
std::vector<uint32_t> voicesOf(VoicesSides& s, uint32_t list);
// rom.4's side: the two destroys, the tone's end test and its cost hooked to log into `log` and answer from `draws`
void voicesInstallRom(Vc2Fw& fw, VoicesDraws& draws, std::vector<VoicesCall>& log);
// The first call the two logs differ at; empty when they don't
std::string voicesCallsDifference(const std::vector<VoicesCall>& rom, const std::vector<VoicesCall>& port);
// The draws before an event (every voice's words at a case's first), and the notes' kinds and tones in rom.4's memory
void voicesSteer(VoicesSides& s, std::mt19937& rng, bool all);
void voicesDrawNotes(Vc2Fw& fw, std::mt19937& rng);
// The assigner's part (v277, Vc2AssignCheck.cpp), after the voices' cases on its own generator: its cases' count
// that differ, its counts said
long vc2AssignPart(VoicesSides& s, const Vc2Run& run);
// The release path's and note start's part (v278, Vc2AssignReleaseCheck.cpp), after the assigner's on the seed after
// its: its cases' count that differ, its counts said
long vc2AssignReleasePart(VoicesSides& s, const Vc2Run& run);
