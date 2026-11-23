# V-Synth XT emulator

A JUCE plugin (AU/VST3/Standalone) that plays samples through ports of the
Roland V-Synth XT's own firmware: its VariPhrase encoders and players, OSC
converters, envelopes, LFOs, pitch path, key assign, COSM filters, TVA. Algorithms
are taken from the user's rom.2 (Ghidra) and every port is checked against the
firmware itself, run in an SH-4 interpreter (`plugin/Source/Sh4Interp.*`; its setup and an image's load in
`Sh4Setup.cpp`, v197).

This file holds the rules and a short map. The long references are read when needed, not on every turn (the user's
decision, 2026-11-23: "Yes, all five"): `research/CODE_MAP.md` (every source file's part, version and check),
`research/TOOLS.md` (every tool and check, and what to run when for each file) and `research/FIRMWARE_FACTS.md` (the
firmware facts behind XtSynth, a patch's load, the effects and the COSM types).

## Rules
- Firmware is Roland copyright: never commit firmware files, disassembly
  listings, or tables copied from them. Ports read tables from the user's
  `firmware/unpacked/V-SynthXT.rom.2.bin` at run time (gitignored).
- Each version is one local commit, `vNN: <what>`, with the attribution line.
  Push only when the user asks, and only the versions named
  (`git push origin <sha>:refs/heads/master`). "Proceed" is not a push.
- A port is done when it equals the firmware in a harness (0 different), and
  existing behaviour is unchanged unless the change is intended. `XtFxChip` goes by JE-8086's interpreter the same way, on the XT's own programs loaded by the firmware's driver.
- Load-time and encode-time firmware runs in the SH-4 interpreter (`Sh4::call` with OS-call hooks, as the `xtFw*`
  harnesses and `vc2PatchRead` do); only real-time code is ported by hand (the user's decision, 2026-10-28).
- The effect chip's programs have no firmware to run, so their reference is captures from the user's XT (the user's decision,
  2026-11-03; HARDWARE_FIRMWARE.md, "The effect captures"). The SH-4 side of the effects still goes by the harness. The digital outs carry the CS8420 converter's output, not the chip's words (v121): USB or Resampling for exact data.
- The project is GPL-3.0 (`LICENSE`, `NOTICE.md`, the user's decision), and JUCE under the AGPLv3 alongside it. JE-8086's ESP core (gearmulator, GPL-3.0) may be used with credit. Its copy in `research/third_party/gearmulator_esp/` stays unmodified: it's the effect chip's reference.
- The VC-2 Vocal Designer (rom.4) comes in as its own mode beside the V-Synth, as the XT's [V-CARD] switches applications, not merged into the V-Synth's patch (the user's decision, 2026-11-13). Its plan and steps: `research/VC2_MODE_PLAN.md`; its work is on the branch `vc2-mode`, and the tag `known-good-before-vc2` (a5562d6) is the plugin before it, known good by the user's word. Its ports read rom.4's tables from the user's `firmware/unpacked/V-SynthXT.rom.4.bin` at run time, as rom.2's are read. Its waves come from the update's disk image beside rom.4: the index and bank past its partition, the Wave Number link files through its FAT (the user's decision, 2026-11-23).
- Files over the kit's 300-line limit are split as the work reaches them, not all at once (the user's decision, 2026-11-09). The kit's file-size-guard refuses any added line in one, so a change to such a file starts with splitting it by what its parts do, and the planted-error lists and `mutate.sh`'s `PORT=` mapping follow the code to its new file.

## Each version (the user's decision, 2026-11-23: "Yes, all five")
- Size: one version takes a part whole where one harness checks it (a player's render, set-ups and seek together,
  as v333 did; a loader's chain), not one routine each, so the gates run once for it.
- Its gates: the harness at 0 different at its seeds; the plants of what changed, `mutate_tone.py --changed <rev>`
  and `mutate_vc2.py --<list> --changed <rev>` (rev the version before; HEAD for work not yet committed), which run the
  plants whose planted file or list line differs from rev's; the per-file runs `research/TOOLS.md` names under "What to
  run when"; `verify.sh`. The whole lists run before a merge, not each version.
- Its facts, written once: a section in `research/HARDWARE_FIRMWARE.md` (newest first), one entry in `notes.txt`, a new
  file's or tool's entry in `research/CODE_MAP.md` or `research/TOOLS.md` in its part's line, its item ticked in
  `research/VC2_MODE_PLAN.md` with the version. `RESEARCH_LOG.md` gets one entry a session, not a version. This file
  changes only when a rule does.

## Where things are
- `plugin/Source/`: the plugin and every port. Each file's part, versions and check: `research/CODE_MAP.md` (read the
  entries of the files you touch; a new file's entry goes there).
- `plugin/Tests/`: PluginTest and the plugin's checks (`VSynthEmu*Test`, what each holds in `research/TOOLS.md`).
- `plugin/`: CMakeLists.txt, BuildId.cmake and `Vc2Sources.cmake`, the VC-2's sources (a new VC-2 file goes there).
- `research/`: `HARDWARE_FIRMWARE.md` (the findings, newest first: addresses, layouts, formulas, checks),
  `VC2_MODE_PLAN.md` (the VC-2's plan and steps), `UI_DESIGN.md` and `UI_DESIGN_VC2.md` (the editor's design, which
  new UI work follows), `CODE_MAP.md`, `TOOLS.md`, `FIRMWARE_FACTS.md`; `research/tools/` the scripts,
  `research/ghidra_scripts/` the Ghidra scripts.
- At the root: `notes.txt` (the versions' notes, and how to build), `RESEARCH_LOG.md` (the sessions), `ARCHITECTURE.md`.
- `IOS_companion/`: a companion app for iPhone and iPad (the XT's Time Trip or X-Y pad over MIDI), built and tested
  apart (the user, 2026-11-13); not part of the plugin's build, tests or outstanding work.

## Tools (`research/tools/`, paths in `env.sh`; work files in `build/tools/`; the whole list in `research/TOOLS.md`)
- `verify.sh [--full] [rev]`: the standard validation, one line each (firmware checks against the baseline, A/B
  renders, the plugin's build and tests, auval). Every version; `--full` before a push.
- `fcheck.sh [--update] [--full]`: every firmware harness against `fcheck_baseline.txt`. In a worktree set `PY` to the
  main checkout's `.venv/bin/python3`. `--update` only when adding checks on purpose, and read its diff.
- `build.sh`: the harnesses and the XtSynth CLI. A harness is `xt_<name> <rom> [cases] [seed]`; `XT_<NAME>_FIRST=1`
  says where it first differs, `XT_<NAME>_STOP=1` stops there.
- Planted errors: `mutate.sh` plants one (its ports in `mutate_vc2ports.sh`, each check's files in
  `mutate_vc2files.sh`); `mutate_tone.py`, `mutate_vc2.py [--<list>]`, both with `--changed <rev>` and `--list`;
  `mutate_panel.py`, `mutate_files.py`, `mutate_fx.py`, `mutate_arp.py`, `mutate_msm.py`, `mutate_enc.py` by what they
  plant. A plant's verdict means something only if the run could have passed: each runs clean first.
- `ab.sh [rev]` (XtSynth renders against rev), `pt.sh [note]` (PluginTest against XtSynth).
- Reading the firmware: `ghidra.sh` (not in a worktree: Ghidra won't open under `.claude`; use the main checkout's
  `firmware/ghidra`), `sh4dis.py`, `build/tools/bin/rd`, `poolrefs.py`, `pnames.py`, `cosmcases.py`.

## What to run when
- While working: the harness of the code you changed, at a few hundred cases, and the plants just added by name.
- Each version: as "Each version" says, with the per-file runs of `research/TOOLS.md`'s "What to run when".
- Before a merge into vc2-mode or master: the whole tone list (`mutate_tone.py`, about 1.7 hours; the user's decision,
  2026-11-21: "Yes, change the rule") and the whole VC-2 lists of the checks the merged changes reach.
- Before a push: `verify.sh --full`.

## Building
- Plugin (JUCE 8.0.7 in ./JUCE, a submodule: in a fresh clone `git submodule update --init JUCE`; `gitlinks_test.sh`, in `verify.sh`, checks every gitlink has its URL): `SDKROOT=$SDK cmake --build build/plugin -j 10`
  (installs AU/VST3), test: `--target VSynthEmuTest`, both (verify.sh): `--target VSynthEmuVerify`. Configure once with
  `cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release` (see notes.txt). The VC-2's sources are listed in `plugin/Vc2Sources.cmake` (v251), which CMakeLists.txt includes: a new VC-2 file goes there.
- Use `.venv/bin/python3` (system python lacks numpy). Write multi-step shell
  work as bash scripts (zsh word-splits differently).
- The editor follows `research/UI_DESIGN.md` (the user's decisions, 2026-11-04 and 2026-11-13): D, drawn at 1200 × 750 and scaled whole, with its regions, pages, palette and how to add a control there. Every control is made from its
  parameter's ID by a table (a rack panel's face, a DETAILS face, a block kind's row, a page's card), an OSC's or a COSM's by its index; `VSynthEmuEditorTest` says if a parameter has no control. Pages don't scroll, so cards must fit; only the FX page's list scrolls. The FX page is `FxPanel`, a `Ui::Page` of cards (`UiParts.h`) in D's palette.

<!-- claude-packs:start -->
<!-- Generated by claude-packs from .claude/layout.json. Edit that file and run `npx --no-install @thedecipherist/claude-packs update`; edits here are replaced. -->
## Project layout

<constraints>
Always do:
- Put every kind of thing in its one place. The rules, checks and skills for it load only there:
  - PluginTest and its checks; a port's firmware check stays in its XT_*_MAIN block, beside the port → `plugin/Tests/`
  - the findings, decisions and designs → `research/`
  - scripts and research tools that never ship → `research/tools/`
  - processBlock, the voices and XtSynth → `plugin/Source/`
  - a port of a firmware block (COSM, LFO, envelope, TVA, the effect chip) → `plugin/Source/`
  - an oscillator or wave player port → `plugin/Source/`
  - parameter IDs and layout, patch and state load/save → `plugin/Source/`
  - the editor and its UI parts (research/UI_DESIGN.md) → `plugin/Source/`
- The other folders:
  - `analysis/`: the earlier VariPhrase analysis: scripts, test files and results; the projects' waves
  - `design/`: shape.json, which the kit's shape law reads
  - `IOS_companion/`: the iOS controller, built and tested elsewhere
  - `plugin/`: the JUCE plugin's build (CMakeLists.txt, BuildId.cmake, and Vc2Sources.cmake, the VC-2 ports' sources, v251)
- Ask the user before inventing a new folder. A new folder goes into `.claude/layout.json` first.
- When a hook, check or guard blocks work that is correct, stop and report its exact message. A blocking hook is a red check: fix the hook first, with a selftest, then continue.
- When a command is refused or fails, the step fails loudly. Never report a refused write or a failed command as done.
- claude-packs (`@thedecipherist/claude-packs`) is the source of truth for every rule, check, hook and skill used to build synths. A fix to one of them, or a lesson that should hold in every synth, is made in the kit, with its test, and comes here with `npx --no-install @thedecipherist/claude-packs update`, so every synth gets it. A project edits its installed copy only to try a fix before it goes into the kit.

Never do:
- Create source files outside these folders, or put a kind of thing anywhere but its place. The folder-law and placement-law hooks refuse both.
- Put drawing code in audio or world folders, or audio and world logic in the painter or UI folders.
- Edit files in `.claude/rules`, `.claude/checks` or `.claude/hooks` to get past a block. Fix the code, or ask the user to change the law.
- Route around a block: no override variables, no writing through the shell after a refusal, no parking files in a scratchpad, no putting new code into an unrelated existing file.
- Point at a document by line number. Cite a fixed anchor (`{#id}`) or a symbol name; lines move.
</constraints>

This project has no package.json: run the kit as `npx @thedecipherist/claude-packs <command>`, wherever its files say `npx --no-install @thedecipherist/claude-packs`.

Installed packs: base, cpp, audio-core, python, shell, reverse-engineering, midi, sound-design, instrument-shell.
<!-- claude-packs:end -->
