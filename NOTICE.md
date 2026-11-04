# Licence and credits

V-Synth XT emulator. Copyright (C) 2026 Mark Borrow.

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License, version 3, as
published by the Free Software Foundation (`LICENSE`). It comes with no warranty, as that licence says.

## What isn't covered

- **Roland's firmware.** The firmware is Roland's copyright and isn't part of this project. The code reads tables and programs at run time
  from your own `firmware/unpacked/V-SynthXT.rom.2.bin`, and runs some of it in an SH-4 interpreter. You need your own copy.
- **The manuals.** These are Roland's too, and they aren't in the repository.

## What it's built with

- **[JUCE 8](https://juce.com)**, which isn't in this repository. It's dual-licensed under the AGPLv3 and a commercial licence. GPLv3 section 13
  allows a GPLv3 work to be linked with AGPLv3 code, each part keeping its own licence, so a plugin built from this source with JUCE under the AGPLv3 carries the
  terms of both. That matters if you distribute a build, or offer it over a network.

## Credits

- **The effect chip.** `plugin/Source/XtFxChip.*` is written from what the XT's firmware shows and from JE-8086's emulation of the same family of chip, the
  Roland/Toshiba ESP in the JP-8000 (TC170C140), by The Usual Suspects in [gearmulator](https://github.com/dsp56300/gearmulator), GPL-3.0. They took its instruction set from the
  silicon. An unmodified copy of their interpreter is in `research/third_party/gearmulator_esp/`, as the reference that port is checked against.
