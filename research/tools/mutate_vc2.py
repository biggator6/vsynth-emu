#!/usr/bin/env python3
# mutate_vc2.py [--in] [cases] [name ...]: every deliberate error in vc2_muts.txt (or the named ones), each planted by
# mutate.sh in Vc2Params.cpp (PORT=vc2params, rom.4's parameter tables, v199) and run through xt_vc2params on rom.4,
# [cases] values an ID besides its ends and default (2), several at a time; a few seconds.  --in: vc2in_muts.txt's, in
# Vc2Unvoiced.cpp (PORT=vc2in, the unvoiced detector, v208), through xt_vc2in, [cases] its cases (8).  One line each, as
# mutlist.run_plants says.  The lists: name|old|new[|port[|seed]], with \n and \| escaped.
import sys
import mutlist
args = sys.argv[1:]
unvoiced = bool(args) and args[0] == '--in'
if unvoiced:
    args.pop(0)
cases = args.pop(0) if args and args[0].isdigit() else ('8' if unvoiced else '2')
HARNESSES = {
    False: mutlist.Harness('mutate_vc2', 'xt_vc2params', 'vc2_muts.txt', ('vc2params', '2024')),
    True: mutlist.Harness('mutate_vc2', 'xt_vc2in', 'vc2in_muts.txt', ('vc2in', '2024'), 'XT_VC2IN_STOP'),
}
sys.exit(mutlist.run_plants(HARNESSES[unvoiced], args, cases))
