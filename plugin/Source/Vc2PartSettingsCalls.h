#pragma once
// xt_vc2settings' calls as both sides log them (Vc2PartSettingsCheck.h's until v295): a read (the read object's block,
// part and kind, and the ID) or a call out (a block past the reads' and its words), and the port's calls out, each
// logged as rom.4's hooks log theirs.  XT_VC2SETTINGS_MAIN only.
#include "Vc2PartSettings.h"
#include "Vc2StaticMatrix.h"
#include <bit>
#include <cstring>
#include <vector>

struct SettingsCall {
    int32_t block, part, kind, sub, id;   // sub: a COSM's read object's COSM input (v287), the OSC (v288), else 0
    bool operator==(const SettingsCall&) const = default;
};

// The vocoder reader's calls out as the log has them (v290): a block past the reads', the value's bits as the ID;
// the settings struct a word each by its index as the kind (the pivots, +0x10 and +0x14, left out: rom.4 hands the
// stack's leftovers there, which the settings path writes before reading), the stereo flag as index 15
inline constexpr int32_t kOutDirect = 10, kOutReverb = 11, kOutMfx = 12, kOutTargets = 13, kOutAutoNote = 14;
inline constexpr int32_t kOutLegato = 15, kOutMode = 16, kOutSettings = 17;
// The mode row's (v291): a message made, its number as the ID; one handed on, its source, channel and last word as
// the part, kind and COSM; All Sound Off's number
inline constexpr int32_t kOutMessage = 18, kOutSoundOff = 19, kSoundOffNumber = 120;
// The Category's (v294): the budget's power, a controller (its source, channel, number and value as the part, kind,
// COSM and ID), the voices truncated by a word, all notes off
inline constexpr int32_t kOutBudget = 20, kOutController = 21, kOutTruncate = 22, kOutNotesOff = 23;
// The static M's (v295): the channel state's set-up (its argument as the COSM, the state as the ID), the enqueue (the
// queue as the kind, the node as the ID)
inline constexpr int32_t kOutChannelState = 24, kOutEnqueue = 25;
inline constexpr int kSettingsWords = 15, kStereoIndex = 15;
void logOut(std::vector<SettingsCall>& log, int32_t what, uint32_t value);
void logSettingsWords(std::vector<SettingsCall>& log, const uint32_t* words, int32_t stereo);

// The port's calls out: each logged as rom.4's hooks log theirs, Unvoice Level kept to compare with rom.4's word; and
// (v292) the Matrix set-up's 0x893b1ee0, the mixer's targets, which the dispatchers' set-up routines never reach
class SettingsPortOuts final : public Vc2SettingsParts, public Vc2MatrixPart, public Vc2StaticMatrixParts {
public:
    explicit SettingsPortOuts(std::vector<SettingsCall>& log) : log_(log) {}
    float unvoiceLevel = 0.0f;
    void levelsMoved() override { mixerTargets(); }
    void setUnvoiceLevel(float v) override { unvoiceLevel = v; }
    void setDirect(float v) override { logOut(log_, kOutDirect, std::bit_cast<uint32_t>(v)); }
    void setReverbSend(float v) override { logOut(log_, kOutReverb, std::bit_cast<uint32_t>(v)); }
    void setMfxSend(float v) override { logOut(log_, kOutMfx, std::bit_cast<uint32_t>(v)); }
    void mixerTargets() override { logOut(log_, kOutTargets, 0); }
    void setAutoNote(int32_t on) override { logOut(log_, kOutAutoNote, (uint32_t)on); }
    void setAutoNoteLegato() override { logOut(log_, kOutLegato, 0); }
    void setVocoderMode(int32_t mode) override { logOut(log_, kOutMode, (uint32_t)mode); }
    void vocoderSettings(Vc2VocoderSettings& s, int32_t stereo) override {
        uint32_t words[kSettingsWords];
        std::memcpy(words, &s, sizeof words);
        logSettingsWords(log_, words, stereo);
    }
    void allSoundOff(int source, int32_t channel) override {
        logOut(log_, kOutMessage, kSoundOffNumber);
        log_.push_back({ kOutSoundOff, source, channel, 0, 0 });
    }
    void setBudget(int32_t power) override { logOut(log_, kOutBudget, (uint32_t)power); }
    void controller(int source, int32_t channel, int32_t number, int32_t value) override {
        log_.push_back({ kOutController, source, channel, number, value });
    }
    void truncateVoices(uint32_t word) override { logOut(log_, kOutTruncate, word); }
    void allNotesOff() override { logOut(log_, kOutNotesOff, 0); }
    void channelStateInit(uint32_t state, int32_t arg) override {
        log_.push_back({ kOutChannelState, 0, 0, arg, (int32_t)state });
    }
    void enqueue(uint32_t queue, uint32_t node) override {
        log_.push_back({ kOutEnqueue, 0, (int32_t)queue, 0, (int32_t)node });
    }

private:
    std::vector<SettingsCall>& log_;
};
