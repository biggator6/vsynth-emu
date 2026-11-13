// XtSynth's zones and zone 1's controls: a key's zone, each zone's controls, and new controls to the sounding voices.
// Moved from XtSynth.cpp as they stood (v193).
#include "XtSynth.h"
#include <algorithm>
#include <iterator>

int XtSynth::xtZoneOf(const int (&upper)[kZones], int key) {
    int top = 0;
    for (int z = 0; z < kZones; ++z) {
        top = z == kZones - 1 ? 127 : std::max(top, upper[z]);
        if (!(key > top)) return z;
    }
    return 0;
}

XtZoneKeys xtZoneKeys(const int (&upper)[kXtZones], int zone) {
    int top = -1, lo = 0;
    for (int z = 0; z <= zone && z < kXtZones; ++z) {
        lo = top + 1;
        top = z == kXtZones - 1 ? 127 : std::max(top, upper[z]);
    }
    return { lo, top };
}

// A zone's own blocks (IDs 0x293…0x3e6 and the COSMs': the tone block, the envelopes, the LFOs), the rest zone 1's: the patch's
// common (tuning, tempo, key assign, portamento, bend, the Matrix) and the system's, which aren't a zone's.  A zone with no
// patch's data is the XT's init zone, both OSC Switches off [FW 0x897a167c: IDs 0x2a4, 0x2d8 default 0], so silent (v195).
XtSynth::Controls XtSynth::zoneControls(int z) const {
    if (!z) return ctl;
    Controls c = zones ? zones->ctl[z] : ctl;
    if (!zones) c.on = c.osc2.on = false;
    c.pitchSemis = ctl.pitchSemis; c.patchCoarse = ctl.patchCoarse; c.patchFine = ctl.patchFine;
    c.scaleTune = ctl.scaleTune; std::copy(std::begin(ctl.scale), std::end(ctl.scale), c.scale);
    c.bendUp = ctl.bendUp; c.bendDown = ctl.bendDown;
    c.patchTempo = ctl.patchTempo; c.clockSource = ctl.clockSource;
    c.portamento = ctl.portamento; c.portaMode = ctl.portaMode; c.portaType = ctl.portaType; c.portaTime = ctl.portaTime; c.portaVeloSens = ctl.portaVeloSens;
    c.mono = ctl.mono; c.legato = ctl.legato; c.octave = ctl.octave;
    std::copy(std::begin(ctl.matrix), std::end(ctl.matrix), c.matrix);
    std::copy(std::begin(ctl.assign), std::end(ctl.assign), c.assign);
    c.outGainTg = ctl.outGainTg; c.parallel = ctl.parallel;
    return c;
}

void XtSynth::zonesFollow() {
    for (int z = 1; z < kZones; ++z) zoneCtl[z] = zoneControls(z);
}

// New knob values reach a playing voice at the next update (mixBlock),
// through the voice update's smoothing, as on the XT.  Any change is an update of every note of the part (0x894d7760), whatever its zone,
// and each reads its own zone's settings again: a voice on another zone sees only what of zone 1's it reads, the patch's common.
void XtSynth::setControls(const Controls& c) {
    const bool anyChanged = !(c == ctl);
    // what each sounding zone had: a voice's update compares its own zone's settings
    bool sounding[kZones] = {};
    if (anyChanged)
        for (const Voice& v : voice)
            if (v.active && !sounding[v.zone]) { sounding[v.zone] = true; zoneWas[v.zone] = zc(v.zone); }
    ctl = c;
    clockFollow();
    if (!anyChanged) {
        msmSync();                                   // a patch the same as the controls before still stops it
        return;
    }
    zonesFollow();
    // the MOD's converter, zone 1's: the other zones' are the patch's, formed when they came (pickUpSample).  What one type
    // leaves the next keeps, and the same settings give the same block
    xtModBlock(ctl.mod, modBlk[0]);
    // the Multi Step Modulator's section (XtSynthMsm.cpp), then the knobs' update of every voice
    msmSync();
    for (Voice& v : voice)
        if (v.active) liveUpdate(v, zc(v), zoneWas[v.zone], anyChanged);
}
