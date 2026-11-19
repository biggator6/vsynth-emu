#!/usr/bin/env python3
# mutate_vc2.py [--in | --legacy | --hireso] [cases] [name ...]: every deliberate error in a list of rom.4's ports (or the named
# ones), each planted by mutate.sh and run through its harness on rom.4, several at a time.  With no switch:
# vc2_muts.txt, in Vc2Params.cpp (PORT=vc2params, rom.4's parameter tables, v199), through xt_vc2params, [cases] values
# an ID besides its ends and default (2), a few seconds.  --in: vc2in_muts.txt, in Vc2Unvoiced.cpp and Vc2Consonants.cpp
# (PORT=vc2in, vc2out: the input and output stages, v208, v209), through xt_vc2in, [cases] its cases (8).  --legacy:
# vc2legacy_muts.txt, in Vc2LegacyAnalysis.cpp (PORT=vc2legacy, LEGACY's analysis, v210), through xt_vc2legacy, [cases]
# its cases (8).  --hireso: vc2hireso_muts.txt, in Vc2HiresoWarp.cpp, Vc2HiresoTables.cpp, Vc2HiresoRate.cpp,
# Vc2HiresoAnalysis.cpp, Vc2Hireso.cpp and Vc2HiresoSynth.cpp (PORT=vc2warp, vc2hitables, vc2rate, vc2hiana, vc2hiobj,
# vc2hisyn: HIRESO's formant warp, v213, rate filters, v214, analysis, v215, object, v216, and mono synthesis, v217),
# through xt_vc2hireso, [cases] its share of the
# ratios' grid (8: every fifth) and its other parts' cases.  --vocoder: vc2voc_muts.txt, in Vc2Vocoder.cpp and
# Vc2VocoderTables.cpp (PORT=vc2voc, vc2voctables: the vocoder object, v220), through xt_vc2vocoder, [cases] its cases
# (8).  --carrier: vc2car_muts.txt, in Vc2Carrier.cpp and Vc2CarrierTables.cpp (PORT=vc2car, vc2cartables: the VOCODER
# OSC's carrier, v221), through xt_vc2carrier, [cases] its cases (40, fcheck's: the Dynamics' and the caps' edges
# need them).  --tracker: vc2track_muts.txt, in Vc2Tracker.cpp, Vc2TrackerTables.cpp, Vc2TrackerAnalysis.cpp and
# Vc2TrackerEstimate.cpp (PORT=vc2tracker, vc2tracktables, vc2trackana, vc2trackest: the pitch tracker's frame, v223,
# and analysis, v224), through xt_vc2tracker, [cases] its cases (100, fcheck's).  --kind6: vc2k6_muts.txt, in
# Vc2Kind6.cpp, Vc2Kind6Core.cpp, Vc2Kind6Tables.cpp and Vc2Math.cpp (PORT=vc2k6, vc2k6core, vc2k6tables, vc2k6math:
# voice kind 6 and the mic ring, v225), through xt_vc2kind6, [cases] its cases (40).  --autonote: vc2an_muts.txt, in
# Vc2AutoNote.cpp and Vc2AutoNoteTables.cpp (PORT=vc2an, vc2antables: Auto Note and the pitch source, v226), through
# xt_vc2autonote, [cases] its cases (40).  --mixer: vc2mix_muts.txt, in Vc2PartMixer.cpp, Vc2MixerBlock.cpp,
# Vc2PartMixerTables.cpp and Vc2Math.cpp's clamp (PORT=vc2mix, vc2mixblock, vc2mixtables, vc2mixmath: the part mixer,
# v227), through xt_vc2mixer, [cases] its cases (40).  --tone: vc2tone_muts.txt, in Vc2Tone.cpp (PORT=vc2tone: the
# tone object's frame and its pool, v228), through xt_vc2tone, [cases] its cases (40).  --osc: vc2osc_muts.txt, in
# Vc2OscVoice.cpp, Vc2OscVoiceGet.cpp, Vc2OscVoiceKinds.cpp, Vc2OscTables.cpp, Vc2OscNoteOn.cpp and Vc2OscTargets.cpp
# (PORT=vc2osc, vc2oscget, vc2osckinds, vc2osctables: the OSC voice's frame, v230; vc2oscnoteon, vc2osctargets: its
# note-on by kind and its targets, v232; vc2ramp, vc2ramptick, vc2ramptables: its pitch ramp, v236, in
# Vc2PitchRamp.cpp, Vc2PitchRampTick.cpp and Vc2RampTables.cpp; vc2oscplayers: its players' installs and VariPhrase's
# note-on, v238, in Vc2OscPlayers.cpp; vc2osclite: LITE's note-on, v240, in Vc2OscLite.cpp; vc2oscanalog and
# vc2oscanalogla: the analog's note-on, v242, in Vc2OscAnalog.cpp and Vc2OscAnalogLa.cpp; vc2oscanalogsub: its sub
# oscillator's, v244, in Vc2OscAnalogSub.cpp; vc2oscanalogshape: its shape, v246, in Vc2OscAnalogShape.cpp, its LA
# way vc2oscanalogla's; vc2oscanalogupdate: the analog's install, update and settings, v248, in
# Vc2OscAnalogUpdate.cpp; vc2oscvibrato: the vibrato, v267, in Vc2OscVibrato.cpp), through xt_vc2osc, [cases]
# its cases (40).  --tva:
# vc2tva_muts.txt, in
# Vc2Tva.cpp and Vc2TvaTables.cpp (PORT=vc2tva, vc2tvatables: the TVA, v233), through xt_vc2tva, [cases] its cases
# (40).  --cosm: vc2cosm_muts.txt, in Vc2CosmGrowl.cpp, Vc2CosmTables.cpp and Vc2Math.cpp's sine and cosine
# (PORT=vc2growl, vc2cosmtables, vc2cosmmath: the Growl, v234), Vc2CosmConvert.cpp, Vc2CosmTvf.cpp, Vc2Matrix.cpp and
# Vc2MatrixTables.cpp (vc2cosmconvert, vc2cosmtvf, vc2matrix, vc2matrixtables: the converters' frame, v235),
# Vc2CosmFilters.cpp, Vc2CosmDynamics.cpp and Vc2CosmConverters.cpp (vc2cosmfilters, vc2cosmdynamics, vc2cosmtypes: the
# converters' first batch, v237), through xt_vc2cosm, [cases] its cases (40).  --libm: vc2libm_muts.txt, in
# Vc2LibMath.cpp and Vc2LibMathTables.cpp (PORT=vc2libm, vc2libmtables: the library's logf and powf), through
# xt_vc2libm, [cases] its draws a routine (20000, fcheck's).  --voices: vc2voices_muts.txt, in Vc2PartVoices.cpp and
# Vc2PartVoicesTables.cpp (PORT=vc2voices, vc2voicestables: the part's voices and their lists, v272), through
# xt_vc2voices, [cases] its cases (40).  --notes: vc2notes_muts.txt, in Vc2Notes.cpp (PORT=vc2notes: the part's
# notes W, v275), through xt_vc2notes, [cases] its cases (40).  --keys: vc2keys_muts.txt, in Vc2Keys.cpp and
# Vc2KeysTables.cpp (PORT=vc2keys, vc2keystables: the part's keys, v276), through xt_vc2keys, [cases] its cases
# (40).  --sets: vc2sets_muts.txt, in Vc2Sets.cpp, Vc2NoteMatrix.cpp and Vc2SetCommon.cpp (PORT=vc2sets, vc2notematrix,
# vc2setcommon: the part's conversion sets, the notes' Matrix objects, v280, and C's common, v281), through
# xt_vc2sets, [cases] its cases (40).  One line
# each, as mutlist.run_plants says.  The lists: name|old|new[|port[|seed]], with \n and \| escaped.
import sys
import mutlist

# Each list's harness and its default [cases], by the switch that names it
HARNESSES = {
    '': (mutlist.Harness('mutate_vc2', 'xt_vc2params', 'vc2_muts.txt', ('vc2params', '2024')), '2'),
    '--in': (mutlist.Harness('mutate_vc2', 'xt_vc2in', 'vc2in_muts.txt', ('vc2in', '2024'), 'XT_VC2IN_STOP'), '8'),
    '--legacy': (mutlist.Harness('mutate_vc2', 'xt_vc2legacy', 'vc2legacy_muts.txt', ('vc2legacy', '2024'),
                                 'XT_VC2LEGACY_STOP'), '8'),
    '--hireso': (mutlist.Harness('mutate_vc2', 'xt_vc2hireso', 'vc2hireso_muts.txt', ('vc2warp', '2024'),
                                 'XT_VC2HIRESO_STOP'), '8'),
    '--vocoder': (mutlist.Harness('mutate_vc2', 'xt_vc2vocoder', 'vc2voc_muts.txt', ('vc2voc', '2024'),
                                  'XT_VC2VOCODER_STOP'), '8'),
    '--carrier': (mutlist.Harness('mutate_vc2', 'xt_vc2carrier', 'vc2car_muts.txt', ('vc2car', '2024'),
                                  'XT_VC2CARRIER_STOP'), '40'),
    '--tracker': (mutlist.Harness('mutate_vc2', 'xt_vc2tracker', 'vc2track_muts.txt', ('vc2tracker', '2024'),
                                  'XT_VC2TRACKER_STOP'), '100'),
    '--kind6': (mutlist.Harness('mutate_vc2', 'xt_vc2kind6', 'vc2k6_muts.txt', ('vc2k6', '2024'),
                                'XT_VC2KIND6_STOP'), '40'),
    '--autonote': (mutlist.Harness('mutate_vc2', 'xt_vc2autonote', 'vc2an_muts.txt', ('vc2an', '2024'),
                                   'XT_VC2AUTONOTE_STOP'), '40'),
    '--mixer': (mutlist.Harness('mutate_vc2', 'xt_vc2mixer', 'vc2mix_muts.txt', ('vc2mix', '2024'),
                                'XT_VC2MIXER_STOP'), '40'),
    '--tone': (mutlist.Harness('mutate_vc2', 'xt_vc2tone', 'vc2tone_muts.txt', ('vc2tone', '2024'),
                               'XT_VC2TONE_STOP'), '40'),
    '--osc': (mutlist.Harness('mutate_vc2', 'xt_vc2osc', 'vc2osc_muts.txt', ('vc2osc', '2024'), 'XT_VC2OSC_STOP'),
              '40'),
    '--tva': (mutlist.Harness('mutate_vc2', 'xt_vc2tva', 'vc2tva_muts.txt', ('vc2tva', '2024'), 'XT_VC2TVA_STOP'),
              '40'),
    '--voices': (mutlist.Harness('mutate_vc2', 'xt_vc2voices', 'vc2voices_muts.txt', ('vc2voices', '2024'),
                                 'XT_VC2VOICES_STOP'), '40'),
    '--notes': (mutlist.Harness('mutate_vc2', 'xt_vc2notes', 'vc2notes_muts.txt', ('vc2notes', '2024'),
                                'XT_VC2NOTES_STOP'), '40'),
    '--keys': (mutlist.Harness('mutate_vc2', 'xt_vc2keys', 'vc2keys_muts.txt', ('vc2keys', '2024'), 'XT_VC2KEYS_STOP'),
               '40'),
    '--sets': (mutlist.Harness('mutate_vc2', 'xt_vc2sets', 'vc2sets_muts.txt', ('vc2sets', '2024'), 'XT_VC2SETS_STOP'),
               '40'),
    '--cosm': (mutlist.Harness('mutate_vc2', 'xt_vc2cosm', 'vc2cosm_muts.txt', ('vc2growl', '2024'), 'XT_VC2COSM_STOP'),
               '40'),
    '--libm': (mutlist.Harness('mutate_vc2', 'xt_vc2libm', 'vc2libm_muts.txt', ('vc2libm', '2024'), 'XT_VC2LIBM_STOP'),
               '20000'),
}
args = sys.argv[1:]
switch = args.pop(0) if args and args[0] in HARNESSES else ''
harness, default = HARNESSES[switch]
cases = args.pop(0) if args and args[0].isdigit() else default
sys.exit(mutlist.run_plants(harness, args, cases))
