#pragma once
// The VC-2 part's settings by ID (v286, step 4.3's fourteenth version, item 9's first part): rom.4's readers that fill
// the tables C's children read, word for word, each giving up (false) when its task's held flag is set before or after
// its reads: the common's 0x893a1be0, the envelopes' 0x8939ada0 by rows 0x8939ab90, the LFOs' 0x8939b9a0 by rows
// 0x8939b860 and their system's 0x8939b970, the mix's 0x8939bff0 by rows 0x8939bf30, the TVA's 0x893a71f0 by rows
// 0x893a6f80 and 0x893a70c0 (the curve's law 0x893a8120 and the depth's 0x893a8070) and its systems' 0x893a7160,
// 0x893a71a0 and 0x893a71d0; and (v287) the part's bytes by kind 0x893a8250 and the COSM inputs' 0x89399ed0, a COSM's
// type's run of IDs from Vc2PartSettingsTables.  The parameters by read object and ID, and the flags, through
// Vc2PartParams.  Not in the
// sound yet.  Bit for bit: xt_vc2settings.  HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID".  The notes' task.
#include "Vc2Engine.h"
#include <array>
#include <cstdint>
#include <functional>

// The read objects a reader sets up, each a block of parameters the getter 0x89324350 reads by ID: the part's
// (0x893207b0), the system's three (0x89320790, 0x89320760, 0x89320f60) and a note kind's three (0x89320a50,
// 0x89320ad0, 0x89320b70); and a COSM's (0x89320c10, v287), which the typed getter 0x89324370 reads
enum class Vc2ParamBlock : uint8_t { kPart, kSystem, kSystemB, kSystemC, kKindA, kKindB, kKindC, kCosm };

class Vc2PartParams {
public:
    virtual ~Vc2PartParams() = default;
    // The getter on a block set up for `part` and `kind` (the system's take neither)
    virtual int32_t value(Vc2ParamBlock block, int part, int kind, int16_t id) = 0;
    // The typed getter on a COSM's block, by the part, kind and COSM (v287)
    virtual int32_t cosmValue(int part, int kind, int cosm, int16_t id) = 0;
    // rom.4's flag byte 0x8993d0c0 + `flag`, a task's hold on the tables
    virtual bool held(int flag) = 0;
};

// [FW] the tables, as rom.4 lays them
struct Vc2PartRows {
    std::array<uint8_t, 96> commons {};         // 0x89997090: 6 bytes a part
    std::array<uint8_t, 0x8200> envelopes {};   // 0x89e2765c: 2080 a part, 130 a kind, 10 an envelope
    std::array<uint8_t, 0x3202> lfos {};        // 0x89e2f85c: 800 a part, 50 a kind, 10 an LFO; the system's byte
    std::array<uint8_t, 0x1600> mixes {};       // 0x89e32a60: 352 a part, 22 a kind
    std::array<uint8_t, 0x1806> tvas {};        // 0x89e4987c: 384 a part, 24 a kind; the systems' trailer
    uint8_t controls = 0;                       // 0x89997fcc: the part's CC 7 and 11 on the TVA's level
    std::array<uint8_t, 256> bytes {};          // 0x89998a14: the part's byte by kind, 16 a part (v287)
    std::array<uint8_t, 0x5400> cosms {};       // 0x89e2003c: 0x540 a part, 84 a kind, 42 a COSM input (v287)
    // 0x89e371e0: 0x1020 a part (a 32-byte header, then 0x100 a kind, 0x80 an OSC), then the systems' (v288)
    std::array<uint8_t, 0x1023c> oscs {};
    // 0x89989c84: the OSCs' step counts, a halfword by kind and OSC, part 0's alone (the update queue follows); the
    // OSC block's reader clears one when its Wave Number changes (v289)
    std::array<uint8_t, 64> steps {};
    float robot = 0.0f;   // 0x89997fdc: robot human depth ÷ 127 when a kind's OSC is EXTIN, else 0 (v290)
};

// What the vocoder reader hands on (v290): kind 6's Unvoice Level, the part mixer's setters and targets, Auto Note's
// setters, the vocoder's mode switch and its settings' wrapper
class Vc2SettingsParts {
public:
    virtual ~Vc2SettingsParts() = default;
    virtual void setUnvoiceLevel(float v) = 0;                               // 0x899d9968
    virtual void setDirect(float v) = 0;                                     // 0x893b1eb0
    virtual void setReverbSend(float v) = 0;                                 // 0x893b1ec0
    virtual void setMfxSend(float v) = 0;                                    // 0x893b1ed0
    virtual void mixerTargets() = 0;                                         // 0x893b1ee0
    virtual void setAutoNote(int32_t on) = 0;                                // 0x893ebc20
    virtual void setAutoNoteLegato() = 0;                                    // 0x893ebc60, given 0
    virtual void setVocoderMode(int32_t mode) = 0;                           // 0x893b1c00 → 0x893f0600
    virtual void vocoderSettings(Vc2VocoderSettings& s, int32_t stereo) = 0; // 0x893b1be0 → 0x893f06a0
};

// The OSC block's reader the part's reader calls for each kind and OSC, false to stop (v289)
using Vc2OscBlockReader = std::function<bool(int kind, int osc)>;

class Vc2PartSettings {
public:
    explicit Vc2PartSettings(Vc2PartParams& params) : p_(params) {}
    bool common(Vc2PartRows& t, int part);                          // 0x893a1be0 (0x893a1b40)
    bool envelopes(Vc2PartRows& t, int part);                       // 0x8939ada0
    bool envelopeRow(Vc2PartRows& t, int part, int kind, int i);    // 0x8939ab90
    bool lfos(Vc2PartRows& t, int part);                            // 0x8939b9a0
    bool lfoRow(Vc2PartRows& t, int part, int kind, int i);         // 0x8939b860
    void lfoSystem(Vc2PartRows& t);                                 // 0x8939b970
    bool mixes(Vc2PartRows& t, int part);                           // 0x8939bff0
    bool mixRow(Vc2PartRows& t, int part, int kind);                // 0x8939bf30
    bool tvas(Vc2PartRows& t, int part);                            // 0x893a71f0
    bool tvaRow(Vc2PartRows& t, int part, int kind);                // 0x893a6f80
    bool tvaVelocityRow(Vc2PartRows& t, int part, int kind);        // 0x893a70c0
    void tvaTrailer(Vc2PartRows& t);                                // 0x893a7160
    void tvaRoute(Vc2PartRows& t);                                  // 0x893a71a0
    void tvaControls(Vc2PartRows& t);                               // 0x893a71d0
    bool bytes(Vc2PartRows& t, int part);                           // 0x893a8250 (v287)
    bool byteRow(Vc2PartRows& t, int part, int kind);               // 0x893a81d0
    bool cosms(Vc2PartRows& t, int part);                           // 0x89399ed0
    bool cosmRow(Vc2PartRows& t, int part, int kind, int cosm);     // 0x89399c90: its type and the type's IDs
    bool cosmVelocityRow(Vc2PartRows& t, int part, int kind, int cosm);   // 0x89399d60: two envelopes' velocity
    // v288, Vc2PartSettingsOsc.cpp: the OSC table's systems' and part reader
    void oscTrailer(Vc2PartRows& t);                                // 0x893a1000
    void oscCalibration(Vc2PartRows& t);                            // 0x893a1060
    bool oscs(Vc2PartRows& t, int part);                            // 0x893a1410
    // v289: the part's reader with the block reader given (oscs gives oscBlock; xt_vc2settings' third part a fake),
    // and the OSC block's reader, Vc2PartSettingsOscBlock.cpp
    bool oscRows(Vc2PartRows& t, int part, const Vc2OscBlockReader& block);
    bool oscBlock(Vc2PartRows& t, int part, int kind, int osc);     // 0x893a04e0
    // v290, Vc2PartSettingsVocoder.cpp: the vocoder's settings, the mixer's levels and Auto Note's switch
    bool vocoder(Vc2PartRows& t, int part, Vc2SettingsParts& out);  // 0x893a72f0

private:
    void velocityPair(uint8_t* curve, uint8_t* depth, int part, int kind, int envelope);
    static uint8_t* oscBlockAt(Vc2PartRows& t, int part, int kind, int osc);   // v289
    void waveNumber(Vc2PartRows& t, uint8_t* at, int32_t v, int part, int kind, int osc);
    void oscHeader(uint8_t* row, int part);
    bool oscVelocityRow(uint8_t* block, int part, int kind, int osc);
    Vc2PartParams& p_;
};

// 0x893a8120: the velocity curve's code from its parameter, 1…7 less 1, else 7
uint8_t vc2CurveOf(int32_t x);
// 0x893a8070: the velocity depth from its parameter, ±32 to ±100, past them on by 31sts
int32_t vc2DepthOf(int32_t x);
