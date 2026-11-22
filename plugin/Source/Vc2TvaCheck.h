#pragma once
// xt_vc2tva's shared parts (v233): the calls each side logs (the OS's, which a told truncation's end makes); where the
// TVA's envelopes and LFOs and the fade's step are in the interpreter's memory, which the draws write and both sides
// read, rom.4's routines directly and the port through its fake parts; the two sides and their counts.
// XT_VC2TVA_MAIN only.
#include "Vc2CheckRig.h"
#include "Vc2Tva.h"
#include "Vc2TvaDraws.h"
#include <array>
#include <functional>
#include <span>
#include <vector>

inline constexpr uint32_t kTvaAt = 0x8C060000;        // the TVA, in Vc2Fw's RAM
inline constexpr uint32_t kTvaBlockAt = 0x8C058000;   // a note-on's or a new block's block, the same
inline constexpr uint32_t kTvaInAt = 0x8C059000;      // a render's input
inline constexpr uint32_t kTvaOutAt = 0x8C05a000;     // and its output
inline constexpr uint32_t kTvaRecordsAt = 0x8C05b000; // the envelopes, then the LFOs the TVA reads, 0x80 apart
inline constexpr uint32_t kTvaRecordStride = 0x80, kTvaLfosFrom = 0x400;
inline constexpr int kTvaRecords = 2;                 // of each kind
inline constexpr uint32_t kTvaFadeStep = 0x8993eb08;  // [FW] the fade's step, rom.4's word in .data
inline constexpr uint32_t kTvaState = 0x00, kTvaLfoOut = 0x38;   // [FW] an envelope's state, an LFO's output

// [FW] where an envelope keeps the floats the TVA reads (Vc2Tva.h's)
struct TvaEnvelopeFloat {
    uint32_t at;
    float Vc2TvaEnvelope::*field;
};
inline constexpr std::array<TvaEnvelopeFloat, 5> kTvaEnvelopeFloats { { { 0x1c, &Vc2TvaEnvelope::attack },
                                                                       { 0x20, &Vc2TvaEnvelope::next },
                                                                       { 0x4c, &Vc2TvaEnvelope::out },
                                                                       { 0x58, &Vc2TvaEnvelope::onset },
                                                                       { 0x6c, &Vc2TvaEnvelope::stageTime } } };
inline uint32_t tvaEnvelopeAt(int k) { return kTvaRecordsAt + kTvaRecordStride * (uint32_t)k; }
inline uint32_t tvaLfoAt(int k) { return kTvaRecordsAt + kTvaLfosFrom + kTvaRecordStride * (uint32_t)k; }

// The OS's call as either side made it: its event and flags
struct TvaCall {
    uint32_t event, flags;
    bool operator==(const TvaCall&) const = default;
};

// The port's parts: the TVA's envelopes, LFOs and fade step read from the interpreter's memory where the draws wrote
// them (rom.4's routines write none of it), the OS's call logged
class TvaPortParts final : public Vc2TvaParts {
public:
    TvaPortParts(Vc2Fw& fw, std::vector<TvaCall>& calls) : fw_(fw), calls_(calls) {}
    Vc2TvaEnvelope envelope(uint32_t at) override;
    float lfoOut(uint32_t at) override { return f32(at + kTvaLfoOut); }
    float fadeStep() override { return f32(kTvaFadeStep); }
    void setEvent(uint32_t event, uint32_t flags) override { calls_.push_back({ event, flags }); }

private:
    float f32(uint32_t at) { return std::bit_cast<float>(fw_.cpu.rd32(at)); }
    Vc2Fw& fw_;
    std::vector<TvaCall>& calls_;
};

struct TvaRunCounts {
    long events = 0, noteOns = 0, updates = 0, renders = 0, fades = 0, fadesEnded = 0, told = 0, over = 0, silent = 0;
};

// The two sides: rom.4's interpreter and its log, the port's TVA, parts, log and output
struct TvaSides {
    Vc2Fw& fw;
    Vc2Tva& tva;
    TvaPortParts& parts;
    std::vector<TvaCall>& rom;
    std::vector<TvaCall>& port;
    std::array<float, Vc2Tva::kOut>& out;
    TvaRunCounts n {};
};

// An event on both sides, after the draws: what differs, or nothing
using TvaEvent = std::function<std::string(TvaSides&, std::mt19937&)>;
std::span<const TvaEvent> tvaEvents();
// The TVA's words against rom.4's, then the output, then the calls; empty when nothing differs
std::string tvaDifference(TvaSides& s);
// rom.4's side: the OS's call hooked to log into `log`
void tvaInstallRom(Vc2Fw& fw, std::vector<TvaCall>& log);
// The first call the two logs differ at; empty when they don't
std::string tvaCallsDifference(const std::vector<TvaCall>& rom, const std::vector<TvaCall>& port);
