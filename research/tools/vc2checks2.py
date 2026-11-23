#!/usr/bin/env python3
"""vc2checks2.py: vc2cut.py's checks of VC-2 runs 2 to 4 (vc2checks.py has run 1's and V-Synth mode's): the carrier and
the vocoder's settings, rom.4's MFX and program A's EQs, the Chorus and Reverb returns.  A vocoder setting is held to
the take it changes, against the floor two takes of the same settings leave (`main` against `main_again`): a setting
whose spectrum and level stay inside it reached nothing.  `Vc2Checks2` is a part of vc2cut.py's analysis."""
import vc2probe
from caprec import db, rms
from vc2checks import SETTINGS_2, VOICE, hz

CARRIERS = ('carrier_analog', 'carrier_vocosc', 'carrier_vocosc_quiet')
WITH_DIRECT = {'hireso_direct_64'}                       # its direct sound plays on through the voice: no test of it
EQ_HZ = [50.0 * 2 ** (k / 6.0) for k in range(55)]     # 50 Hz to 27 kHz, a sixth of an octave a step


class Vc2Checks2:
    def voiced(self, name):
        """Whether a take sounds over the input's voice part: in run 2 the direct sound's marker is over by then."""
        t = self.by.get(name)
        if t is None or not t.present or t.cut_short:
            return False
        floor = getattr(self, 'floor_db', self.env_floor)
        return db(rms(self.section(t, VOICE).ravel())) > max(floor + 20.0, -90.0)

    def check_vocoder(self):
        """Run 2: the carrier, then each vocoder setting against the take it changes."""
        for name in CARRIERS:
            if name in self.by and not self.voiced(name):
                self.add('FAIL', 'The carrier is silent in `%s`: OSC 2 or the key didn\'t land. Tell Claude.' % name)
        if self.voiced('carrier_analog'):
            self.add('INFO', 'The analog carrier sounds at %s Hz (C4 is 261.6).'
                     % hz(self.f0_of(self.by['carrier_analog'], vc2probe.VOWEL_A)))
        if self.voiced('carrier_vocosc') and self.voiced('carrier_vocosc_quiet'):
            d = self.by['carrier_vocosc'].loudest - self.by['carrier_vocosc_quiet'].loudest
            self.add('INFO', 'The VOCODER OSC is %+.2f dB with the input playing against without: its Dynamics.' % d)
        voc = [t.name for t in self.takes if t.note and t.name not in CARRIERS and t.name not in WITH_DIRECT]
        if not any(self.voiced(n) for n in voc):
            self.add('FAIL', 'Every vocoder take is silent: the vocoder hears no modulator, or no carrier. '
                     'Tell Claude.')
            return
        quiet = [n for n in voc if n in self.by and not self.voiced(n)]
        if quiet:
            self.add('LOOK', 'Silent vocoder takes: %s.' % ', '.join(quiet))
        floor = None
        if self.sounding('main') and self.sounding('main_again'):
            floor, _ = self.distance(self.by['main'], self.by['main_again'], VOICE)
            self.add('INFO', '`main_again` against `main`: %.2f dB apart in their spectra, the floor for the rest.'
                     % floor)
        for name, base in SETTINGS_2:
            self.setting_moved(name, base, max(2.0 * (floor or 0.0), 0.2))
        self.formants()
        for name in ('main', 'legacy'):
            if self.sounding(name):
                ms = self.onset(self.by[name], vc2probe.SIDES[0])
                self.add('INFO', '`%s` starts %s ms after the input\'s first vowel.' % (name, hz(ms)))

    def setting_moved(self, name, base, floor):
        if not (self.sounding(name) and self.sounding(base)):
            return
        d, level = self.distance(self.by[name], self.by[base], VOICE)
        if d is None:
            return
        if d <= floor and abs(level) < 0.2:
            self.add('LOOK', '`%s` changed nothing measurable against `%s` (%.2f dB apart, %+.2f dB in level).'
                     % (name, base, d, level))
        else:
            self.add('INFO', '`%s` against `%s`: %.2f dB apart in the spectrum, %+.2f dB in level.'
                     % (name, base, d, level))

    def formants(self):
        names = [n for n in ('hireso_fshift_0', 'main', 'hireso_fshift_127') if self.sounding(n)]
        if len(names) == 3:
            c = [hz(self.centroid(self.by[n], vc2probe.VOWEL_A)) for n in names]
            self.add('INFO', 'Formant Shift 0, 64, 127: the spectrum\'s centre at %s, %s, %s Hz.' % tuple(c))
        self.key_moved('hireso_key48', 'main', 'HIRESO on C3 against C4')

    def eq_moved(self, name, ref, what, should=True):
        """The largest of a take's response against ref, and where; a LOOK when it `should` move and doesn't."""
        if not (self.sounding(name) and self.sounding(ref)):
            return
        v, _, _ = self.response(self.by[name], self.by[ref], EQ_HZ)
        pts = [(abs(x), x, f) for x, f in zip(v, EQ_HZ) if x is not None]
        if not pts:
            return
        _, top, at = max(pts)
        if abs(top) < 0.5:
            self.add('LOOK' if should else 'INFO', '%s: `%s` is within 0.5 dB of `%s` everywhere.' % (what, name, ref))
        else:
            self.add('INFO', '%s: `%s` against `%s` is %+.2f dB at its most, at %.0f Hz.' % (what, name, ref, top, at))

    def check_mfx(self):
        """Run 3: the feed reaches the MFX; program A's EQs; each MFX type differs from the one before."""
        if not self.sounding('main'):
            self.add('FAIL', 'The feed is silent: the EXT IN tone through MFX Thru reaches nothing, so no MFX can be '
                     'heard. Tell Claude: run 3 wants another feed.')
            return
        d = self.delta('feed_off', 'main')
        if d is not None:
            self.add('INFO', 'MFX Switch off is %+.2f dB against Thru.' % d)
        self.eq_moved('sys_eq_off', 'main', 'The system EQ off (flat at rom.4\'s defaults, so likely no change)',
                      should=False)
        self.eq_moved('peq1_plus15', 'main', 'EQ 1 Gain +15')
        self.eq_moved('peq2_plus15', 'main', 'EQ 2 Gain +15')
        d = self.delta('peq1_direct', 'direct')
        if d is not None:
            ok = abs(d) < 0.1
            self.add('OK' if ok else 'LOOK', 'EQ 1 Gain +15 on the direct sound (MAIN): %+.2f dB against `direct`%s.'
                     % (d, ', so the patch EQ is MULTI\'s alone' if ok else ''))
        self.check_differs({t.name for t in self.takes if t.name.startswith('mfx')})

    def check_returns(self):
        """Run 4: each Chorus and Reverb type differs from the one before, and each Reverb has a tail."""
        self.check_differs({t.name for t in self.takes if t.name.startswith(('cho', 'rev'))})
        tails = ['%s %.1f' % (t.name, t.after_noise) for t in self.takes if t.name.startswith('rev') and t.present]
        if tails:
            self.add('INFO', 'The Reverbs\' tails after the input\'s noise, dBFS: %s.' % ', '.join(tails))
