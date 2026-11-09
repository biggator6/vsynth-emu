#pragma once
// The V-Synth XT's SOLO grain player as a block-rendering voice, for the
// offline engine (VariphraseEngine::soloFirmwareResynthOffline) and the
// plugin's voices alike.  Behaviour as described in VariphraseEngine.cpp
// (firmware 0x8950cd90 chain).  Bit for bit against the firmware run in the
// SH-4 interpreter: xt_fw solocmp, and in a whole tone, xt_tone.
#include <cstdint>
#include <cstdio>
#include <vector>
#include "XtBeat.h"
#include "XtLoop.h"
#include "XtSoloEncoder.h"

// A sample prepared for SOLO playback: the samples and their grain records.
// A stereo wave has one record stream (encoded from (L+R) >> 1); the player
// reads both channels through the same grains (0x8950d1f0).
struct XtSoloPlan {
    std::vector<float> in;              // samples: mono, or the left channel
    std::vector<float> inR;             // the right channel of a stereo wave, else empty
    std::vector<double> segStart, segLen;
    std::vector<char> voiced;
    std::vector<int> info;              // record info word, −1 if unknown
    // The events and segments, as the wave's vpdt has them (xtSoloReadVpdt).  Event e: its position (1/16 sample: 0 the start, the
    // last the wave's end, between them the attacks the encoder found) and the segment containing it.  Segment k: its start (a record's,
    // the one nearest its attack), its first record (an index; size() is the terminator) and that record's first length word, as the
    // player reads it (int16).  pastStart, pastLen: what the player reads as the segment after the last (0x8950d770): the record count's
    // entry, the records' second word and the word at the count's offset.  Empty: one segment, the start and the end alone.
    std::vector<int32_t> evPos;
    std::vector<uint8_t> evKind;        // and its kind, its flags & 0xb (0x8950c7e0's +5): 9 the start, 10 the end, 0 an attack; V-Trainer's 1 and 2
    std::vector<int> evSeg;
    std::vector<int32_t> segPos;
    std::vector<int> segRec;
    std::vector<int16_t> segFirst;
    int32_t pastStart = 0x7fffffff;
    int16_t pastLen = 0;
    // The record after the last, the stream's terminator, which the player reads as any record (0x8950d4c0): its length and info words.
    // The XT's encodes write (256, 1), as do 298 of the 300 SOLO waves there are; V-Trainer 1.00's VP9SAMnoise1 (256, 0) and preset 270
    // (256, 128), whose unvoiced grains at the wave's end then play at rate 1, not the pitch (info bit 0).  Without a vpdt, the XT's.
    double termLen = 16.0;
    int termInfo = 1;
    // Diagnostics (read from the environment where the plan is built)
    bool anchorMode = true;             // VSE_SCHED=pos: record containing P
    bool forceUnvoiced = false;         // VSE_LITE=solo-np
    int forcedInfo = -1;                // VSE_LITEINFO
    size_t size() const { return segStart.size(); }
    bool stereo() const { return !inR.empty(); }
};

// A SOLO wave's vpdt as the XT lays it out: events at (stride·e + 8)·2 (stride 10, 16 stereo), each {u32 flags, u32 position, u32 segment
// entry, u32 record, u16 mark}; segment entries {u32 record, u32 flags, u32 start}; the count of record words; then the records {u16 length,
// u16 info}, 0xffff continuing a length, ending with (256, 1).  Read into its records and the plan's events and segments.  False if it isn't
// one, or its records don't reach its last event.
bool xtSoloReadVpdt(const std::vector<uint8_t>& vpdt, int channels, std::vector<XtRecord>& recs, XtSoloPlan& P);

class XtSoloVoice {
public:
    // A = pitch ratio, F = formant ratio (clamped to [0.5, 2] by the caller),
    // walk = playback speed (player +0x7c: 1 as recorded, 0 holds, < 0 plays
    // backward from the end), energy = SOLO Energy g (0 = OFF).
    void noteOn(const XtSoloPlan& plan, double A, double F, double walk, double energy);
    // Pitch, formant and time may change while the note plays.
    void setRates(double A, double F, double walk);
    void setEnergy(double g) { energy = (float)g; }
    // The output gain (player +0x80 target, +0x84 current), the firmware's: each sample
    // the gain moves 1/64 toward the target, then scales the output.  1 is unity for
    // render's ±1; the firmware's int16 scale has 1/65536.  resetGain sets both (note-on).
    void setGain(float target) { gainTarget = target; }
    void resetGain(float g) { gainTarget = gain = g; }
    // Robot Voice (OSC flag 0x10, player byte 0 bit 0x10; set before noteOn):
    // A is then the pitch against A440 and voiced grains come at that fixed
    // pitch (0x8950d5a0).
    void setRobot(bool on) { robot = on; }
    // Loop playback (set before noteOn; off by default).  The firmware keeps
    // the loop points as four cached events (player +0x6c: forward start,
    // forward end, backward end, backward start); the player recomputes its
    // next crossing (+0x2c) each block (0x8950ccc0) and acts on it in the
    // walk (0x8950c880 forward, 0x8950ca70 backward): One Way jumps to the
    // other end (0x8950d7d0), Alternate turns the walk round.
    void setLoop(const XtLoop& l) { loopCfg = l; }
    // The Start Offset in samples, as the converter leaves it in the block (+0xb8, at most the last sample).  Set before noteOn.  The voice
    // keeps it × 16 at +0x18 (0x894f3170): a note walking forward starts there (0x8950fa60, then 0x8950fed0), and a walk backward stops
    // there, unless the forward loop has turned it round (0x8950cf9c, 0x8950d048).
    void setStart(int32_t samples) { startCfg = samples; eventCfg = false; }
    // Playback Mode STEP or EVENT instead (0x894f3150 with the block's 0x80 clear): the start and end events, held to the last.  The note
    // starts at the first (the last walking backward) by the event's jump (0x8950d790), and its walk stops at both (+0x18, +0x1c).
    // No loop: the converter's tails clear the Loop bit, though the cached loop events stay.  Set before noteOn.
    void setEvents(int first, int last) { evFirstCfg = first; evLastCfg = last; eventCfg = true; }
    // An update's Loop bit, the block's (0x894f3fb6), as the band player's: only where the note-on cached the loop.
    void setLoopBit(bool on) { loopOn = on && cachedLoop; }
    // Tempo Sync and Time Trip (XtBeat).  Set before noteOn: the note's tempo and the wave's
    // Original Tempo (+0x4c, +0x48), byte 0 bits 1 and 2.  Then, from the voice: the bits as a
    // block sets them (0x894f3ec0: a clock counts from when the bit comes on, and Time Trip
    // starts with one step), a clock, a Time Trip step.
    void setTempo(uint32_t tempo, uint32_t orig) { tempoCfg = tempo; origCfg = orig; }
    void setBeatFlags(bool sync, bool trip) {
        if (sync && !(flags0 & 2)) beat_.clk = 0;
        if (trip && !(flags0 & 4)) beat_.trip = 1;
        flags0 = (uint8_t)((sync ? 2 : 0) | (trip ? 4 : 0));
    }
    void addClock(int n) { beat_.clk = (int8_t)(beat_.clk + n); }
    void setTrip(int step) { beat_.trip = (int8_t)step; }
    bool syncOn() const { return flags0 & 2; }
    bool tripOn() const { return flags0 & 4; }
    const XtBeat& beat() const { return beat_; }
    // To a position in 1/16 sample (0x8950fed0 with byte 0 bit 7), as a released Time Trip goes
    // back to the beat: the record there and the anchor at its start, the running grains and the
    // trigger timer carrying on, the beat again from there.
    void seek(int32_t pos16);
    // The output gain as it stands (+0x84), and the position (+0x38, 1/16 sample).
    float gainNow() const { return gain; }
    int32_t position() const { return Pi; }
    // Renders `count` samples: mono (a stereo plan's left channel), or both
    // channels (a mono plan plays L = R, as the firmware writes it), scaled to ±1.
    void render(float* out, int count) { render(out, nullptr, count); }
    void render(float* left, float* right, int count);
    // The same as the firmware's output words, L/R pairs: int16 × 65536 × the gain.  Bit for bit.
    void renderXt(float* lr, int count);
    bool ended() const { return ended_ != 0; }
    // Since note-on, a segment past the vpdt's list taken (0x8950cef0 on the last segment, only after starting at the last event): the
    // XT then reads its records from RAM.  Silent from then here.
    bool readPastWave() const { return pastWave; }
    int segmentMoves() const { return segMoves; }   // since note-on, the walk's moves between segments (for the check's counts)
    int kindTurns() const { return kindTurn; }      // and its turns at a kind-1 event (0x8950cb60)
    int terminatorGrains() const { return termGrain; }   // its grains on the stream's terminator (at the wave's end), all its notes'
    int endFlag() const { return ended_; }  // +4 as it stands: 1 at the end, −1 at the start, else 0
    FILE* trace = nullptr;              // VSE_TRACE: per-sample state, as EmuSolo.java writes it

private:
    // A grain's fields as the firmware keeps them: start and tap in 1/16 samples, age in
    // 1/4096 samples, the rest single precision.
    struct Grain { int32_t start; float v, frac; int32_t C; };
    struct Chan { int32_t start = 0, age = 0, tap = 0; float v = 1, e = 1, tapW = 0; };
    const XtSoloPlan* p = nullptr;
    float A = 1, F = 1, walk = 1, energy = 0;
    float f9c = 1, rate98 = 1;          // +0x9c the loop's rate factor; +0x98 what the grains and the filter take, the pitch × that, each block
    Chan ch[2];
    int next = 1;
    float slope = 0.0f;
    // The phrase position in the firmware's 12.20 fixed point (player +0x38
    // in 1/16 samples, +0x3c its 20-bit fraction), stepped per sample by
    // half the block's walk step (+0x34 = trunc(walk·2^25), 0x8950c280).
    int32_t Pi = 0, Pf = 0, walkStep = 0;
    double Pq = 0.0;
    int32_t timer = 0, ppw = 0;         // grain +0xc, +4: 1/4096 samples
    int ended_ = 0;                     // +4: 1 at the end, −1 at the start
    // The current record (+0xe8 block: +0x20 start, +0x24 end, +8 the repeat
    // budget), as 0x8950d4c0 loads the next and 0x8950d530 the previous.
    // Index size() is the stream's (256, 1) terminator.
    size_t seg = 0;
    double recStart = 0.0, recEnd = 0.0;
    double anchor = 0.0, anchorStep = 1.0, budget = 0.0;
    float low[2] = {}, band[2] = {}, fCoef = 0, q = 1.2f;   // SVF state per channel
    float gainTarget = 1.0f, gain = 1.0f;
    bool robot = false;                 // byte 0's bit 0x10, which a block's flags reach during the note (0x894f3fa8): the rate's (0x8950cde0)
    bool robotEv = false;               // the grains' (+0x124 = 0): taken from it only when an event loads (0x8950c7e0)
    // Loops: the cached events' positions (1/16 sample: forward start,
    // forward end, backward end, backward start) and the modes at the
    // forward end and the backward start; the walk flags (player +3: 0x04 /
    // 0x08 turned round in the backward / forward loop, 0x10 / 0x20 inside
    // it, 0x40 holding); the wave's two events (0 at the start, 1 at the end:
    // +0xc current, +0x24 its position, +0x28 the next's) and the next
    // crossing (+0x2c).
    XtLoop loopCfg;
    XtBeat beat_;
    uint32_t tempoCfg = 1200000, origCfg = 1200000;
    uint8_t flags0 = 0;                 // byte 0 bits 1, 2 (Tempo Sync, Time Trip)
    int32_t lp[4] = { 0, 0x7fffffff, 0x7fffffff, 0 };
    int lmode1 = 0, lmode3 = 0;
    bool loopOn = false;                // byte 0 bit 0, as the crossings read it
    bool cachedLoop = false;            // the note-on cached the loop's events
    uint8_t f3 = 0x80;
    int curEv = 0;
    int32_t prevEv = 0, nextEv = 0, nextCross = 0, endPos16 = 0;
    int32_t startCfg = 0, startPos16 = 0;   // +0x18: the Start Offset × 16, or the start event's position
    int32_t endStop16 = 0;              // +0x1c: the end event's position (the wave's end but under STEP and EVENT)
    int32_t lastRead = 0;               // +0x5c of the record block: the last sample a read takes
    int32_t nextLen16 = 0;              // +0x30: the next record's length, 1/16 sample, as a load leaves it
    int segCur = 0;                     // the segment the walk is in (+0xe8 block: +0xc its start, +0x10 the next's, +0x14 the one after it)
    bool pastWave = false;
    int segMoves = 0;
    int kindTurn = 0;
    int termGrain = 0;
    bool eventCfg = false;
    int evFirstCfg = 0, evLastCfg = 0;
    float f0n = 0.0f;
    double anchorStepInit = 1.0;
    long t = 0;

    double lenOf(size_t i) const;
    double budgetOf(size_t i) const;
    void loadNext();
    void loadPrev();
    void loaded();
    double repeatCost() const;
    void anchorForward();
    void anchorBackward();
    void crossings();
    void crossForward();
    void crossBackward();
    void jumpTo(int32_t pos16);
    int evLast() const;                 // the wave's last event (its end)
    int32_t evAt(int e) const;          // an event's position, 1/16 sample
    int evSegAt(int e) const;           // the segment containing it
    int evKindAt(int e) const;          // its kind (flags & 0xb), 0 on a plan without them
    int segLast() const;
    int32_t segAt(int k) const;         // a segment's start, 1/16 sample
    size_t segRecAt(int k) const;       // its first record
    int16_t segFirstAt(int k) const;    // that record's first length word
    int eventBefore(int32_t pos16) const;   // the last event before the position, or 0 (0x8950f9c0 from event 0)
    int segContaining(int32_t pos16) const;
    void eventAt(int e);                // the event current (0x8950c7e0), and the next crossing at the event after it
    void eventJump(int e);              // to an event (0x8950d790): it current, the position and anchor there, its segment and record
    void loopJump(int32_t pos16);       // to a cached loop event (0x8950d7d0)
    void seekTo(int32_t pos16);         // 0x8950fed0 with byte 0 bit 7: the event before, then segments and records to the position
    void segForward();                  // the walk into the next segment (0x8950cef0)
    void segBackward(int32_t r1);       // and back out of one (0x8950cfde)
    Grain grainParams(bool setF0);
    void walkBlock();
    void filterBlock();
};
