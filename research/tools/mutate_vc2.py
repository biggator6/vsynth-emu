#!/usr/bin/env python3
# mutate_vc2.py [cases] [name ...]: every deliberate error in vc2_muts.txt (or the named ones), each planted by mutate.sh
# in Vc2Params.cpp (PORT=vc2params, rom.4's parameter tables, v199) and run through xt_vc2params on rom.4, [cases] values
# an ID besides its ends and default (2), several at a time; a few seconds.  One line each, as mutlist.run_plants says.
# vc2_muts.txt: name|old|new[|port[|seed]], with \n and \| escaped.
import sys
import mutlist
args = sys.argv[1:]
cases = args.pop(0) if args and args[0].isdigit() else '2'
XT_VC2PARAMS = mutlist.Harness('mutate_vc2', 'xt_vc2params', 'vc2_muts.txt', ('vc2params', '2024'))
sys.exit(mutlist.run_plants(XT_VC2PARAMS, args, cases))
