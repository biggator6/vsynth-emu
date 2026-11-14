#!/usr/bin/env python3
# mutate_msm.py [cases] [name ...]: every deliberate error in msm_muts.txt (or the named ones), each planted by mutate.sh in
# XtMsm.cpp, or by its port in XtMsmTrack.cpp (msmtrack) or XtMsmPatch.cpp (msmpatch), and run through xt_msm on [cases]
# random cases (300) after the presets, each to its first case that differs (XT_MSM_STOP), several at a time.  One line
# each: caught (or "stopped", when it crashes or hangs), MISSED, or ERROR when the old text is no longer in the source or
# the run never started.  It stops first if a run with nothing planted doesn't say 0 different, for each port and seed
# the list uses.  msm_muts.txt: name|old|new[|port[|seed]], with \n and \| escaped.  The runner is mutlist.run_plants
# (v199, from here).
import sys
import mutlist
args = sys.argv[1:]
cases = args.pop(0) if args and args[0].isdigit() else '300'
XT_MSM = mutlist.Harness('mutate_msm', 'xt_msm', 'msm_muts.txt', ('msm', '2024'), 'XT_MSM_STOP')
sys.exit(mutlist.run_plants(XT_MSM, args, cases))
