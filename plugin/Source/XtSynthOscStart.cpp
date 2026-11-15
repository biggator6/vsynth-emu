// An OSC's start on a voice: whether it plays, its LFO and its player's note-on (moved from XtSynth.cpp as they stood,
// v200).
#include "XtSynth.h"
#include "XtFloat.h"

// Audio thread (it reads the samples the last render picked up).
bool XtSynth::oscPlays(int zone, int osc) const {
    const OscControls& o = osc ? zc(zone).osc2 : zc(zone);
    if (!o.on) return false;
    if (o.oscType == 0) return xtAnalogTablesLoaded() && xtAnalogPlays(o.ana.wave);
    if (o.oscType == 2) return true;                 // EXT IN plays the input, there or not (v200)
    return waveOf(zone, osc) != nullptr;
}

bool XtSynth::canPlay(int zone) const { return oscPlays(zone, 0) || oscPlays(zone, 1); }

// An OSC's LFO at note-on, whatever its Switch: every LFO of the tone starts
// (0x894f5e60), and a key-unsynced one sets its free-running phase's rate.
void XtSynth::oscLfoNoteOn(const Voice& v, OscVoice& s, const OscControls& o, XtLfoFree& free, const XtLfo* other, float seed) {
    if (o.lfoKeySync) s.lfo.noteOn(lfoParams(v, s, o), seed, v.blockTempo);
    else if (other) s.lfo.noteOnFollow(lfoParams(v, s, o), *other);
    else s.lfo.noteOnFree(lfoParams(v, s, o), seed, free, v.blockTempo);
}

// An OSC's voice at note-on.  The OSC gain starts from the amp envelope's
// start level, or its output when there is no attack (0x894f3e20), and goes
// to the envelope's output (0x894f3dc0).
void XtSynth::oscStart(Voice& v, OscVoice& s, const OscControls& o) {
    const XtEnvParams oe = ampEnvParams(v, o);
    const float gainStart = gainTarget(v, s, o, oe.time[0] == 0.0f ? s.oenv.level() : oe.level[0]);   // +0x158 (0x894f3e20)
    s.curGain = gainTarget(v, s, o, s.oenv.level());                                                  // +0x154 (0x894f3dc0)
    s.chase = false;                                                                                  // +0x9b0 (0x894f21e6)
    if (s.analog) {   // the analog note-on (0x89504af0): Fat on the formant envelope, the Pulse Width on the time envelope
        XtAnalogBlock b = analogBlock(v, s, o);
        if (&s == &v.osc2 && modBlk[v.zone].type == 4 && b.code == 10) b.code = 2;   // the tone's note-on (0x894f6148): under OSC SYNC OSC 2 plays LA-SAW as SAW
        s.ana.noteOn(b, s.curA, s.curGain, gainStart, s.fenv.level(), s.tenv.level(), s.lfo.out(), anaShared);
    }
    else if (s.extIn) s.ext.noteOn(gainStart, s.curGain);   // kind 4's note-on (0x894f2ff0): the gain, its start and target
    else pcmStart(v, s, o, gainStart);
}

// A PCM OSC's player at note-on, its gain from gainStart to the target (split from oscStart, v200)
void XtSynth::pcmStart(Voice& v, OscVoice& s, const OscControls& o, float gainStart) {
    // the player's tempo (0x894f3110): this OSC's block +0x60 × 10000, against the wave's, and
    // the bits as the block has them, the pad's if it is held
    const uint32_t tempoWord = (uint32_t)xtf::ftrc(xtf::mul(tempoNow(), 10000.0f));
    const bool tripNow = o.timeTrip && pad.on();
    if (s.linear) {
        s.lin.setLoop(waveLoop(s, o));
        s.lin.setStart(waveStart(s, o));
        s.liteStart = waveStart(s, o);
        s.lin.setLite(s.lite);
        s.lin.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.lin.setBeatFlags(o.tempoSync, tripNow);
        s.lin.noteOn(s.wave->solo.in, s.wave->solo.inR, s.curA, s.curWalk);
        s.lin.setGainXt(gainStart, true);
        s.lin.setGainXt(s.curGain);
    }
    else if (s.lite) {                                // Vari on (0x8950b8d0)
        s.xfg.setLoop(waveLoop(s, o));
        s.xfg.setStart(waveStart(s, o));
        s.liteStart = waveStart(s, o);
        s.xfg.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.xfg.setBeatFlags(o.tempoSync, tripNow);
        s.xfg.noteOn(s.wave->solo.in, s.wave->solo.inR, s.curA, s.curF, s.curWalk, s.root);
        s.xfg.setGainXt(gainStart, true);
        s.xfg.setGainXt(s.curGain);
    }
    else if (s.wave->type == 1) {
        int first = 0, last = 0;
        s.solo.setRobot(s.robot);
        s.solo.setLoop(waveLoop(s, o));
        if (waveEvents(v, s, o, first, last)) s.solo.setEvents(first, last);
        else s.solo.setStart(waveStart(s, o));
        s.solo.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.solo.setBeatFlags(o.tempoSync, tripNow);
        s.solo.noteOn(s.wave->solo, s.curA, s.curF, s.curWalk, s.curG);
        s.solo.resetGain(gainStart);
        s.solo.setGain(s.curGain);
    }
    else {
        // STEP and EVENT play between two events with no loop: their tails clear the block's flags 0x01 (the Loop), 0x80 and 0x08,
        // which the player keeps apart from the loop's cached events (setEvents)
        int first = 0, last = 0;
        s.mb.setLoop(waveLoop(s, o));
        if (waveEvents(v, s, o, first, last)) s.mb.setEvents(first, last);
        else s.mb.setStart(waveStart(s, o));
        s.mb.setVari(o.vari);
        s.mb.setTempo(tempoWord, (uint32_t)o.origTempo);
        s.mb.setBeatFlags(o.tempoSync, tripNow);
        if (s.mb.noteOn(s.wave->wave, s.wave->type, s.curA, s.curWalk)) {
            s.mb.setGainXt(gainStart, true);
            s.mb.setGainXt(s.curGain);
        } else {
            s.on = false;
            if (!v.on && !v.osc2.on) v.active = false;
        }
    }
}
