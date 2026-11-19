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
#include "Vc2MatrixSetup.h"
#include "Vc2PartNodes.h"
#include <array>
#include <cstdint>
#include <functional>

// The read objects a reader sets up, each a block of parameters the getter 0x89324350 reads by ID: the part's
// (0x893207b0), the system's three (0x89320790, 0x89320760, 0x89320f60) and a note kind's three (0x89320a50,
// 0x89320ad0, 0x89320b70); and a COSM's (0x89320c10, v287), which the typed getter 0x89324370 reads; and (v291) one
// never set up, Auto Note's switch's second with flag 0 held: rom.4 reads through the stack's leftover word; and (v298)
// the system's fourth (0x893206f0, the store object's first word, where system B's is its second)
enum class Vc2ParamBlock : uint8_t {
    kPart, kSystem, kSystemB, kSystemC, kKindA, kKindB, kKindC, kCosm, kLeftover, kSystemD
};

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
    // 0x89995390: the parts' settings, 32 a part (v278's): the key's offset, mono, Legato, Patch Tempo, the zones'
    // upper keys, Vocoder Sw, Split Point, Modulator Key Mode (v291)
    std::array<uint8_t, 512> modes {};
    uint32_t heldNote = 0;   // 0x899891d0: 1 with a patch of Category 10 or 11, a note held for the carrier (v294)
    // v296: Time Trip Assign X and Y (0x89989404), OFF and CC32 as −1; Patch Remain (0x899826fd); Number of Voices,
    // Offset of Voice Number and the two CPU Powers as last read (0x8993d02c)
    std::array<int8_t, 2> tripAssigns {};
    uint8_t patchRemain = 0;
    std::array<int16_t, 4> voiceCounts {};
    // v299: ChordMemory Switch as last read (0x8993d03c); each channel's byte (0x899893e0) that lets the Hold pedal's
    // release (below 64) be heard there (0x89393980), 1 after the routing's reset
    int32_t chordMemory = 0;
    std::array<uint8_t, 16> holdLifts {};
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
    // v291, the mode row's: All Sound Off to `source`'s `channel`, a message 120 made (0x89393c00) and handed on
    // (0x89393ba0, its last argument 0)
    virtual void allSoundOff(int source, int32_t channel) = 0;
    // v294, the Category's: the budget's setter, a controller to a source's channel, the sounding voices of a node's
    // word truncated and queued, all notes off (event 0x80 to the notes' task)
    virtual void setBudget(int32_t power) = 0;                                                 // 0x89391240
    virtual void controller(int source, int32_t channel, int32_t number, int32_t value) = 0;   // 0x89392f90
    virtual void truncateVoices(uint32_t word) = 0;                                            // 0x89390fd0
    virtual void allNotesOff() = 0;                                                            // 0x89391e30
    // v296, the voices' counts': the reserve of `count` voices from `offset`, CPU Power Offset for Voice's setter
    virtual void reserve(int32_t count, int32_t offset) = 0;                                   // 0x893911b0
    virtual void setPowerOffset(int32_t power) = 0;                                            // 0x89391260
    // v297, the parameters' messages': the reload task's request and run for a part (by its object at 0x89996120 + 24
    // × part), part 0's receive channel and the routing's reset (each answering whether it changed anything; the
    // receive channel's always 1, v298's `Vc2PartSettings::receive`, which the wiring gives here; the routing's reset's
    // always 1 too, v299's `Vc2PartSettings::routing`), and every sounding voice truncated
    virtual void reloadRequest(int part) = 0;                                                  // 0x8939af40
    virtual void reloadRun(int part) = 0;                                                      // 0x8939af10
    virtual bool receive() = 0;                                                                // 0x89393fb0
    virtual bool routing(bool on) = 0;                                                         // 0x89393e30
    virtual void truncateAll() = 0;                                                            // 0x89391120
    // v298, the receive channel's: the held note's event 0x200 to the notes' task (its key 60 at 127 on source 4)
    virtual void heldNoteOn() = 0;                                                             // 0x89391e50
};

// G's channel word's two sources (v292): the word at 0x8999738c, with Part 1 Receive Switch off; and with it on, the
// channel states' table 0x8950d6e0's word for the channel, by the source byte at 0x8950d6d9 (64 bytes a source)
class Vc2ChannelStates {
public:
    virtual ~Vc2ChannelStates() = default;
    virtual uint32_t unreceived() = 0;
    virtual uint32_t receiving(int32_t channel) = 0;
};

// What the parameters' messages reach (v297): the tables, G and the Matrix's set-up, the nodes and keys, the channel
// states, the calls out and the nodes' own
struct Vc2PartWorld {
    Vc2PartRows& t;
    Vc2MatrixSettings& g;
    Vc2MatrixSetup& setup;
    Vc2PartNodes& nodes;
    Vc2Keys& keys;
    Vc2ChannelStates& states;
    Vc2SettingsParts& out;
    Vc2NodeParts& nodeParts;
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
    // v291, Vc2PartSettingsModes.cpp: the mode row (All Sound Off to the part's nodes' channels) and Auto Note's switch
    bool modes(Vc2PartRows& t, int part, const Vc2PartNodes& nodes, Vc2SettingsParts& out);   // 0x89396f70
    void autoNoteSwitch(Vc2SettingsParts& out);                                                // 0x89396f90
    // v292, Vc2PartSettingsDispatch.cpp: the dispatchers, each reader in turn, the part's stopping at the first that
    // gives up (the others' each answer 1), the Matrix's set-up v247's, given; and G's channel word
    bool readPart(Vc2PartRows& t, Vc2MatrixSettings& g, Vc2MatrixSetup& setup, int part, const Vc2PartNodes& nodes,
                  Vc2SettingsParts& out);                                        // 0x8939b170
    void readSystem(Vc2PartRows& t, Vc2MatrixSettings& g, Vc2ChannelStates& states);   // 0x8939b0d0
    void readAssigns(Vc2PartRows& t, Vc2MatrixSettings& g, Vc2MatrixSetup& setup);    // 0x8939b110
    void readCalibration(Vc2PartRows& t);                                             // 0x8939b140
    void matrixChannel(Vc2MatrixSettings& g, Vc2ChannelStates& states);               // 0x893a5050
    // v294, Vc2PartSettingsCategory.cpp: the Category and budget, given the node of source 1 and `part`
    void category(Vc2PartRows& t, const Vc2PartNodes& nodes, int part, Vc2SettingsParts& out);   // 0x89392c60
    // v296: the budget by the Category (Vc2PartSettingsCategory.cpp); the system's readers (Vc2PartSettingsSystem.cpp)
    void budget(Vc2PartRows& t, Vc2SettingsParts& out);           // 0x893937b0
    void tripAssigns(Vc2PartRows& t);                             // 0x893943f0
    void patchRemain(Vc2PartRows& t);                             // 0x89391520
    void voiceCounts(Vc2PartRows& t, Vc2SettingsParts& out);      // 0x89391550
    // v297, Vc2PartSettingsMessages.cpp: the parameters' messages by kind and the part's, given a message's two
    // bytes; the held note's switch; the step counts' clear (part 0's alone are kept); a node's class by a flag
    void parameters(Vc2PartWorld& w, uint8_t kind, uint8_t byte1);       // 0x89392890
    void partParameters(Vc2PartWorld& w, uint8_t kind, uint8_t part);   // 0x89392830
    void heldNoteSwitch(Vc2PartRows& t, bool on, Vc2SettingsParts& out);   // 0x89391500
    void clearSteps(Vc2PartRows& t);                                      // 0x89392750, part 0
    void nodeClass(Vc2PartWorld& w, int part, int32_t plain);             // 0x89394370
    // v298, Vc2PartSettingsReceive.cpp: a part's receive channel (its four sources' nodes onto its Receive Channel,
    // their glides on the key tables; part 0's source 3 channel's change heard) and part 0's, each answering 1
    bool receiveChannel(Vc2PartWorld& w, int part);                       // 0x89393fd0
    bool receive(Vc2PartWorld& w);                                        // 0x89393fb0
    // v299, the same file: the routing's reset (ChordMemory Switch's change heard; with `on`, every node to no
    // channel, every glide and key table reset, part 0's receive channel, its node's held class), answering 1
    bool routing(Vc2PartWorld& w, bool on);                               // 0x89393e30

private:
    void zoneUppers(uint8_t* at, int part);                         // 0x89320870 (v291)
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
