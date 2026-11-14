# The editor's design

## The editor, D (the plugin's from v196) {#redesign-d}

The user's decision (2026-11-13): a new UI and workflow with Arturia Pigments and Xfer Serum as ideas, a fresh palette, room
for a V-Synth / VC-2 mode switch, on the branch `ui-redesign` cut from master. Of the four mock-ups on the canvas
https://claude.ai/artifact/RQFA4HhEHdfSYsMYUdYQvP, the user chose D ("Yes, build D"), the mix of A (the module rack) and C
(the signal path, a block's own envelopes and LFO beside it). The mock-up's source, `Mix.dc.html` on that canvas, is the
reference: the numbers below are read from it. The ideas come from Pigments and Serum, their look does not. Nothing here
changes a parameter or the sound: every control sets a parameter, as the tabbed editor's did (v144–v195), and
`VSynthEmuEditorTest` holds D to that. From v196 D is the plugin's editor (`SynthEditor`, `UiEditor.h`), and this is the
only description of it; the tabbed editor's files went (UI step 5).

### The window

Drawn at 1200 × 750 and scaled whole, from 0.8 (960 × 600, the tabbed editor's smallest) to 1.2 (1440 × 900), the aspect
kept. Top to bottom, in the drawing's own units:

| Region | y | Height | Holds |
|---|---|---|---|
| Header | 0 | 48 | the wordmark and the build under it; the mode switch, V-SYNTH and VC-2 (VC-2 greyed until the VC-2 mode is in); the patch box (the patch's name, under it in the warning colour what the patch asks for that isn't here, Browse… to load one); the tempo; the Effects switch; the ROM's state and the ROM… button |
| Page bar | 48 | 34 | SYNTH, MODULATION, FX, PLAY, ZONES; a line saying what a click on a block does |
| Signal path | 82 | 52 | OSC 1, OSC 2, MOD, COSM 1, COSM 2, AMP as blocks to click, the selected one filled with the accent; MFX, CHORUS, REVERB with ON or OFF (a click opens its FX page); the Structure box |
| Rack | 134 | 304 | 12 in from each side, 8 between: the Time Trip column (184 wide), OSC 1 and OSC 2 (310 each), and a column of COSM 1 (116 high), COSM 2 (116) and AMP (the rest) |
| Lower panel | 438 | 272 | the list (148 wide): SELECTED, DETAILS, ENVELOPES, LFOs; then the chosen view |
| Footer | 710 | 40 | Keys MONO or POLY and the number of zones (184 wide), the keyboard: 42 white keys with each zone's range in its colour above them |

The SYNTH page is the path, the rack and the lower panel. The other pages take everything under the page bar.

### The blocks and the lower panel

A click on a block on the path, or on a rack panel's name, selects it: the path fills it, its rack panel's border turns the
accent, and the lower panel follows it. The lower panel (built in step 3) is the selected block's name in the accent over
its list (148 wide: SELECTED, DETAILS, ENVELOPES, LFOs, the chosen one filled), a divider, and the chosen view. What each
kind of block has is one table (`UiBlockKinds.cpp`): its envelopes, its LFO, the Matrix destinations that land on it, the
depths its LFO reaches it by, and its DETAILS' faces; each OSC and each COSM is made from its kind's row and its index.
SELECTED is the block's shape, three columns:

- **Envelope** (392 wide): a tab for each envelope the block has (OSC: Pitch, Time, Formant, Amp; COSM: Env A, Env B; AMP:
  TVA; MOD: none), the curve, and a bar for every parameter the envelope has, in rows of six: Attack, Decay, Sustain,
  Release, Depth, Time KF, then Velo, V-Curve and the A-, D- and R-Velo sensitivities. Bars, not knobs, as an envelope
  has up to eleven and a knob's name would not fit in a sixth of 392; a bar is filled from its range's zero, its value on
  it. The curve takes the height the bars leave (about 392 × 146): the attack up to the top, the decay down to the
  sustain's level, a hold to three quarters, the release to the floor, each time across up to a quarter of the width by
  its share of its range. Its three points drag: the attack's sets Attack, the decay's Decay across and Sustain up, the
  release's Release. Of points on one another (Decay 0 and Sustain full put the decay's on the attack's), a press takes
  the later, which can be drawn off; the earlier would carry it along.
- **LFO** (214 wide): the block's own LFO (OSC n: LFO n; COSM n: LFO 2+n; AMP: LFO 5), its name and Key Sync, its shape (the
  height left, about 214 × 90) drawn from its waveform, rate (one to four cycles), delay, fade (in for ON <<, out for ON >>;
  the fades after a release aren't drawn) and offset, the Wave, Fade Mode and Offset boxes, and the Rate, Delay and Fade
  knobs. The shape of RND, S&H and CHAOS is drawn through fixed levels: a drawing, not the LFO's values.
- **What reaches the block** (the rest): a row for each depth its LFO reaches it by (an accent dot: an OSC's pitch, time or
  PW, formant or Fat and level; AMP's level and pan) and for each Matrix route whose destination lands on it (a `mod` dot:
  its source → its destination), each amount a bar to drag in place. The Matrix's rows are made again when a source or a
  destination moves. A COSM's depths are its type's own, on its panel and in DETAILS. A line says when no route lands on
  the block, or how many more there are than the rows hold, and that the MODULATION page makes them. Dropping a source
  here to make a route comes later (below).

MOD has none of the three, so its SELECTED is its DETAILS. DETAILS holds the selected block's settings that are neither on
its rack panel nor in SELECTED, from a table of faces by its Type (`UiDetailTables.cpp`), each control as its parameter
is (a choice a box, a switch a switch, any other a knob), laid across in rows: for an OSC on ANALOG, Fat and its KF, LFO
and Env, Detune, the sub oscillator, Pitch KF; on PCM, Key, Time Offset, the Time and Formant KFs, Start, Loop and its
mode and points, Original Tempo and Beat Keep; on both, Random pitch, Level KF, Wave gain and the velocity curve. For a
COSM, what its type's five knobs and box on its panel leave (the Compressor, the Limiter and the Speaker Simulator have
nothing more, which a line says); for MOD its switch, type, level and times; for AMP the TVA's KFs and its output. The
patch TVA's velocity is on its envelope's tab. ENVELOPES shows every envelope at once (13 tiles, seven a row) and LFOs
every LFO (5), each its name over its curve or shape; a click on one opens its block in SELECTED, at that envelope.

### The rack

- **Time Trip column:** the pad (162 × 196), whose OSC it follows (1 or 2), Robot and Hold under it.
- **An OSC panel** (one panel, made for each OSC): On, its name, Type (PCM or ANALOG), the encoding (SOLO, BACKING, ENSEMBLE,
  LITE) or the analog waveform, the wave's name, Load…; a display (286 × 72): the wave with its loop and the Time Trip
  position, or the analog waveform; four big knobs (44): Pitch, Time, Formant, Level for PCM, Pitch, Pulse Width, Impact,
  Level for ANALOG; six small (30): Coarse, Fine, Depth, Energy, Velocity Sens, Pitch KF for PCM, Coarse, Fine, PW KF, PW
  LFO, PW Env, Velocity Sens for ANALOG; a line of Vari, Playback Mode and Tempo Sync.
- **A COSM panel** (one panel, made for each COSM): On, its name, Type, its filter type where it has one; a display (96 × 62)
  of the response or the curve; up to five knobs (30), the type's first ones, the rest in DETAILS.
- **AMP:** Level, Pan, Chorus and Reverb sends (24, the name and value beside each knob).

An OSC's and a COSM's panel show their Type's face (`FacedPanel`): its knobs, and the OSC's PCM line, as the panel is
made and then as the Type moves, on the editor's timer. Until v196 the face was chosen while the panel's base was made,
before its knobs were, so a freshly opened editor showed none of them until a Type moved; `VSynthEmuEditorTest`'s first
check is a fresh editor's panels.

### The pages

Built in step 4. Each page but SYNTH takes the drawing under the page bar (1200 × 628) and is made by its row of one table
(`UiPages.cpp`, in the page bar's order); a hidden page draws nothing. Its cards are rack panels' frames with the name at
the top left and what the card holds besides on its title line (`Card`). Most hold a grid of controls made from a table
of parameter IDs, each control as its parameter is: a choice a box under its name, a switch a switch, any other a knob,
a knob a cell (64) and a box or a switch two, a row as high as its tallest (`ControlGrid`, which DETAILS uses too).

- **MODULATION:** the Multi Step Modulator's card down the left (640 wide): its switch, Key Sync and a tab for each track,
  then the chosen track, made for each track by index (`msmTrackFwId`): its eight settings as a grid, and its 16 steps as
  upright bars to drag, in `mod`, filled from 0, the steps past its End Point faint. Down the right, the Matrix's card:
  its eight Controls as rows under the columns' names (Source; Destination 1 and Sens 1; Destination 2 and Sens 2), a row
  made for each Control by index; and the controller assigns' card, the twelve boxes by the processor's names.
- **PLAY:** the arpeggio's card and its pattern's down the left; portamento, keys (Voices, Legato, Octave), tune (Coarse,
  Fine, Bend Up and Down) and tempo (Clock, Tempo) down the right, each a grid from a table. The pattern is a patch's
  data, no parameter, so it is drawn and not edited: its 16 lines down, each a note or a controller named at the left,
  its 32 steps across, a played step's cell in the accent by its velocity, a tie a thin bar carrying the line on, the
  steps past the End Point faint; a line says so when no step plays.
- **ZONES:** the keys' card: the 128 keys with each zone's range in its colour (the footer's), as a note picks its zone,
  its number on it, the C keys named every other octave (`UiZoneStrip`); and the
  zones' card, a cell for each of the sixteen made by index: its colour and name, its Range Upper as a bar in its colour,
  its keys, and what it plays (the panel for zone 1, the patch's as the processor says, or no patch, silent).
- **FX:** the Effects panel (`FxPanel`), in D's palette (`Look::cards`, which `FxPanel`, its `Ui::Page` of cards and its
  `Ui::Segmented` page buttons are given), so its cards, boxes, buttons, switches and sliders are drawn in D's look. A
  click on MFX, CHORUS or REVERB on the path opens the FX page at that effect's own page.

The footer's keyboard and its zones' ranges are step 1's.

### The look

| Token | Colour | Used for |
|---|---|---|
| `ground` | `141518` | window, page bar |
| `header` | `0F1012` | header, footer |
| `panel` | `1C1E22` | rack panels, path blocks, boxes in the header |
| `raised` | `24272D` | buttons and boxes on a panel |
| `lower` | `1A1C20` | the lower panel |
| `field` | `0F1012` | displays: the wave, curves, the pad, routes |
| `edge` | `2C2F36` | a panel's border |
| `divider` | `2A2D33` | lines between regions; the selected list entry |
| `track` | `33363D` | a knob's track, an outlined button |
| `text` | `ECEDEF` | values, names on panels |
| `label` | `A3A8B2` | a control's name |
| `muted` | `8E949F` | hints, secondary text (5.2:1 on `panel`) |
| `dim` | `6B717C` | arrows and marks only, never text (3.3:1) |
| `accent` | `F5A524` | value arcs, the selected block, what is on |
| `mod` | `46C2E0` | modulation: rings on knobs, LFO curves, Matrix routes, the Time Trip |
| `ready` | `5FD28B` | the ROM's state when it is loaded |

Zones take the accent, `mod` and `B9A3FF`, then lighter and darker steps of them for zones 4–16.

Type: a sans for names (Inter in the mock-up), a monospaced face for values (JetBrains Mono). Until they are built in,
the system's sans and its monospaced face stand in. Sizes: the wordmark 15 bold, spaced 0.12; page names 12 bold, spaced
0.1; a panel's name 12 bold, spaced 0.1; a knob's name 10 or 11 semibold in `label`; its value 10 or 11 in the monospaced
face; a path block's name 11 bold over its setting 10.

A knob sweeps 270°, from 135° (lower left) to 405° (lower right). Its track is the whole sweep in `track`, 3 to 4 wide with
round ends; its value arc is `accent` from the bottom of the sweep, or from the top for a bipolar knob; a modulated knob
has a `mod` ring 3.2 outside the track over the modulation's reach; a big knob has a cap (`2A2D33`); a pointer in `text`
runs from the centre to 6 inside the track. Its name is under it, its value under that.

### Where the tabbed editor's controls went

| The tabbed editor (v144–v195) | In D |
|---|---|
| Header, source bar | the header (Load Patch is the patch box's Browse…, the patch's name and what it misses are the patch box's); Load WAV is each OSC panel's Load…; OSC 1/2 goes, both OSCs show; On and Type are the OSC panel's |
| Perform column | each OSC panel's big knobs and its line (Robot under the pad, for the OSC the pad follows) |
| Oscillator tab | the OSC panels and their DETAILS |
| Envelopes, LFOs tabs | SELECTED for each block, and ENVELOPES and LFOs |
| COSM tab | the COSM panels and their DETAILS; LFO 3 and 4 in SELECTED; MOD is a block of its own; Structure is on the path |
| Amp & Pan tab | AMP and its DETAILS |
| Play tab | the PLAY page |
| MSM, Matrix tabs | the MODULATION page; a block's Matrix routes in SELECTED too |
| Zones tab | the ZONES page; the zones' ranges on the footer's keyboard |
| Effects tab | the FX page; ON or OFF of each block on the path |

### The steps

D was new code in files of its own (`Ui*`), beside the tabbed editor, which stayed as it stood until D took its place: the
plugin kept it until the last step, and a test program of its own drew and checked D as it grew. Each step is a commit
`UI step n: …` on `ui-redesign`, with D's snapshots and its layout check.

The first plan split `PluginEditor.cpp` (1,483 lines) by job and changed it a step at a time. Moved as it stood, its parts
were refused by the kit's guards (2026-11-13: loops repeated with other names, the constructor's parts past 50 statements),
so splitting it meant rewriting code D replaces whole: the commit and its revert are on the branch.

0. This design and the plan.
1. D's look and frame: the palette, type and LookAndFeel (knobs, switches, boxes, buttons), a knob made from a parameter's
   ID, the scaled window, the header, the page bar, the signal path and its selection, the footer; `VSynthEmuEditorTest`
   draws and checks it.
2. The rack: the Time Trip column, the OSC panels, the COSM panels, AMP, from tables of which parameter goes where.
3. The lower panel: SELECTED (the envelope editor, the LFO, what reaches the block), DETAILS, ENVELOPES, LFOs. Done
   2026-11-13: `VSynthEmuEditorTest` holds each block's views to its kind's tables, each instance's controls to its own
   parameters, a curve's points to theirs, a route to its block, and the list's and overviews' presses.
4. The MODULATION, PLAY, ZONES and FX pages; the footer's keyboard with the zones. Done 2026-11-13:
   `VSynthEmuEditorTest` holds each MSM track, Matrix Control and zone to its own parameters, the FX page's four pages,
   each effect on the path to its page, and D to every parameter but the engine's three and Scale Tune (745 of 761).
5. Landing: the plugin's editor is D, the tabbed editor's files go, `VSynthEmuEditorTest` takes `TEST_UI_LAYOUT`'s place
   and the plants hold D, these sections the only description, v196. Done 2026-11-13: the tabbed editor's twelve files
   and its layout test went; `VSynthEmuEditorTest` took the checks of it D's lacked (every effect type, the FX page's
   buttons and cards, the header's build and buttons) and a fresh editor's faces, which found D's panels showing no
   knobs until a Type moved (fixed); D's patch box says what a patch misses; D has 29 planted errors of its own.

### Adding a control

Every control is made from its parameter's ID, so a new parameter needs a row in a table, not code of its own:

1. Its place: a rack panel's face (`kFaces` in `UiOscPanel.cpp` or `UiCosmPanel.cpp`) for what a block is played by; its
   kind's DETAILS face (`UiDetailTables.cpp`) for the rest of a block's settings; its kind's row (`UiBlockKinds.cpp`) for
   an envelope, an LFO, a depth or a Matrix destination; a page's card (`Pages::CardRows`, as `UiPlayPage.cpp`'s) for
   what is the patch's; the FX page (`FxPanel::buildCards`) for the effects'.
2. Its parameter's ID and its name: the control is made as its parameter is (`Controls`, `ControlGrid`: a choice a box, a
   switch a switch, any other a knob, or on an envelope a bar) and attached to it. An OSC's or a COSM's is made for each
   instance by its index (`Params::oscId`, `Params::cosmId`), never named for one.
3. `VSynthEmuEditorTest`: it moves every control and says a parameter no control reaches (`list` prints them all), a
   control over another or outside its parent, and text cut short. Pages don't scroll: a card must fit its page's
   1200 × 628, and only the FX page's list scrolls.

### Checks

- `VSynthEmuEditorTest <rom> [folder | list]` (in `verify.sh`; from v196 in `TEST_UI_LAYOUT`'s place): a fresh editor's
  panels showing their faces; at the smallest, default and largest sizes the drawing filling the window, scaled whole; on
  each page, and on SYNTH with each block selected, no control outside its parent, over another, without a size or with
  its text cut short; every control moved, each instance's (each OSC, COSM, MSM track, Matrix Control and zone) moving its
  own parameters alone, and every parameter with a control but the 16 below; each page's name, block and effect pressed;
  the header's build, Browse…, ROM… and each PCM OSC's Load…, and a patch's note; a curve's points dragged, a route on its
  block, the list's and the overviews' presses; the FX page on every effect type, its list's text whole with room for 15
  rows, its left cards level with its list, and its page buttons both ways. With a folder it draws each page, each block and each lower view into it; with
  `list` it prints the parameters no control reaches.
- `mutate_panel.py --editor`: the editor's planted errors, D's (`panel_muts.txt`'s `editor-d-` lines) and the FX page's
  (`editor-fx-`, `editor-row-gap`), each built into `VSynthEmuEditorTest`, to show the check catches it.
- `TEST_SNAPSHOT=file.png` (PluginTest): D drawn offscreen as it opens, with `TEST_SNAPSHOT_PAGE=n`, `TEST_SNAPSHOT_BLOCK=n`,
  `TEST_SNAPSHOT_SIZE=WxH`, `TEST_SNAPSHOT_ANALOG=1`, `TEST_SNAPSHOT_TYPE=n` (COSM 1's type, it selected),
  `TEST_SNAPSHOT_FX=page` (with `TEST_SNAPSHOT_FXTYPE=n`) and `TEST_SNAPSHOT_PATCH=file.p00`. `TEST_FX_PANEL=1`: the FX
  page's lists and values against the firmware's text.

### Not shown anywhere

16 of the 761 parameters have no control; only a host reaches them:

- Scale Tune (13).
- The engine's Pitch, Time and Formant (3): unread since v191, kept for old sessions.

### Still to do

Each with the user's yes where it needs one: Inter and JetBrains Mono built in (a folder for font files); the wave drawn
from the loaded wave; the pad sending its controllers; the keyboard playing; dragging a source onto a knob to make a
Matrix route; the VC-2 mode's pages; the page and the block kept when the editor's window closes (it opens on SYNTH, with
COSM 1 selected).
