#!/usr/bin/env python3
"""vc2takes.py: the VC-2 captures' takes (research/VC2_MODE_PLAN.md, step 5, says what each run settles;
RECORDING_GUIDE.md, "VC-2 captures", how to record them).  A take is what it sets, by rom.4's own parameter names
(vc2capture.py finds each name's ID in rom.4's table), the key it holds, and how much of vc2probe's signal goes into
the input.

Values are as the patch stores them, the descriptor's raw value: Output Gain 16 + dB, a type by its number.  What a
take leaves out is BASE's, over PATCH Init's.  The four runs are VC-2 mode's; vc2vsynth.py asks rom.2 the same
question in V-Synth mode (HARDWARE_FIRMWARE.md, "rom.4's outside input")."""

ANALOG, PCM, EXTIN, VOCODER = 0, 1, 2, 3          # OSC Type, rom.4's value list
STEREO, MONO_LR, MONO_L, MONO_R = 0, 1, 2, 3      # External In Type
LEGACY, HIRESO = 0, 1                             # Vocoder Type

BASE = {
    # the patch: no MFX, Chorus or Reverb, the vocoder off, no direct sound
    'MFX Switch': 0, 'Chorus Switch': 0, 'Reverb Switch': 0, 'Portamento Switch': 0,
    'Vocoder Sw': 0, 'Vocoder Type': HIRESO, 'Direct Sound Level': 0,
    'Auto Note On Switch': 0, 'Auto Note Retrigger': 0, 'Formant Shift': 64, 'Formant Template': 0,
    'Vocoder Depth': 127, 'Vocoder Sound Level': 127, 'Unvoice Level': 40, 'Unvoice Detect': 25,
    'Env Attack': 8, 'Env Release': 18, 'Stereo Process Sw': 1, 'Unvoice Spread switch': 0,
    'EQ 1 Gain': 15, 'EQ 2 Gain': 15,
    'MFX Type': 0, 'MFX Master Level': 127, 'MFX Chorus Send Level': 0, 'MFX Reverb Send Level': 0,
    'Chorus Type': 0, 'Chorus Master Level': 127, 'Chorus Reverb Send Level': 0,
    'Reverb Type': 0, 'Reverb Master Level': 127,
    # zone 1's tone: OSC 1 EXTIN and off (the direct sound's flag reads its Type alone), OSC 2 off, the TVA to MULTI
    'MOD Switch': 0, 'TVA Switch': 0, 'Output Assign': 0,
    'MFX Send Level': 127, 'Chorus Send Level': 0, 'Reverb Send Level': 0,
    'OSC 1 Switch': 0, 'OSC 1 Type': EXTIN, 'OSC 1 Wave Gain': 16, 'OSC 1 Vari Sw': 0, 'OSC 1 Time Trip Sw': 0,
    'OSC 2 Switch': 0, 'OSC 2 Type': ANALOG, 'OSC 2 Vari Sw': 0, 'OSC 2 Time Trip Sw': 0,
    # the system
    'Master Level': 127, 'Output Gain': 16, 'External In Type': MONO_LR, 'EQ Switch': 1,
}
SYSTEM = ('Master Level', 'Output Gain', 'External In Type', 'EQ Switch')


def take(name, why, sets=None, key=60, probe='full', tail=1.0):
    """probe: 'full' (vc2probe's 32.5 s), 'fx' (its first 16.8 s, the effects' part) or None (the input silent)."""
    return dict(name=name, why=why, set=dict(sets or {}), key=key, probe=probe, tail=tail)


def run1():
    """The input: its level, which side reaches the direct sound and the EXT IN tone, Auto Note."""
    d = {'Direct Sound Level': 127}
    x = {'OSC 1 Switch': 1}
    a = {**x, 'Auto Note On Switch': 1}
    return [
        take('silence', 'the floor: no key, the input silent', key=None, probe=None),
        take('main', 'the direct sound at 127: the input word to MAIN at 0.025, and which input side it is', d),
        take('main_again', 'the same again: does a take repeat?', d),
        take('direct_64', 'Direct Sound Level 64: -5.95 dB, eased 3% a block', {'Direct Sound Level': 64}),
        take('direct_16', 'Direct Sound Level 16: -18.0 dB', {'Direct Sound Level': 16}),
        take('direct_0', 'Direct Sound Level 0: nothing', {'Direct Sound Level': 0}),
        take('direct_pcm', 'OSC 1 Type PCM: the EXTIN flag clear, the direct sound the voices\' buffer, silent',
             {**d, 'OSC 1 Type': PCM}),
        take('direct_osc2', 'OSC 1 PCM, OSC 2 Type EXTIN: does OSC 2\'s Type set the flag too?',
             {**d, 'OSC 1 Type': PCM, 'OSC 2 Type': EXTIN}),
        take('direct_rev', 'Reverb Send 127, Stereo Room: the direct sound\'s send, Reverb Send x MAIN\'s level',
             {**d, 'Reverb Send Level': 127, 'Reverb Switch': 1, 'Reverb Type': 5}, tail=4.0),
        take('master_100', 'Master Level 100: a System write lands', {**d, 'Master Level': 100}),
        take('gain_plus6', 'Output Gain +6 dB', {**d, 'Output Gain': 22}),
        take('extin_stereo', 'the EXT IN tone, External In Type STEREO: the input as a voice, and its sides',
             {**x, 'External In Type': STEREO}),
        take('extin_monolr', 'External In Type MONO-L+R', {**x, 'External In Type': MONO_LR}),
        take('extin_monol', 'External In Type MONO-L', {**x, 'External In Type': MONO_L}),
        take('extin_monor', 'External In Type MONO-R', {**x, 'External In Type': MONO_R}),
        take('extin_key67', 'the EXT IN tone on G4: does the key move it (kind 6\'s P)?', x, key=67),
        take('autonote', 'Auto Note on: the tone\'s pitch by the tracker, and which side it hears', a),
        take('autonote_retrig', 'Auto Note Retrigger on', {**a, 'Auto Note Retrigger': 1}),
    ]


def run2():
    """The vocoder and the carrier: OSC 1 EXTIN off (the modulator, the input word), OSC 2 the carrier."""
    c = {'OSC 2 Switch': 1}
    vc = {**c, 'OSC 2 Type': VOCODER}
    h = {**c, 'Vocoder Sw': 1, 'Vocoder Type': HIRESO}
    g = {**c, 'Vocoder Sw': 1, 'Vocoder Type': LEGACY}
    return [
        take('silence', 'the floor', key=None, probe=None),
        take('carrier_analog', 'the carrier alone: OSC 2 ANALOG at its defaults, the vocoder off', c),
        take('carrier_vocosc', 'the VOCODER OSC alone at its defaults', vc),
        take('carrier_vocosc_quiet', 'the same, the input silent: its Dynamics read the input\'s peak', vc, probe=None),
        take('main', 'HIRESO at its defaults on the analog carrier', h),
        take('main_again', 'the same again', h),
        take('legacy', 'LEGACY at its defaults', g),
        take('hireso_vocosc', 'HIRESO on the VOCODER OSC', {**h, 'OSC 2 Type': VOCODER}),
        take('legacy_vocosc', 'LEGACY on the VOCODER OSC', {**g, 'OSC 2 Type': VOCODER}),
        take('hireso_fshift_0', 'Formant Shift 0: an octave down', {**h, 'Formant Shift': 0}),
        take('hireso_fshift_127', 'Formant Shift 127: an octave up', {**h, 'Formant Shift': 127}),
        take('legacy_fshift_0', 'LEGACY, Formant Shift 0', {**g, 'Formant Shift': 0}),
        take('legacy_fshift_127', 'LEGACY, Formant Shift 127', {**g, 'Formant Shift': 127}),
        take('hireso_tmpl_5', 'Formant Template 5', {**h, 'Formant Template': 5}),
        take('hireso_tmpl_15', 'Formant Template 15', {**h, 'Formant Template': 15}),
        take('hireso_depth_64', 'Vocoder Depth 64', {**h, 'Vocoder Depth': 64}),
        take('hireso_level_64', 'Vocoder Sound Level 64', {**h, 'Vocoder Sound Level': 64}),
        take('hireso_uvlevel_0', 'Unvoice Level 0: no consonants added', {**h, 'Unvoice Level': 0}),
        take('hireso_uvlevel_127', 'Unvoice Level 127', {**h, 'Unvoice Level': 127}),
        take('hireso_uvdetect_1', 'Unvoice Detect 1', {**h, 'Unvoice Detect': 1}),
        take('hireso_uvdetect_50', 'Unvoice Detect 50', {**h, 'Unvoice Detect': 50}),
        take('hireso_env_0', 'Env Attack and Release 0', {**h, 'Env Attack': 0, 'Env Release': 0}),
        take('hireso_env_100', 'Env Attack and Release 100', {**h, 'Env Attack': 100, 'Env Release': 100}),
        take('hireso_mono', 'Stereo Process Sw off', {**h, 'Stereo Process Sw': 0}),
        take('legacy_mono', 'LEGACY, Stereo Process Sw off', {**g, 'Stereo Process Sw': 0}),
        take('hireso_spread', 'Unvoice Spread on', {**h, 'Unvoice Spread switch': 1}),
        take('hireso_direct_64', 'HIRESO with Direct Sound 64: the mix', {**h, 'Direct Sound Level': 64}),
        take('hireso_key48', 'HIRESO on C3: the carrier an octave down', h, key=48),
    ]


def run3():
    """rom.4's MFX and program A: the EXT IN tone into MULTI, so the input reaches the MFX; each type at its
    defaults."""
    f = {'OSC 1 Switch': 1, 'MFX Switch': 1}
    takes = [
        take('silence', 'the floor', key=None, probe=None),
        take('main', 'the feed: the EXT IN tone through MFX Thru, MULTI to the mix', f, probe='fx', tail=2.0),
        take('main_again', 'the same again', f, probe='fx', tail=2.0),
        take('feed_off', 'MFX Switch off: the same through Master?', {'OSC 1 Switch': 1}, probe='fx'),
        take('sys_eq_off', 'the system EQ off (on by default on rom.4, its Hi Gain 0 dB)', {**f, 'EQ Switch': 0},
             probe='fx'),
        take('peq1_plus15', 'the patch EQ 1 Gain +15 dB: on MULTI only', {**f, 'EQ 1 Gain': 30}, probe='fx'),
        take('peq2_plus15', 'the patch EQ 2 Gain +15 dB', {**f, 'EQ 2 Gain': 30}, probe='fx'),
        take('peq1_direct', 'EQ 1 Gain +15 on the direct sound, MAIN: nothing, if the EQ is MULTI\'s alone',
             {'Direct Sound Level': 127, 'EQ 1 Gain': 30}, probe='fx'),
    ]
    for i in range(1, 42):
        takes.append(take('mfx%02d' % i, 'MFX type %d at its defaults' % i, {**f, 'MFX Type': i},
                          probe='fx', tail=2.0))
    return takes


def run4():
    """rom.4's Chorus and Reverb types, wet: the MFX's sends with its Master at 0, so the dry path is out."""
    f = {'OSC 1 Switch': 1, 'MFX Switch': 1}
    w = {**f, 'MFX Master Level': 0}
    takes = [
        take('silence', 'the floor', key=None, probe=None),
        take('main', 'the feed through MFX Thru at Master 127, the reference', f, probe='fx', tail=2.0),
        take('main_again', 'the same again', f, probe='fx', tail=2.0)]
    for i in range(1, 9):
        takes.append(take('cho%02d' % i, 'Chorus type %d at its defaults, wet' % i,
                          {**w, 'MFX Chorus Send Level': 127, 'Chorus Switch': 1, 'Chorus Type': i},
                          probe='fx'))
    for i in range(1, 15):
        takes.append(take('rev%02d' % i, 'Reverb type %d at its defaults, wet (1-7 rom.4\'s stereo ones)' % i,
                          {**w, 'MFX Reverb Send Level': 127, 'Reverb Switch': 1, 'Reverb Type': i},
                          probe='fx', tail=6.0))
    return takes


RUNS = {'vc2_run_1': run1, 'vc2_run_2': run2, 'vc2_run_3': run3, 'vc2_run_4': run4}
