#pragma once
// The V-Synth XT's Multi Step Modulator (v192): the firmware's object at 0x89cf7634 (constructor 0x894d9820), four tracks of
// 16 steps on a timer of the OS's tempo ticks (480 a quarter, 0x8930eb00) that fires every 20, so 24 clocks a quarter, the
// first 20 ticks after it starts.  A destination's converter asks it for its term (0x894d96d0) and adds that to its knob; a
// track that moves asks for a tone update.  It serves part 0 only.  HARDWARE_FIRMWARE.md, "The Multi Step Modulator".
// Real-time: no allocation, no locks.  Checked against the firmware's own object by XT_MSM_MAIN (XtMsmCheck.cpp).
#include <cstdint>
#include <vector>

// A track's settings as the loaders read them (IDs 0x48f + 24·track on; XtMsm.cpp's read), each as the getter gives it:
// the stored value less the descriptor's centre
struct XtMsmTrackSettings {
    int on = 1;            // Track n Switch
    int loop = 1;          // Track Loop Switch: taken only under Key Sync; with it OFF a track always loops
    int type = 0;          // STEP, SMOOTH
    int grid = 3;          // QUARTER, 8TH, 8TH-T, 16TH, 16TH-T, 32TH: 24, 12, 8, 6, 4, 3 clocks a step (0x897c4a30)
    int endPoint = 15;     // 0…15, steps 1…16
    int direction = 0;     // FWD1, FWD2, FWD3, BWD1, BWD2, BWD3
    int zone = 0;          // 0…15
    int dest = 0;          // 0…71, the value string's list (OSC1+2-PIT-CRS … MFX-PRM3)
    int steps[16] = {};    // −63…+63
    bool operator==(const XtMsmTrackSettings&) const = default;
};

struct XtMsmSettings {
    static constexpr int kTracks = 4;
    int master = 0;        // 0x48d Master Switch
    int keySync = 1;       // 0x48e OFF, PART, VOICE
    XtMsmTrackSettings track[kTracks];
    bool operator==(const XtMsmSettings&) const = default;
};

// The IDs the loaders read: 0x48d Master Switch, 0x48e Key Sync, then 24 a track from 0x48f: Switch, Loop, Type, Grid,
// End Point, Direction, Zone, Destination, Step 1…16 (0x4ef, after them, is a dummy).  The setting an ID is, nullptr for
// none; xtMsmValue its value, 0 for none.  The destinations are 72.
constexpr int kXtMsmFirstId = 0x48d, kXtMsmLastId = 0x4ee, kXtMsmTrackIds = 24, kXtMsmDests = 72;
int* xtMsmField(XtMsmSettings& s, int id);
int xtMsmValue(XtMsmSettings s, int id);

// The destinations by number: the patch's three, OSC 1's from 3 and OSC 2's from 17 (an OscDest on), COSM 1's from 31 and
// COSM 2's from 44 (a CosmDest on), the TVA's from 57 (a TvaDest on), the sends and MFX-PRM1…3
namespace xtmsm {
enum Dest : int {
    PitCrs = 0, PitFine = 1, PortaTime = 2, Osc1 = 3, Osc2 = 17, Cosm1 = 31, Cosm2 = 44, Tva = 57,
    MfxSend = 66, ChoSend = 67, RevSend = 68, MfxPrm = 69,
};
enum OscDest : int {
    OscCrs, OscFine, OscPitch, OscPenv, OscTime, OscTenv, OscFormant, OscFenv,
    OscLfoRate, OscLfoPitch, OscLfoTime, OscLfoFormant, OscLfoLevel, OscLevel,
};
enum CosmDest : int {
    CosmPrm1, CosmPrm2, CosmEnv1, CosmEnv2 = CosmEnv1 + 4, CosmLfoRate = CosmEnv2 + 4, CosmLfoPrm1, CosmLfoPrm2,
};
enum TvaDest : int { TvaLevel, TvaPan, TvaEnv, TvaLfoRate = TvaEnv + 4, TvaLfoLevel, TvaLfoPan };
enum EnvStage : int { Attack, Decay, Sustain, Release };
}
// The settings a patch's store holds (XtMsmPatch.cpp; its reads need xtFwOpen, and aren't for the audio thread)
XtMsmSettings xtMsmSettingsOf(const std::vector<uint8_t>& store);

// A track as the firmware lays it out: 30 bytes at the object's +0x2c + 30·n (constructor 0x894da2e0)
struct XtMsmTrack {
    uint8_t state = 0;     // +0: 0 stopped, 1 past its end without a loop (it holds its last step), 2 running
    int8_t cur = 0;        // +1: the step
    int8_t next = 0;       // +2: the step after, which SMOOTH goes toward
    uint8_t end = 15;      // +3: End Point
    int8_t delta = 0;      // +4: ±1, the way FWD2, FWD3, BWD2 and BWD3 go now
    uint8_t on = 0;        // +5: Track Switch
    uint8_t loop = 0;      // +6: Loop
    uint8_t grid = 0;      // +7
    uint8_t dir = 0;       // +8: Direction
    uint8_t type = 0;      // +9: STEP, SMOOTH
    uint8_t zone = 0;      // +0xa
    uint8_t dest = 0;      // +0xb
    int8_t steps[16] = {}; // +0xc
    int8_t out = 0;        // +0x1c: its term
    int8_t count = 24;     // +0x1d: clocks left in the step (the constructor's, by the Grid in .bss, 0)

    int8_t stepAt(int i) const { return steps[i & 15]; }   // the firmware's +0xc + i; i is 0…15 throughout
    bool stopped() const { return state == 0; }

    // XtMsmTrack.cpp
    void start();          // 0x894da320: at its first step, step 1 forward or the End Point backward
    void halt();           // 0x894da3e0: stopped, its term 0
    bool clock();          // 0x894da400: a clock; true when its term was made again
};
static_assert(sizeof(XtMsmTrack) == 30, "the firmware's track is 30 bytes");

// The tables the tracks read from rom.2 (XtMsmTrack.cpp): the coarse tunes' curve and the Grid's clocks
bool xtMsmLoadTables(const char* romPath);
bool xtMsmTablesLoaded();

class XtMsm {
public:
    static constexpr int kTracks = 4, kTicksPerClock = 20;
    static constexpr int32_t kNever = 0x7fffffff, kFiring = 0x7ffffffe;   // the OS timer's +8: not set, fired

    void reset();                                       // the constructor (0x894d9820) and 0x894d9580: the tempo ticks at 0
    // The settings, of part `part` (only 0 is taken): a patch (0x894d9950) stops it first; an edit of its section
    // (0x894d9b20) stops it only when the Master Switch goes off
    void load(const XtMsmSettings& s, int part);
    void edit(const XtMsmSettings& s, int part);
    // A note-on of `part` (the allocator's head, 0x894d4b08…0x894d4bd2): starts or restarts it by Key Sync.  keyHeld: a
    // note of part 0 whose key is still down besides it (0x894d5550, before the new note is taken)
    void noteOn(int part, bool keyHeld);
    // The OS's tempo timer moved on by `ticks` (0x8930eb00): the object's timer fires at the first count at or past its
    // time, at most once a call, counts a clock and sets itself 20 ticks on from its time (0x894d9510)
    void tick(int32_t ticks);
    // The tone task's flag 0x20 (0x894d4240): every clock counted since, then a tone update if a track moved.  sounding: a
    // note of part 0 that hasn't ended (0x894d55a0); under Key Sync with none, and no arpeggio, it stops instead
    void clocks(bool sounding);
    // The arpeggio's flag (+0x25): set as its steps play (0x894d9920), cleared when it halts (0x894d9930)
    void arpeggio(bool playing) { arp = playing ? 1 : 0; }

    // The terms: the sum of the tracks on `dest` that are on, while it runs, for part 0
    int term(int dest, int part, int zone) const;       // 0x894d96d0, of the tracks on `zone`: the converters'
    int termAnyZone(int dest, int part) const;          // 0x894d9750 → 0x894d97b0, any zone, as a 16-bit value
    int termAll(int dest) const;                        // 0x894d97b0: any zone (the effects' sends and MFX-PRM1…3)
    int shownStep(int track) const;                     // 0x894d9870: the step the screen shows, −1 stopped

    // The object's fields (offsets in the firmware's), public for its check
    int32_t due = kNever;      // the OS timer's +8: when it fires
    int32_t fired = 0;         // +0xc: the time it last fired at
    int32_t pos = 0;           // +0x1c: the time it was last set for
    int32_t pending = 0;       // +0x20: clocks counted and not yet run
    uint8_t running = 0;       // +0x24
    uint8_t arp = 0;           // +0x25
    uint8_t master = 0;        // +0x26
    uint8_t keySync = 0;       // +0x27
    uint8_t loops[kTracks] = {};   // +0x28: each track's Loop Switch as read
    XtMsmTrack track[kTracks];     // +0x2c
    int32_t now = 0;           // the OS's tempo ticks (0x898586f0)
    int updates = 0;           // the tone updates asked for (0x894d96b0 → 0x894d72f0)
    // Each tone update at once, where the firmware's runs: inside start, stop and clocks, so that one stopped and started
    // again gives its notes an update with no terms, then one with the first steps'.  reset keeps it.
    void (*onUpdate)(void*) = nullptr;
    void* updateTo = nullptr;

    void start();              // 0x894d95a0
    void stop();               // 0x894d9600

private:
    void read(const XtMsmSettings& s);                  // the loaders' common part, 0x894d9988… and 0x894d9b4c…
    bool clockAll();                                    // 0x894d9660
    void toneUpdate();                                  // 0x894d96b0
};

// 0x894d98a0: a step's value as semitones, for the coarse tunes (destinations 0, 3, 17): ±63 to ±24, ⌊|v|·24/63⌋; past
// ±63 (SMOOTH after an edit of the Grid) the bytes after the curve, as the firmware reads them
int xtMsmCoarse(int v);
