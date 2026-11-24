#pragma once
// The part's settings, the part's own (v345, Vc2Synth whole's sixth version): its parameters by read object (a patch's
// store given, read as xt_vc2patch reads one, the kind its zone; with none, each ID's default less its centre, as
// rom.4's getter 0x89324350 reads a read object never set up, 0x8932b340, and a typed ID's typed default, 0x8932b530;
// the system's blocks the defaults, no system store yet), the held flags, the readers and messages on the part's tables
// (Vc2PartSettings) with the Matrix's set-up, and what they call out to the part's ports.  The set-up's levels
// (0x89997290) are the static M's (+0x188), whose region holds them, and the part mixer's offsets the same three
// floats: each move gives the mixer theirs, then its targets (0x893b1ee0).  Bit for bit: xt_vc2synth's load part.
// HARDWARE_FIRMWARE.md, "The part's settings and voices wired".  The notes' task.
#include "Vc2PartSettings.h"
#include "Vc2SynthVoices.h"

class Vc2SynthSettings final : public Vc2PartParams, public Vc2MatrixParams, public Vc2MatrixPart,
                               public Vc2SettingsParts, public Vc2ChannelStates, public Vc2NodeParts {
public:
    explicit Vc2SynthSettings(Vc2Synth& s) : s_(s) {}
    // The reload task's read of a part 0x8939b170; the system's 0x8939b0d0, the assignments' 0x8939b110, the
    // calibration's 0x8939b140; the parameters' messages 0x89392890 and the part's 0x89392830
    bool readPart(int part) { return settings.readPart(s_.rows, s_.g, setup, part, s_.nodes, *this); }
    void readSystem() { settings.readSystem(s_.rows, s_.g, *this); }
    void readAssigns() { settings.readAssigns(s_.rows, s_.g, setup); }
    void readCalibration() { settings.readCalibration(s_.rows); }
    void parameters(uint8_t kind, uint8_t byte1);
    void partParameters(uint8_t kind, uint8_t part);
    // The parameters by read object, the held flags (0x8993d0c0…c4)
    int32_t value(Vc2ParamBlock block, int part, int kind, int16_t id) override;
    int32_t cosmValue(int part, int kind, int cosm, int16_t id) override;
    bool held(int flag) override { return s_.held[(size_t)flag] != 0; }
    // The Matrix's: the part's block and the system's, the common's flag
    int32_t partValue(int part, int16_t id) override { return value(Vc2ParamBlock::kPart, part, 0, id); }
    int32_t systemValue(int16_t id) override { return value(Vc2ParamBlock::kSystem, 0, 0, id); }
    bool held() override { return held(0); }
    void levelsMoved() override;
    // What the readers and messages call out, each the part's port
    void setUnvoiceLevel(float v) override { s_.ring.g.unvoiceLevel = v; }
    void setDirect(float v) override { s_.mixer.setDirect(v); }
    void setReverbSend(float v) override { s_.mixer.setReverbSend(v); }
    void setMfxSend(float v) override { s_.mixer.setMfxSend(v); }
    void mixerTargets() override { s_.mixer.targets(); }
    void setAutoNote(int32_t on) override { s_.autoNote.setSwitch(on); }
    void setAutoNoteLegato() override { s_.autoNote.setLegato(); }
    void setVocoderMode(int32_t mode) override { s_.vocoder.setMode(mode); }
    void vocoderSettings(Vc2VocoderSettings& v, int32_t stereo) override;
    void allSoundOff(int source, int32_t channel) override;
    void setBudget(int32_t power) override { s_.voices.setBudget(power); }
    void controller(int source, int32_t channel, int32_t number, int32_t value) override;
    void truncateVoices(uint32_t word) override { s_.assigner.truncatePart(word, s_.voices, *s_.voiceParts); }
    void allNotesOff() override { s_.post(kVc2SynthNotesTask, kVc2SynthAllNotesOff); }
    void reserve(int32_t count, int32_t offset) override { s_.voices.reserve(count, offset, *s_.voiceParts); }
    void setPowerOffset(int32_t power) override { s_.voices.setOffset(power); }
    void reloadRequest(int part) override { s_.reloads.request(reloadOf(part)); }
    void reloadRun(int part) override { s_.reloads.run(reloadOf(part)); }
    bool receive() override;
    bool routing(bool on) override;
    void truncateAll() override { s_.assigner.truncateAll(s_.voices, *s_.voiceParts); }
    void heldNoteOn() override { s_.post(kVc2SynthNotesTask, kVc2SynthHeldNoteEvent); }
    // G's channel word's two sources: the static M's unreceived word (0x8999738c), the states' table by its source
    uint32_t unreceived() override { return s_.staticM.unreceived; }
    uint32_t receiving(int32_t channel) override;
    void stopVoices(uint32_t word) override { s_.assigner.stopPart(word, s_.voices, *s_.voiceParts); }

    Vc2PartSettings settings { *this };
    Vc2MatrixSetup setup { *this, *this };

private:
    Vc2PartWorld world() { return { s_.rows, s_.g, setup, s_.nodes, s_.keys, *this, *this, *this }; }
    static uint32_t reloadOf(int part) { return Vc2PartReloads::kAt + (uint32_t)(sizeof(Vc2Reload) * part); }
    Vc2Synth& s_;
};
