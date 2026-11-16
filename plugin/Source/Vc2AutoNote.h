#pragma once
// The VC-2's Auto Note and the pitch source (v226): rom.4's input stage 0x893ebb00 around the pitch tracker's block,
// Auto Note's state in .data (0x8993fe3c…0x8993fe53) and the bend's globals (0x899d994c…0x899d995f), word for word.
// With the On Switch on, Auto Note posts a note-on of C4 to the tone generator and, while it sounds, keeps a ratio,
// C4's period over the tracked one, which the converters multiply into the note's pitch: C4 bent to the sung pitch, no
// semitone steps.  At 1 a key held by the player clears it; at 2 it plays regardless.  The pitch-source getter
// 0x89394a40 gives a converter that ratio while the tone generator's object says Auto Note's note sounds, and
// otherwise the bend 0x893ebc70, the singer's movement by a depth.  The semitone detector 0x893ef400, its query
// 0x893ef380 and the snap 0x893ebde0 are not ported: their switch (0x8993fe48) has one writer, the Legato Switch's
// setter, which stores 0, so none of them runs; the switch and its two tests are kept.  Bit for bit: xt_vc2autonote
// against rom.4's routines in Sh4Interp, the tracker inside.  HARDWARE_FIRMWARE.md, "Auto Note and the pitch source";
// research/VC2_VOCAL_DESIGNER.md, "Kind 6's pitch source, run".  Audio thread.
#include "Vc2Tracker.h"
#include <cstdint>

// Auto Note's state as rom.4 lays it out from 0x8993fe3c (.data)
struct Vc2AutoNoteWords {
    int32_t note = -1;       // fe3c: the note Auto Note sounds, −1 none
    float period = 0.0f;     // fe40: that note's period, C4's; 0 none
    int32_t release = 0;     // fe44: a release asked for, done at the next block
    int32_t detector = 0;    // fe48: the semitone detector's switch, which nothing sets
    float ratio = 1.0f;      // fe4c: C4's period over the tracked one
    int32_t onSwitch = 0;    // fe50: Auto Note's On Switch, 0…2
};
static_assert(sizeof(Vc2AutoNoteWords) == 0x18, "rom.4's layout, 0x8993fe3c…0x8993fe54");

// The bend's globals as rom.4 lays them out from 0x899d994c (.bss)
struct Vc2AutoNoteGlobals {
    int32_t bendStarted;   // 994c: the smoothed period has been started
    float smoothed;        // 9950: the tracked period, smoothed
    float c4Period;        // 9954: 44100 / 261.63, the init's
    float a440Period;      // 9958: 44100/440, the init's, which nothing reads
    float unreadRate;      // 995c: 880/44100, the same
};
static_assert(sizeof(Vc2AutoNoteGlobals) == 0x14, "rom.4's layout, 0x899d994c…0x899d9960");

// The tone generator's pitch source for the part, 0x89989748: its note-on and note-off keep it; read here
struct Vc2PitchSource {
    int32_t note;     // +0: the key Auto Note's note took (−1 none)
    int32_t others;   // +4: how many other keys are down
    float depth;      // +8: the bend's depth
};

// The tone generator as the input stage reaches it: its mailbox
class Vc2AutoNoteHost {
public:
    virtual ~Vc2AutoNoteHost() = default;
    virtual bool noteOn(int32_t note, float level) = 0;   // 0x89391dc0: true when the note-on (note + 12) is posted
    virtual void noteOff(int32_t note) = 0;               // 0x89391e20
};

class Vc2AutoNote {
public:
    static constexpr int32_t kC4 = 60;   // [FW] 0x893ebbb8: the note Auto Note plays
    void init();                          // 0x893ebe50, the init table's
    // 0x893ebb00: n (1…32) samples of the mic through the tracker, then Auto Note
    void block(Vc2Tracker& tracker, const float* in, int n, const Vc2PitchSource& source, Vc2AutoNoteHost& host);
    void setSwitch(int32_t on);           // 0x893ebc20: 0…2 when new, a sounding note released
    void setLegato() { w.detector = 0; }  // 0x893ebc60: the detector's switch off, whatever it's given
    void askRelease();                    // 0x893ebcf0: the tone generator's, at a key's note-on
    float bend(const Vc2Tracker& tracker, float depth);   // 0x893ebc70: 1 − depth × the movement over C4's period
    float ratioFor(const Vc2Tracker& tracker, const Vc2PitchSource& source);   // 0x89394a40

    Vc2AutoNoteWords w {};
    Vc2AutoNoteGlobals g {};

private:
    void clear();   // no note: −1, period 0, no release, ratio 1
};
