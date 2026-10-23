# research/ — provenance

Primary-source material used to derive the emulation.  Reference only; none of it is
ours to redistribute.

## V-Synth_Owners_Manual.pdf

Roland V-Synth Owner's Manual, © Roland Corporation.  Retrieved 2026-10-21.

Roland's own download (`roland.com/global/support/by_product/v-synth/owners_manuals/`)
is behind a click-through licence agreement that returns no direct file URL, so this copy
came from a third-party mirror: `http://www.plzi.com/aanta/manuals/V-SYNTH-owner-manual.pdf`.
It is the V1 manual — the **MIDI Implementation** appendix is dated Nov. 20 2002 and the
MIDI Implementation Chart is version 1.00.  Roland also publish a separate "Version 2
Manual" and the V-Synth XT manuals, neither of which is mirrored here.

Verify against Roland's official copy before relying on it for anything load-bearing.

## V-Synth_Owners_Manual.txt

`pdftotext -layout` extraction of the above (13,001 lines).  This is the file to grep —
the PDF's text layer is Flate-compressed and not searchable in place.

Two-column pages come out with both columns on the same line, so a `grep` hit may show
unrelated text from the facing column.  Read around a hit with `sed -n` before trusting
the layout.

Useful offsets:

| Line    | Section                                                      |
|---------|--------------------------------------------------------------|
| ~10023  | MIDI Implementation appendix begins                          |
| ~10417  | Control Change — Data Transmission (incl. CC80–83 / GP 5–8)  |
| ~10567  | Parameter Address Map — block base addresses                 |
| ~10746  | System Controller block (pad / D Beam / knob CC assignments) |
| ~11784  | Patch Controller block (Beam Type, Pad Mode, Pad Hold)       |
| ~12247  | MIDI Implementation Chart                                    |

Every SysEx address and range in `../CONTROLLERS.md` traces back to this file.

## V-SYNTH_XT_OM.pdf / .txt

Roland V-Synth XT Owner's Manual, © Roland Corporation.  The PDF lives in
`Vsynth_software/V-SYNTH_XT_OM.pdf` (gitignored); only the text extraction is here.  Added by the
repo owner — I did not fetch it and cannot attest to where it came from.  Its own imprint
line reads `Reference_e.book ... ２００５年３月８日`, the March 2005 printing; its chart is
version 1.00.

The `.txt` is a `pdftotext -layout` extraction (14,556 lines) with the same two-column
caveat as above.

**It does not contain the parameter address map.**  The XT Owner's Manual carries only the
MIDI Implementation Chart; Roland ship the XT's full MIDI Implementation as a separate
download.  Every SysEx address in `../CONTROLLERS.md` therefore comes from the V-Synth
manual, with XT parity assumed but unverified.

Useful offsets:

| Line   | Section                                               |
|--------|-------------------------------------------------------|
| ~4966  | Matrix Control source list (PAD-X, TRIP-R, ...)       |
| ~8287  | Controller Settings — the four TT Pad CC assignments  |
| ~9830  | Parameter List — Matrix Control Source                |
| ~10278 | Parameter List — TT Pad/Knob ranges                   |
| ~13654 | MIDI Implementation Chart (manual p. 201–202)         |

For CC-level questions prefer the **V-Synth** manual's chart (line ~12247): its per-CC
rows name `TT Pad XY-X` / `XY-Y` / `TT-X` / `TT-Y` against CC80–83, where the XT's chart
collapses the same ground into a single `1–31, 33–95` row.

## PATENTS.md

Predates this directory listing; see the file itself for its own sources.

## A note on tracking

The PDFs are 3.8 MB (V-Synth) and 7.7 MB (XT); the extractions about 1.1 MB each.  None of
them is committed; `research/*.pdf` and `research/*.txt` are gitignored (Roland
copyright, and the repo is on GitHub).
