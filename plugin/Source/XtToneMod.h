#pragma once
// The tone's MOD section and its render order by Structure Type (v336: moved as they stood from XtAnalog.h, which
// includes this, so its includers see them as before; the code in XtAnalog.cpp).
#include <cstdint>

// The MOD section: one object at tone +0x1708, after both OSCs in Structure 0.
// The patch's values (input +6…+0xa, IDs 0x294…0x298): Switch, Type 0 MIX, 1
// RING, 2 FM, 3 ENV RING, 4 OSC SYNC, Original Level, Attack and Release
// Times, 0…127.
struct XtModParams {
    bool on = false;
    int type = 0, level = 127, attack = 0, release = 0;
    bool operator==(const XtModParams&) const = default;
};
// The block (tone block +0x5dc), which the converter (0x894e0920) writes:
//   off, or MIX    type 0, a = b = 1 (c left as it was)
//   RING, FM       a = 1, b = level curve(Level)/32768, c = 1
//   ENV RING       a = Attack/127, b = Release/127, c = 1
//   OSC SYNC       a = 1 (b and c left as they were)
// What it leaves stays in the block, and an update copies it into a held note.
struct XtModBlock {
    uint32_t type = 0;
    float a = 0, b = 0, c = 0;
};
void xtModBlock(const XtModParams& p, XtModBlock& b);

// FM's delay lines, for all tones: 496 floats per line, the 24 tones' L lines
// (0x89f91c5c) followed by their R lines (0x89f9d65c).  A tone reads outside its own
// lines when the modulator passes full scale, so they are one array here too; past
// its ends the XT reads other memory, and the port reads 0.
struct XtModShared {
    static constexpr int kTones = 24, kLine = 496;
    float line[2 * kTones * kLine] = {};
};

// The MOD object.  The note-on (0x894f1b00, then the chooser 0x894f1d90) takes
// the type and the routine for the note; a new block on a held note (0x894f1c60)
// changes neither, only the targets, by the note's type.  So a held note goes on
// with its routine under gains from another Type's block.  All of the fields
// stay from note to note.
//   a, b, c   +4, +8, +0xc the targets, +0x18, +0x1c, +0x20 the current values:
//             each routine moves them 0.009375 of the way per sample, from the
//             difference at the block's start
//   MIX       (0x89527d80) out = L1·a + L2·b.  At gains of 1 the plain sum.
//   RING      (0x89527e00) out = ((L2·a + b)·c)·L1, with a's target (+4)/2^31,
//             both channels; b is the Original Level
//   FM        (0x89527b80, with either voice's stereo flag 0x89527c50) L1 goes
//             into the tone's line at +0x30, kept from note to note (the note-on
//             clears the lines); d = (L2 + (2^31 − 128))·a·1.15484e−7, so 0…496
//             samples at full scale; out = (line[w − ⌊d⌋ − 1]·f + line[w − ⌊d⌋]·(1 −
//             f) + L1·b)·c, each index brought back by 496 once.  c moves by (target
//             − c)·(2^31 − 128) per sample, which only an unwritten block reaches.
//             The first reads L only and writes both channels.
//   ENV RING  (0x895279d0, with either voice's stereo flag 0x89527aa0) L1 under OSC
//             2's level, and OSC 2 is not heard.  The level (0x895017e0): the sum of
//             |L2| over 32 samples of L, the last of each eight counted twice, ×
//             0x2e7fffff; compared with the last block's (+0x2c, kept from note to
//             note): rising or equal the Attack's rate, falling the Release's.  env
//             (+0x28, 0 at note-on) moves (level − env)·((1 − t)·0.005 + 0.0001) every
//             4 samples; out = L1·(env·gain), gain moving toward +0xc each sample
//             from +0x18.  The first reads L only.
//   OSC SYNC  OSC 2's mask (the tone's render: while +0x40 is set it goes to OSC 1's
//             sync input and to +0x44) restarts OSC 1.  With OSC 1 analog (0x894f1aa0):
//             out = L1·v + L2·(1 − v), v = +0x28 from the block's a, so at 1 OSC 2 is
//             not heard.  Otherwise (0x894f1a40), a PCM OSC 1 or none: at a mask bit
//             the held values (+0x18, +0x1c) become L1·v; out = (L1 − held)·(1 − v/2).
//             The note-on sets +0x40 and only a note-on for another Type's block
//             above 4 clears it, so it outlasts the note: the next note, of any Type,
//             still syncs OSC 1, until a new block on it (setBlock).
// process: L1 and L2 in the XT's units, 32 frames of interleaved L/R; out may be
// l1.
class XtMod {
public:
    // stereo1, stereo2: the voices' +0x960 as their setups leave them; osc1Analog:
    // OSC 1's kind is 3; tone: the tone's number (+0x3c), its lines in the pool
    void noteOn(const XtModBlock& b, bool stereo1, bool stereo2, bool osc1Analog, XtModShared& shared, int tone);
    void setBlock(const XtModBlock& b);
    void process(float* out, const float* l1, const float* l2);
    // MIX at gains of 1, targets and current values: the plain sum
    bool plain() const;

    // the state, for the firmware check
    uint32_t type() const { return type_; }
    uint32_t stereoCode() const { return stereo_; }
    uint32_t routine() const;                           // the routine's address (+0x50)
    float target(int i) const { return t[i]; }
    float current(int i) const { return g[i]; }
    int writeIndex() const { return w_; }
    float env() const { return env_; }                  // +0x28
    float lastLevel() const { return last_; }           // +0x2c
    // the tone's render: while this is set OSC 2's mask goes to OSC 1 and to the MOD
    bool syncOn() const { return sync_; }               // +0x40
    void setMask(uint32_t m) { mask_ = m; }             // +0x44
    uint32_t mask() const { return mask_; }

private:
    uint32_t type_ = 0, stereo_ = 0;
    bool analog_ = false;
    float t[3] = {}, g[3] = {};                         // a, b, c: the targets and current values
    XtModShared* shared_ = nullptr;
    int lineL = 0, lineR = 0, w_ = 0;                   // the tone's lines in the pool, +0x30
    float env_ = 0, last_ = 0;
    bool sync_ = false;
    uint32_t mask_ = 0;
    float lineAt(int base, int i) const;
    float fmTap(int base, float m, float x, float a, float b, float c) const;
    void mix(float* out, const float* l1, const float* l2);
    void ring(float* out, const float* l1, const float* l2);
    void fm(float* out, const float* l1, const float* l2);
    void fmStereo(float* out, const float* l1, const float* l2);
    void envRing(float* out, const float* l1, const float* l2, bool stereo);
    void syncAnalog(float* out, const float* l1, const float* l2);
    void syncHold(float* out, const float* l1);
};

// The tone's render by Structure Type (ID 0x293, 0…2, Type1…Type3 on the panel: the
// tone block's +0, kept at +0x1770, the routine's pointer at +0x1774).  The tone's
// note-on is the only writer of either, so a sounding note keeps its Structure.
//   0x894f6d40: OSC 2, OSC 1, MOD, COSM 1, COSM 2
//   0x894f6e20: OSC 2, OSC 1, COSM 1, MOD, COSM 2
//   0x894f6f40: OSC 2, COSM 2, OSC 1, COSM 1, MOD
// Before the MOD a COSM has its OSC's buffer alone; from the MOD on, the MOD's output.
// Each then goes to the TVA.
enum class XtToneStep : uint8_t { Osc2, Osc1, Mod, Cosm1, Cosm2, Cosm2OnOsc2, End };
const XtToneStep* xtToneOrder(int structure);
// Which OSC, 0 or 1, COSM section k takes its pitch ratio (+0x144), root (+0x9a4) and
// stereo flag (+0x960) from: OSC 2's only for COSM 2 in the third Structure (the tone's
// note-on 0x894f6320, 0x894f6540, and its update 0x894f76b0).
int xtCosmOsc(int structure, int k);
