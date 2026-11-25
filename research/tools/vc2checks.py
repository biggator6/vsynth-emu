#!/usr/bin/env python3
"""vc2checks.py: vc2cut.py's checks of each VC-2 run, by its takes' names (vc2takes.py, vc2vsynth.py): whether each
setting reached the sound, and what it measured, the way fxcut.py's check_run1 goes through the effects' run 1.  A
finding the firmware can't give (which side the direct sound carries, say) is said as it is, never failed; a FAIL is
only for a recording that can't answer (nothing came back, the writes didn't land).  `Vc2Checks` is a part of
vc2cut.py's analysis."""
import math

import vc2probe
from capbase import STEP_DB, LOW_STEPS

SYSTEM_FIX = 'The System writes aren\'t reaching the XT: tell Claude, as the runs depend on them.'
PATCH_FIX = 'The patch\'s writes aren\'t landing: tell Claude, and do vc2_test.mid\'s checks again.'
VOICE = (vc2probe.SIDES[0], vc2probe.LADDER[1])
SIDE_WORDS = {'MONO-L': 'the input\'s left alone', 'MONO-R': 'the input\'s right alone',
              'MONO-L+R': 'the input\'s left and right, mixed', 'STEREO': 'each side on its own side',
              'swapped': 'the sides crossed', 'none': 'neither side'}
VS_WANT = {'main': 'STEREO', 'vs_monolr': 'MONO-L+R', 'vs_monol': 'MONO-L', 'vs_monor': 'MONO-R'}
DIRECT_TAKES = ('main', 'main_again', 'direct_64', 'direct_16', 'master_100')
SETTINGS_2 = [('legacy', 'main'), ('hireso_vocosc', 'main'), ('legacy_vocosc', 'legacy'),
              ('hireso_fshift_0', 'main'), ('hireso_fshift_127', 'main'), ('legacy_fshift_0', 'legacy'),
              ('legacy_fshift_127', 'legacy'), ('hireso_tmpl_5', 'main'), ('hireso_tmpl_15', 'main'),
              ('hireso_depth_64', 'main'), ('hireso_level_64', 'main'), ('hireso_uvlevel_0', 'main'),
              ('hireso_uvlevel_127', 'main'), ('hireso_uvdetect_1', 'main'), ('hireso_uvdetect_50', 'main'),
              ('hireso_env_0', 'main'), ('hireso_env_100', 'main'), ('hireso_mono', 'main'),
              ('legacy_mono', 'legacy'), ('hireso_spread', 'main'), ('hireso_direct_64', 'main'),
              ('hireso_key48', 'main')]


def hz(v):
    return '-' if v is None else '%.1f' % v


class Vc2Checks:
    def sounding(self, name):
        t = self.by.get(name)
        return t is not None and t.present and not t.cut_short and name not in self.silent

    def check_run(self):
        {'vc2_run_1': self.check_input, 'vc2_run_2': self.check_vocoder, 'vc2_run_3': self.check_mfx,
         'vc2_run_4': self.check_returns, 'vc2_vsynth': self.check_vsynth}[self.run]()

    def sides_of(self, name, want=None):
        """A take's sides said; with `want`, held to that pattern.  Returns the pattern it matches."""
        t = self.by.get(name)
        m = self.sides(t) if t is not None and self.sounding(name) else None
        if m is None:
            self.add('INFO', '`%s`: no clean mark, so its sides can\'t be read.' % name)
            return None
        got = self.pattern(m)
        words = SIDE_WORDS.get(got, 'no pattern of the four')
        if want is None:
            self.add('INFO', '`%s` carries %s: %s.' % (name, words, self.said(m)))
        elif got == want:
            self.add('OK', '`%s` carries %s, as v204\'s mapping says for %s: %s.' % (name, words, want, self.said(m)))
        else:
            self.add('LOOK', '`%s` carries %s, where v204\'s mapping says %s: %s. Tell Claude.'
                     % (name, words, SIDE_WORDS.get(want, want), self.said(m)))
        return got

    def check_vsynth(self):
        """V-Synth mode: the EXT IN OSC at each External In Type, held to v204's mapping of the returned words."""
        if not any(self.sounding(n) for n in VS_WANT):
            self.add('FAIL', 'The EXT IN OSC is silent in V-Synth mode at every External In Type. Check the input, '
                     'and that the XT was in V-Synth mode for this run.')
            return
        for name, want in VS_WANT.items():
            self.sides_of(name, want)

    def check_input(self):
        """Run 1: the input's level and side, the direct sound's law and flag, the EXT IN tone, Auto Note."""
        held = [n for n in DIRECT_TAKES if n in self.by]           # a recording made --from may hold none of them
        direct = [n for n in DIRECT_TAKES if self.sounding(n)]
        extin = [n for n in ('extin_stereo', 'extin_monolr', 'extin_monol', 'extin_monor') if self.sounding(n)]
        if not direct and not extin:
            self.add('FAIL', 'Nothing of the input came back: no direct sound and no EXT IN tone. Check the cable into '
                     'the XT, its INPUT (or MIC) Jack Switch, and that fxplay was given --audio.')
            return
        if held and not direct:
            self.add('LOOK', 'The direct sound is silent at Direct Sound Level 127 while the EXT IN tone hears the '
                     'input: the word the part mixer reads as the mic is one this input doesn\'t reach. That answers '
                     'the slot question with "none of these": tell Claude.')
        elif direct:
            self.direct_law()
        for name, what in (('direct_0', 'Direct Sound Level 0'), ('direct_pcm', 'OSC 1 Type PCM')):
            if self.sounding(name):
                self.add('LOOK', '%s: `%s` still has the input in it.' % (what, name))
            elif name in self.by:
                self.add('OK', '%s: `%s` is silent, as the part mixer reads it.' % (what, name))
        if 'direct_osc2' in self.by:
            self.add('INFO', 'OSC 2 Type EXTIN with OSC 1 PCM: the direct sound %s.' % (
                'sounds, so OSC 2\'s Type sets the flag too' if self.sounding('direct_osc2')
                else 'is silent, so the flag reads OSC 1\'s Type'))
        self.tail_of('direct_rev', 'the direct sound\'s Reverb send')
        for name in ('extin_stereo', 'extin_monolr', 'extin_monol', 'extin_monor'):
            self.sides_of(name)
        self.pitches()

    def direct_law(self):
        """The direct sound's sides by its first take, and its levels against a take at Direct Sound Level 127: a
        recording made --from past `main` and `main_again` has none, and each other direct take moves its own level."""
        self.sides_of(next(n for n in DIRECT_TAKES if self.sounding(n)))
        ref = next((self.by[n] for n in ('main', 'main_again') if self.sounding(n)), None)
        if ref is None:
            self.add('INFO', 'No `main` or `main_again` here (a recording made --from?): the direct sound\'s levels '
                     'have no take at 127 to be read against.')
            return
        if ref.lvl() is not None:
            self.add('INFO', 'The direct sound at 127 is %+.2f dB against the input, on its 1 kHz steps at %d and %d '
                     'dBFS.' % (ref.lvl(), LOW_STEPS[0], LOW_STEPS[1]))
            bent = next((STEP_DB[k] for k in range(13) if ref.law[k] is not None and STEP_DB[k] > LOW_STEPS[-1]
                         and abs(ref.law[k] - ref.lvl()) > 0.5), None)
            if bent is not None:
                self.add('INFO', 'Its 1 kHz steps bend from %+d dBFS of the input: the input clips, or limits, '
                         'there.' % bent)
        for name, value in (('direct_64', 64), ('direct_16', 16)):
            self.check_setting(name, ref.name, 20.0 * math.log10(value / 127.0), 'Direct Sound Level %d' % value,
                               PATCH_FIX)
        self.check_setting('master_100', ref.name, None, 'Master Level 100', SYSTEM_FIX)
        self.check_setting('gain_plus6', ref.name, 6.0, 'Output Gain +6', SYSTEM_FIX)

    def tail_of(self, name, what):
        t = self.by.get(name)
        if t is None or not self.sounding(name):
            return
        floor = getattr(self, 'floor_db', self.env_floor)
        if t.after_noise > floor + 10.0:
            self.add('OK', '%s: `%s` has a tail at %.1f dBFS after the input\'s noise.' % (what, name, t.after_noise))
        else:
            self.add('LOOK', '%s: `%s` has no tail after the input\'s noise (%.1f dBFS, floor %.1f).'
                     % (what, name, t.after_noise, floor))

    def pitches(self):
        """The EXT IN tone's and Auto Note's fundamentals on the input's steps and sides, and the key's move."""
        want = ', '.join('%.1f' % f for f in vc2probe.STEPS_F0)
        for name in ('extin_stereo', 'autonote', 'autonote_retrig'):
            if self.sounding(name):
                t = self.by[name]
                self.add('INFO', '`%s` on the input\'s steps (%s Hz): %s Hz; on its sides (220 Hz left, 330 right): '
                         'out L %s, out R %s Hz.' % (name, want, ', '.join(hz(f) for f in self.steps_f0(t)),
                                                     hz(self.f0_of(t, vc2probe.SIDES, 0)),
                                                     hz(self.f0_of(t, vc2probe.SIDES, 1))))
        self.key_moved('extin_key67', 'extin_stereo', 'The EXT IN tone on G4 against C4')

    def key_moved(self, name, ref, what):
        """How far a take's fundamental is from ref's on the input's vowel, in semitones: what a key moved."""
        if self.sounding(name) and self.sounding(ref):
            a = self.f0_of(self.by[name], vc2probe.VOWEL_A)
            b = self.f0_of(self.by[ref], vc2probe.VOWEL_A)
            if a and b:
                self.add('INFO', '%s: %+.2f semitones.' % (what, 12 * math.log2(a / b)))
