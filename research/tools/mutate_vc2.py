#!/usr/bin/env python3
# mutate_vc2.py [--in | --legacy | --hireso] [cases] [name ...]: every deliberate error in a list of rom.4's ports (or the named
# ones), each planted by mutate.sh and run through its harness on rom.4, several at a time.  With no switch:
# vc2_muts.txt, in Vc2Params.cpp (PORT=vc2params, rom.4's parameter tables, v199), through xt_vc2params, [cases] values
# an ID besides its ends and default (2), a few seconds.  --in: vc2in_muts.txt, in Vc2Unvoiced.cpp and Vc2Consonants.cpp
# (PORT=vc2in, vc2out: the input and output stages, v208, v209), through xt_vc2in, [cases] its cases (8).  --legacy:
# vc2legacy_muts.txt, in Vc2LegacyAnalysis.cpp (PORT=vc2legacy, LEGACY's analysis, v210), through xt_vc2legacy, [cases]
# its cases (8).  --hireso: vc2hireso_muts.txt, in Vc2HiresoWarp.cpp, Vc2HiresoTables.cpp, Vc2HiresoRate.cpp,
# Vc2HiresoAnalysis.cpp and Vc2Hireso.cpp (PORT=vc2warp, vc2hitables, vc2rate, vc2hiana, vc2hiobj: HIRESO's formant
# warp, v213, rate filters, v214, analysis, v215, and object, v216), through xt_vc2hireso, [cases] its share of the
# ratios' grid (8: every fifth) and its other parts' cases.  One line
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
}
args = sys.argv[1:]
switch = args.pop(0) if args and args[0] in HARNESSES else ''
harness, default = HARNESSES[switch]
cases = args.pop(0) if args and args[0].isdigit() else default
sys.exit(mutlist.run_plants(harness, args, cases))
