#!/usr/bin/env python3
"""vc2writer.py: the VC-2 runs' writer, for vc2capture.py.  Vc2Run is capsysex's Run writing rom.4's parameters by name
through vc2map's map, each take on its patch from the card (vc2cardrun.py): when what SysEx can't reach differs from
the last take's, the take selects its patch by Bank Select and Program Change, waits for its load, and sends by DT1 only
what differs from what the patch holds.  What neither reaches (the system's Output Gain and External In Type: no patch
holds them, and rom.4 has no address for them) the user sets at the panel, in a pause the run makes for it at a "hand:"
cue that fxplay shows; the run's timing and its takes stay as they were.  Also a take in the panel's words, a run built
from its takes, and the test."""
import vc2probe
import vc2takes
from capsysex import Run, nibbles
from vc2cardrun import BANK_MSB, LOAD_MS, program_of
from vc2map import MODEL, TYPES4

NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B']
EFFECT_SWITCHES = ('MFX Switch', 'Chorus Switch', 'Reverb Switch')
RECEIVES = ('Receive Program Change', 'Receive Bank Select')   # [FW] rom.4's system block, 02 00 00 08 and 09
HAND_MS = 20000     # the pause for what only the panel reaches, set by hand while fxplay shows the run's "hand:" cue
# Where VC-2 mode's panel has each of those (HARDWARE_FIRMWARE.md, "The VC-2's system settings at its panel"): [FW]
# rom.4's "SYSTEM Com IO" screen draws Output Gain (0x894a0c84), as rom.2's same screen does (the XT's manual, pp. 123
# and 156: SYSTEM, the Common group, the IO tab); rom.4 draws External In Type on no screen
PANEL = {'Output Gain': 'SYSTEM > Common > IO > Output Gain',
         'External In Type': 'External In Type (rom.4 draws it on no VC-2 page: RECORDING_GUIDE.md, step 3)'}
TEST_SETS = ('Direct Sound Level', 'OSC 1 Switch', 'MFX Switch', 'Master Level', 'MFX Type')   # test_run sets


def note_name(k):
    return '%s%d' % (NOTE_NAMES[k % 12], k // 12 - 1)


def probe_seconds(probe):
    return {'full': vc2probe.END, 'fx': vc2probe.FX_END}.get(probe, 5.0)


class Vc2Run(Run):
    """A VC-2 run: rom.4's parameters by name through the map, each take on its patch (vc2cardrun.Patches)."""
    def __init__(self, dev, ch, m, patches):
        super().__init__(dev, ch, MODEL)
        self.map, self.patches, self.patch = m, patches, None
        self.hand = {k: vc2takes.BASE[k] for k in patches.by_hand}   # as RECORDING_GUIDE.md has them set first

    def by_hand(self, want):
        """What only the panel reaches (the system's Output Gain and External In Type), when it differs from what the
        run last had: a "hand:" cue saying it, which fxplay shows, then HAND_MS for the user to set it."""
        hand = {k: want[k] for k in self.patches.by_hand}
        if hand == self.hand:
            return
        words = ['%s: %s' % (PANEL.get(k, k), self.map.text(self.map.id_named(k), v))
                 for k, v in hand.items() if self.hand[k] != v]
        self.cue('hand:' + '; '.join(words))
        self.wait(HAND_MS)
        self.hand = hand

    def select(self, n):
        """Patch n: the system's receive switches on, Bank Select MSB 87 and its LSB, its Program Change, its load.  The
        edit then holds what the patch holds; the system's values stay as they were."""
        for k in RECEIVES:
            self.put(k, 1)
        lsb, pc = program_of(n)
        self.control(0, BANK_MSB)
        self.control(32, lsb)
        self.program(pc)
        self.wait(LOAD_MS)
        self.patch = n
        self.state.update(self.patches.holds(n))

    def put(self, name, v):
        """One value by DT1 when it differs; none for what the patch carries or only the panel reaches."""
        if name in self.patches.only or name in self.patches.by_hand:
            return
        if self.state.get(name) != v:
            self.sx(self.map.address(self.map.id_of(name)), [v])
            self.state[name] = v

    def apply(self, want):
        """What only the panel reaches, by hand in a pause; its patch when that changes; then what differs: the effects'
        switches off first, so nothing sounds through a change; a type with each of its typed defaults; the rest; the
        switches on last.  Returns whether a patch or an effect's program loaded."""
        self.by_hand(want)
        n = self.patches.number(want)
        reload = n != self.patch
        if reload:
            self.select(n)
        for k in EFFECT_SWITCHES:
            if want[k] == 0:
                self.put(k, 0)
        for k in TYPES4:
            if self.state.get(k) != want[k]:
                self.put(k, want[k])
                reload = True
                for address, v in self.map.typed(k, want[k]):
                    self.sx(address, nibbles(v))
        for k, v in want.items():
            if k not in EFFECT_SWITCHES and k not in TYPES4:
                self.put(k, v)
        for k in EFFECT_SWITCHES:
            if want[k] == 1:
                self.put(k, 1)
        return reload

    def apply_take(self, t):
        """A take's settings over the base; with none, the base alone (the run's set-up)."""
        return self.apply({**vc2takes.BASE, **(t['set'] if t else {})})


def settings(m, t, patches):
    """A take in the panel's words: its patch, what it sets over the base (what only the panel reaches marked so), for
    doing it by hand."""
    p = ['patch %03d' % patches.number({**vc2takes.BASE, **t['set']})]
    for k, v in t['set'].items():
        value = m.text(m.id_named(k), v)
        p.append('by hand, %s: %s' % (PANEL.get(k, k), value) if k in patches.by_hand else '%s %s' % (k, value))
    p.append('play nothing' if t['key'] is None else 'hold %s' % note_name(t['key']))
    if t['probe'] is None:
        p.append('the input silent')
    if t.get('marker'):
        p.append('Direct Sound Level 127 for the input\'s first %.1f s (its clicks and sweep), then as set, and only '
                 'then the key' % vc2takes.MARKER_S)
    return '; '.join(p)


def build(run, takes, w, text):
    """A run on writer w (its apply_take: a take's writes, None the set-up): its take rows and cues [(ms, probe)]."""
    rows, cues = [], []
    w.wait(3000)
    w.apply_take(None)
    w.wait(2000)
    for t in takes:
        reload = w.apply_take(t)
        if t.get('marker'):
            w.put('Direct Sound Level', 127)
        w.wait(2500 if reload else 800)                     # a program's load, a mute ramp, a reverb's clear
        start = w.ms
        if t['probe']:
            w.cue('probe:%.1f' % probe_seconds(t['probe']))     # fxplay plays the input's first that many seconds
            cues.append((start, t['probe']))
        if t.get('marker'):             # the clicks through the direct sound, then the take's own level
            w.wait(vc2takes.MARKER_S * 1000)
            w.put('Direct Sound Level', {**vc2takes.BASE, **t['set']}['Direct Sound Level'])
        if t['key'] is not None:
            w.note(True, t['key'])
        w.wait(start + probe_seconds(t['probe']) * 1000 + 200 - w.ms)
        if t['key'] is not None:
            w.note(False, t['key'])
        off = w.ms
        w.wait(t['tail'] * 1000)
        rows.append(dict(run=run, take=t['name'], note_on_s='%.3f' % (start / 1000), note_off_s='%.3f' % (off / 1000),
                         next_s='%.3f' % (w.ms / 1000), probe=t['probe'] or '', settings=text(t), why=t['why']))
    w.wait(1000)
    return rows, cues


def test_run(m, dev, ch, patches):
    """The test: its patch by Program Change; the input as the direct sound and as the EXT IN tone on C4, so it is
    heard whichever word the mic is; Master Level down in its sweep; MFX Type to Parametric EQ and back."""
    w = Vc2Run(dev, ch, m, patches)
    heard = {**vc2takes.BASE, 'Direct Sound Level': 127, 'OSC 1 Switch': 1, 'MFX Switch': 1}
    w.wait(1000)
    w.apply(heard)
    w.wait(1500)
    w.cue('probe:%.1f' % vc2probe.FX_END)
    w.note(True, 60)
    w.wait(8000)
    w.put('Master Level', 40)
    w.wait(1000)
    w.apply({**heard, 'Master Level': 40, 'MFX Type': 1})
    w.wait(8000)
    w.note(False, 60)
    w.put('Master Level', 127)
    w.apply(vc2takes.BASE)
    w.wait(200)
    return w
