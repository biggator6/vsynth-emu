#!/usr/bin/env python3
"""vc2cardrun.py: the VC-2 runs' patches on a card (research/VC2_MODE_PLAN.md, step 5, the user's "Patch files",
2026-11-25).  What a take sets that rom.4 takes no SysEx for, but a patch holds, goes in a patch file made by rom.4's
own routines (vc2patchwrite: its default patch, its setters, its writer; HARDWARE_FIRMWARE.md, "A VC-2 patch saved").
The files sit in a project folder on the card, which rom.4's boot takes when the card's "VC-2 Project.ini" names it, a
file "NNN name.p02" becoming the bank's patch NNN; a take selects its patch by Bank Select and Program Change, which
rom.4 parses as MSB 87, LSB 0 for 1…64 and LSB 1…4 for 65…512 (HARDWARE_FIRMWARE.md, "A card's patches in VC-2
mode").  Each patch holds BASE's every patch value with the take's own of those, so after its Program Change the run
knows what the patch holds and sends by SysEx only what differs."""
import os, subprocess

PROJECT = 'RUNS.vc2'                  # the project folder on the card
INI_NAME = 'VC-2 Project.ini'         # [FW] on the card's root: "C:\VC-2 Project.ini" (0x89342cb0)
INI = '[current project]\r\ndrive="PC Card"\r\npath="%s\\"\r\n' % PROJECT   # [FW] its three lines, each read whole
BANK_MSB = 87                         # [FW] 0x89318a20: the only MSB it takes
STORE_FIRST, STORE_LAST = 0x120, 0x4f6    # [FW] the IDs a patch's store holds (v450's sections), the typed ones aside
LOAD_MS = 1500                        # a patch's load after its Program Change, before anything is sent over it


def program_of(n):
    """Patch n (1…512) as (LSB, Program Change): rom.4's parse (0x89318a20) the other way round."""
    if not 1 <= n <= 512:
        raise ValueError('rom.4 has patches 1 to 512, not %d' % n)
    return (0, n - 1) if n <= 64 else ((n - 65) // 128 + 1, (n - 65) % 128)


class Patches:
    """The patches a set of runs needs: one for each combination of what SysEx can't send, numbered from 1 in the
    order the takes first need them."""
    def __init__(self, m, base, unaddressable):
        self.m, self.base = m, base
        ids = {n: [i for i, x in enumerate(m.names) if x == n] for n in base}
        self.held = {n: i[0] for n, i in ids.items() if len(i) == 1 and STORE_FIRST <= i[0] <= STORE_LAST}
        self.only = [n for n in unaddressable if n in self.held]           # what a patch must carry
        self.by_hand = [n for n in unaddressable if n not in self.held]    # what neither reaches: the system's
        self.numbers = {}

    def key(self, want):
        return tuple((n, want[n]) for n in self.only)

    def number(self, want):
        """The patch a take's settings need, numbered on first use."""
        return self.numbers.setdefault(self.key(want), len(self.numbers) + 1)

    def holds(self, n):
        """What patch n holds of the addressable patch values: BASE's."""
        return {k: v for k, v in self.base.items() if k in self.held and k not in self.only}

    def values(self, key):
        """A patch's every value, BASE's with its own over them, as rom.4's read gives them (less the centre)."""
        want = {**{k: v for k, v in self.base.items() if k in self.held}, **dict(key)}
        return ['%#x=%d@1' % (self.held[k], v - self.m.descriptor(self.held[k])[0]) for k, v in sorted(want.items())]

    def write(self, out, writer, rom4):
        """The card's folder under `out`: the ini and each patch's file, by rom.4's own routines (vc2patchwrite)."""
        folder = os.path.join(out, PROJECT, 'Patch')
        os.makedirs(folder, exist_ok=True)
        os.makedirs(os.path.join(out, PROJECT, 'Wave'), exist_ok=True)
        with open(os.path.join(out, INI_NAME), 'w', newline='') as f:
            f.write(INI)
        for key, n in sorted(self.numbers.items(), key=lambda kv: kv[1]):
            path = os.path.join(folder, '%03d RUN %03d.p02' % (n, n))
            subprocess.run([writer, rom4, path] + self.values(key), check=True)
        return folder
