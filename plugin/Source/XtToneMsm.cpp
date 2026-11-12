// xt_tone's MSM mode (v193): `xt_tone <rom> msm [cases] [seed]`.  The firmware's own Multi Step Modulator runs in the
// interpreter beside the firmware's tone (XtToneFwMsm.h), and XtSynth's XtMsm beside XtSynth: each its own engine, on the
// same settings, notes and tempo ticks.  So every converter's term, the tone updates it asks for and the note-on head are
// each side's own, and a term taken in the wrong place shows in the sound.  Compared each block: the whole output bus,
// word by word, and the two objects' running flag and tracks.  A case is a fresh firmware and a fresh XtSynth (no voice
// used before, so nothing to carry between them), analog tones (no waves), POLY, Clock Source INTERNAL.  XT_TONE_FIRST=1
// says what differs.  HARDWARE_FIRMWARE.md, "The Multi Step Modulator in XtSynth".
#ifdef XT_TONE_MAIN
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "XtToneCases.h"
#include "XtToneMsmCheck.h"

namespace {
constexpr int kNotes = FwMsm::kNotes;

XtMsmSettings drawMsm(Rng& r) {
    XtMsmSettings s;
    s.master = !r.one(8) && !xtSwitchIs("XT_TONE_MSM_OFF", "1");
    s.keySync = r.range(0, 2);
    for (XtMsmTrackSettings& t : s.track) {
        t.on = !r.one(6);
        t.loop = !r.one(3);
        t.type = r.range(0, 1);
        t.grid = r.one(2) ? 5 : r.range(0, 5);
        t.endPoint = r.range(0, 15);
        t.direction = r.range(0, 5);
        t.zone = r.one(6) ? r.range(1, 15) : 0;
        t.dest = r.range(0, kXtMsmDests - 1);
        t.dest = xtSwitchInt("XT_TONE_MSM_DEST", t.dest);   // every track on one destination, to find one at fault
        for (int& v : t.steps) {
            v = r.one(4) ? 0 : r.range(-63, 63);
        }
    }
    return s;
}


// One case: its settings on both sides, its notes, an edit of the section now and then, and its blocks
struct Case {
    int g = 0;
    Rng r { 0 };
    Ctl ctl;
    std::unique_ptr<XtSynth> syn = std::make_unique<XtSynth>();
    Firmware fw;
    FwNote fn[kNotes];
    FwMsm m { fw, fn };
    int raw[kNotes] = {};
    int vel[kNotes] = {};
    int on[kNotes] = {};
    int off[kNotes] = {};
    int blocks = 0;
    int editAt = -1;
    int holdAt = -1;
    int holdUp = -1;
    int holdDown = 0;
    int holdLet = 0;
    bool over = false;
    bool rendered = false;

    // The settings: a patch's on both sides (msmPatch, 0x894d9950).  False: nothing to play, or (ok false) the firmware failed
    bool setUp(const char* rom, bool& ok) {
        ctl = drawControls(r, g, false);
        ctl.clockSource = 0;
        ctl.msm = drawMsm(r);
        syn->prepare(44100.0);
        syn->msmPatch();
        syn->setControls(ctl);
        ok = true;
        if (!syn->canPlay()) return false;
        fw.par = withMsmIds(patchOf(ctl), ctl.msm);
        fw.cosmBuf = kRam + 0x1000000;                  // the COSMs' buffers, as xt_tone's cases have them
        ok = fw.open(rom) && m.begin();
        if (!ok) return false;
        XtToneMsmProbe::controllers(*syn, fw);
        ok = fw.updatePatch() && m.settings(true);
        return ok;
    }
    void draw() {
        blocks = r.range(600, 1600);
        editAt = r.one(3) ? r.range(0, blocks) : -1;
        for (int i = 0; i < kNotes; ++i) {
            raw[i] = r.range(36, 84);
            // two keys: a key-off ends every note on its key (0x894d4e60), the firmware's side here only its own
            if (i == 1 && raw[1] == raw[0]) raw[1] = raw[0] < 84 ? raw[0] + 1 : raw[0] - 1;
            vel[i] = r.range(1, 127);
            on[i] = i == 0 ? r.range(0, 40) : (r.one(3) ? -1 : r.range(0, blocks / 2));
            off[i] = on[i] < 0 ? -1 : on[i] + r.range(20, blocks);
            if (xtSwitchIs("XT_TONE_MSM_HOLD", "1")) off[i] = -1;    // every key held, to find a difference that isn't a release's
        }
        drawHold();
        if (xtSwitchIs("XT_TONE_MSM_ONE", "1")) on[1] = off[1] = -1;   // the first note alone, its buffers the last
    }
    // Hold in a case in four, down before the first key lets go, then up or not: a key let go under it leaves its note on
    // the list with its key up, which Key Sync PART's head reads (0x894d5550).  In half of them the second note comes then.
    void drawHold() {
        if (off[0] <= 0 || !r.one(4)) return;
        holdAt = r.range(0, off[0] - 1);
        if (on[1] >= 0 && r.one(2)) {
            on[1] = off[0] + r.range(1, 40);
            off[1] = on[1] + r.range(20, blocks);
        }
        holdUp = r.one(4) ? -1 : std::max(off[0], on[1]) + r.range(1, blocks / 2);
        holdDown = r.one(2) ? 127 : r.range(64, 127);
        holdLet = r.one(2) ? 0 : r.range(0, 63);
    }
    // An edit of the section: the firmware's (0x894d9b20), then the knob's update of the notes; setControls on XtSynth's
    // A value moves: the XT sends no edit that leaves one as it was, and XtSynth's setControls takes none as an update
    bool edit() {
        XtMsmTrackSettings& track = ctl.msm.track[r.range(0, 3)];
        const int dest = r.range(0, kXtMsmDests - 1);
        track.dest = dest == track.dest ? (dest + 1) % kXtMsmDests : dest;
        if (r.one(4)) ctl.msm.master = 0;
        fw.par = withMsmIds(patchOf(ctl), ctl.msm);
        if (!fw.updatePatch() || !m.settings(false)) return false;
        for (FwNote& n : fn)
            if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
        syn->setControls(ctl);
        return true;
    }
    // A note-on: the key state, the allocator's head, the note (0x894dbcb0's way for POLY); and XtSynth's
    bool noteOn(int i, long& notes) {
        const FwNote& o = fn[1 - i];
        const FwNote* prev = o.live && !o.off && !o.ended ? &o : nullptr;
        const int key = std::clamp(raw[i] + 12 * ctl.octave, 0, 127);
        if (!fw.keyOn(raw[i], vel[i]) || !m.head()) return false;
        int index = 0;
        const XtVoice* pv = XtToneMsmProbe::next(*syn, index);
        const auto carry = [&](FwNote& n) { xtToneCarry(fw, n, pv, index, ctl); };
        if (!fw.start(fn[i], i, key, 0, carry, prev)) return false;
        fn[i].vi = index;
        syn->noteOn(raw[i], (float)vel[i] / 127.0f);
        ++notes;
        return true;
    }
    bool noteOff(int i) {
        int back = -1;
        if (!fw.keyOff(raw[i], 64, back)) return false;
        if (fn[i].live && !fn[i].released && !fw.keyRelease(fn[i])) return false;
        syn->noteOff(raw[i], 64.0f / 127.0f);
        return true;
    }
    bool events(int blk, long& notes) {
        fw.frames = XtToneMsmProbe::frames(*syn);
        if (blk == editAt && !edit()) return false;
        if (blk == holdAt && !controller(64, holdDown)) return false;
        if (blk == holdUp && !controller(64, holdLet)) return false;
        const bool cc7 = xtSwitchIs("XT_TONE_MSM_CC7", "1");
        for (int i = 0; i < kNotes; ++i) {
            if (blk == on[i] && !noteOn(i, notes)) return false;
            if (blk == off[i] && !noteOff(i)) return false;
            if (cc7 && off[i] >= 0 && blk == off[i] + 10 && !controller(7, 40)) return false;
        }
        return true;
    }
    // After a block, a note over on one side only differs, and ends the case; one over on both is stopped, as the XT's
    // tone stops itself (0x894f5a70), so that the next note finds no tone of it to follow (xt_tone's way)
    bool ends(Tally& t) {
        for (FwNote& n : fn) {
            if (!n.live) continue;
            const bool fwDone = n.ended || fw.c.rd32(Firmware::tva(n) + 0x44) != 0;
            const bool synDone = n.vi < 0 || !XtToneMsmProbe::voice(*syn, n.vi).active;
            t.same("voice end", fwDone, synDone);
            over = over || fwDone != synDone;
            if (!fwDone) continue;
            n.ended = true;
            n.vi = -1;
            if (!fw.stop(n)) return false;
        }
        return true;
    }
    // A controller moved: the channel takes it, Hold's handler if it is Hold (a pedal let go goes over the notes), the
    // pad's, each note's update (xt_tone's pedalMove); then XtSynth's.  Hold, and CC 7 by a research switch
    bool controller(int number, int value) {
        fw.setCc(number, value);
        if (number == 64 && !fw.pedal(number, value, fn, kNotes)) return false;
        if (!fw.padCc(number, value)) return false;
        for (FwNote& n : fn)
            if (n.live && !n.ended && !fw.liveUpdate(n)) return false;
        syn->controller(number, value);
        return true;
    }
    // The firmware's block in XtSynth's order (mixBlock): the tempo clock, the ms with the MSM's, the update, the render
    bool fwBlock() {
        const long at = XtToneMsmProbe::frames(*syn);
        fw.frames = at;
        for (int k = XtToneMsmProbe::clocksDue(*syn); k > 0; --k)
            if (!fw.clock((uint32_t)(at / 32))) return false;
        int32_t d[4] = {};
        const int ms = XtToneMsmProbe::msDue(*syn);
        XtToneMsmProbe::ticks(*syn, ms, d);
        for (int i = 0; i < ms; ++i)
            if (!m.ms(d[i])) return false;
        const bool tick = XtToneMsmProbe::tick(*syn);
        if (tick && !fw.tickFree()) return false;
        fw.clearBus();
        // in XtSynth's voice order, so that the last rendered is the same on both sides (xt_tone's way)
        const bool swap = fn[0].vi >= 0 && fn[1].vi >= 0 && fn[1].vi < fn[0].vi;
        rendered = false;
        for (int i = 0; i < kNotes; ++i) {
            FwNote& n = fn[swap ? 1 - i : i];
            if (!n.live || n.ended) continue;
            if (tick && !fw.tick(n)) return false;
            n.ended = fw.c.rd32(n.TN + 0xc) != 2;
            if (!n.ended && !fw.render(n)) return false;
            rendered = rendered || !n.ended;
        }
        return true;
    }
};

// With XT_TONE_FIRST, a case's settings where it first differs: Key Sync, each track's switch, zone, destination and Grid
void sayCase(const Case& c) {
    std::printf("  case %d: master %d key sync %d; notes at %d, %d, off at %d, %d; edit at %d; Hold at %d, up at %d; "
                "ended %d, %d\n", c.g, c.ctl.msm.master, c.ctl.msm.keySync, c.on[0], c.on[1], c.off[0], c.off[1],
                c.editAt, c.holdAt, c.holdUp, c.fn[0].ended, c.fn[1].ended);
    for (const XtMsmTrackSettings& s : c.ctl.msm.track)
        std::printf("    track on %d zone %d dest %d grid %d type %d\n", s.on, s.zone, s.dest, s.grid, s.type);
    std::printf("    structure %d; COSM 1 on %d type %d, COSM 2 on %d type %d\n", c.ctl.structure, c.ctl.cosm[0].on,
                c.ctl.cosm[0].type, c.ctl.cosm[1].on, c.ctl.cosm[1].type);
    for (int i = 0; i < XtSynth::kVoices; ++i) {
        const XtVoice& v = XtToneMsmProbe::voice(*c.syn, i);
        if (v.active)
            std::printf("    XtSynth's voice %d: key %d, TVA envelope stage %d at %.9g, gain %.9g\n", i, v.note,
                        v.env.stage(), v.env.level(), v.cur[0]);
    }
    for (const FwNote& n : c.fn)
        std::printf("    the firmware's note on voice %d: live %d ended %d\n", n.vi, n.live, n.ended);
}

bool runCase(const char* rom, int g, uint32_t seed, Tally& t) {
    auto c = std::make_unique<Case>();
    c->g = g;
    c->r = Rng { seed + 7919u * (uint32_t)g };
    bool ok = true;
    if (!c->setUp(rom, ok)) return ok;
    c->draw();
    ++t.cases;
    float L[32];
    float R[32];
    const long before = t.different;
    bool told = false;
    for (int blk = 0; blk < c->blocks; ++blk) {
        if (!c->events(blk, t.notes) || !c->fwBlock()) return false;
        c->syn->render(L, R, 32);
        ++t.blocks;
        t.g = g;
        t.blk = blk;
        if (!c->ends(t)) return false;
        if (c->over) break;
        for (const FwNote& n : c->fn)
            if (n.live && !n.ended && n.vi >= 0) compareNote(t, c->fw, n, *c->syn);
        if (c->rendered) XtToneMsmProbe::buffers(t, c->fw, *c->syn);
        compare(t, c->fw, *c->syn);
        if (!t.first || told || t.different == before) continue;
        told = true;
        sayCase(*c);
    }
    t.updates += c->m.updates;
    return true;
}
}  // namespace

bool xtToneMsmMode(int argc, char** argv) {
    return argc > 2 && std::strcmp(argv[2], "msm") == 0;
}

int xtToneMsm(int argc, char** argv) {
    if (!xtSynthSetRom(argv[1])) {
        std::fprintf(stderr, "usage: xt_tone rom.2.bin msm [cases] [seed]\n");
        return 2;
    }
    const long cases = argc > 3 ? std::strtol(argv[3], nullptr, 10) : 60;
    const uint32_t seed = argc > 4 ? (uint32_t)std::strtoul(argv[4], nullptr, 10) : 2024u;
    Tally t;
    const bool stop = xtSwitch("XT_TONE_STOP") != nullptr;   // ends at the first case that differs (mutate_tone.py)
    for (int g = 0; g < (int)cases && !(stop && t.different > 0); ++g) {
        if (runCase(argv[1], g, seed, t)) continue;
        std::printf("tone, msm: case %d: the firmware failed\n", g);
        ++t.different;
    }
    std::printf("tone, msm: %ld cases, %ld notes, %ld blocks, %ld of the MSM's tone updates, %ld checks\n", t.cases,
                t.notes, t.blocks, t.updates, t.checks);
    std::printf("tone, msm: %ld different\n", t.different);
    return t.different ? 1 : 0;
}
#endif
