#pragma once
// xt_vc2voices' assigner part's shared parts (v277): the port's assigner parts on the voices' sides (the voices' calls
// through VoicesPortParts, the assigner's logged into the same log with their arguments), the counts, and the events.
// XT_VC2VOICES_MAIN only.
#include "Vc2Assigner.h"
#include "Vc2PartVoicesCheck.h"

inline constexpr uint32_t kAssignMessagesAt = 0x89987420, kAssignMessageBytes = 0x24;   // [FW] the portamento keys
inline constexpr uint32_t kAssignTablesAt = 0x89982720, kAssignTableBytes = 0x134;      // [FW] the key tables
inline constexpr uint32_t kAssignPrepareEntry = 0x3c, kAssignRetuneEntry = 0x18;        // [FW] a voice's two slots
inline constexpr uint32_t kAssignNoteOnByte = 0x740;                                     // [FW] a note's byte
inline constexpr int kAssignMessages = 4, kAssignTables = 3;

// The port's parts for the assigner: the voices' through the voices' sides, the rest logged and answered alike
class AssignPortParts final : public Vc2AssignParts {
public:
    explicit AssignPortParts(VoicesSides& s) : s_(s) {}
    void destroy(uint32_t voice) override { s_.parts.destroy(voice); }
    uint32_t toneOf(uint32_t note) override { return s_.parts.toneOf(note); }
    int8_t kindOf(uint32_t note) override { return s_.parts.kindOf(note); }
    bool ended(uint32_t tone) override { return s_.parts.ended(tone); }
    float cost(uint32_t tone) override { return s_.parts.cost(tone); }
    void prepare(uint32_t voice, const Vc2AssignCall& call, const Vc2Legato& legato) override;
    void retune(uint32_t voice, uint32_t message, uint32_t part, uint32_t value) override;
    void noteOn(uint32_t note, uint32_t partner, int32_t onByte) override;
    void startNote(uint32_t note) override;
    void truncateOnce(uint32_t note) override;
    int8_t onByteOf(uint32_t note) override;
    uint32_t tableOf(uint32_t message) override { return s_.fw.cpu.rd32(message); }
    void waitQueue(uint32_t queue, uint32_t voice) override;
    void keyOff(uint32_t voice) override;
    uint8_t partSetting(uint32_t part, uint32_t offset) override;
    void readParameters(uint32_t part) override;
    uint8_t firstByte(uint32_t object) override { return s_.fw.cpu.rd8(object); }
    int32_t clock() override;

    int32_t ms = 0;   // what the OS clock answers, drawn before an event

private:
    VoicesSides& s_;
};

inline constexpr uint32_t kAssignSettingsAt = 0x89995390, kAssignSettingBytes = 32;    // [FW] the parts' settings
inline constexpr uint32_t kAssignParametersAt = 0x89996120, kAssignParameterBytes = 24;   // [FW] their messages
inline constexpr uint32_t kAssignKeyOffEntry = 0x30;                                     // [FW] a voice's slot
inline constexpr uint32_t kAssignReadAt = 0x8939b050, kAssignClockAt = 0x893018d0;       // [FW] the message, clock

struct AssignCounts {
    long events = 0;
    std::array<long, 10> byEvent {};
    long prepared = 0, partners = 0, stolen = 0;
};

// The assigner's sides: the voices', the port's assigner and its parts, the note a few sounding voices are made to be
// (a call takes it half the time), the counts
struct AssignSides {
    VoicesSides& s;
    Vc2Assigner assigner {};
    AssignPortParts parts;
    Vc2AssignCall target {};
    AssignCounts n {};
};

// The note a prepare gives its voice, by the answer drawn: one of the notes always
uint32_t assignNoteOf(const VoicesAnswer& a);
// A call drawn: the target half the time, else one of the messages, a part, a byte, a word passed on and a kind
Vc2AssignCall assignDrawCall(AssignSides& a, std::mt19937& rng);

using AssignEvent = std::string (*)(AssignSides&, std::mt19937&);
std::span<const AssignEvent> assignEvents();
// A case's start (the voices noise, the notes drawn, rom.4's three inits; what differs), the second part's draws
// before an event (all at a case's first), and rom.4's side's hooks
std::string assignStart(AssignSides& a, std::mt19937& rng);
void assignSteer(AssignSides& a, std::mt19937& rng, bool all);
void assignInstallRom(VoicesSides& s);
