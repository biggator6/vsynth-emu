# The editor's design

## The redesign, D (from 2026-11-13) {#redesign-d}

The user's decision (2026-11-13): a new UI and workflow with Arturia Pigments and Xfer Serum as ideas, a fresh palette, room
for a V-Synth / VC-2 mode switch, on the branch `ui-redesign` cut from master. Of the four mock-ups on the canvas
https://claude.ai/artifact/RQFA4HhEHdfSYsMYUdYQvP, the user chose D ("Yes, build D"), the mix of A (the module rack) and C
(the signal path, a block's own envelopes and LFO beside it). The mock-up's source, `Mix.dc.html` on that canvas, is the
reference: the numbers below are read from it. The ideas come from Pigments and Serum, their look does not. Nothing here
changes a parameter or the sound: every control sets a parameter, as today, and `TEST_UI_LAYOUT` holds D to that.

Until step 5 lands, the sections after this one describe the editor as it stands.

### The window

Drawn at 1200 × 750 and scaled whole, from 0.8 (960 × 600, today's smallest) to 1.2 (1440 × 900), the aspect kept. Top to
bottom, in the drawing's own units:

| Region | y | Height | Holds |
|---|---|---|---|
| Header | 0 | 48 | the wordmark and the build under it; the mode switch, V-SYNTH and VC-2 (VC-2 greyed until the VC-2 mode is in); the patch box (previous, the patch's name, next, Browse… which is today's Load Patch); the tempo; the Effects switch; the ROM's state and the ROM… button |
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

### Where today's controls go

| Today | In D |
|---|---|
| Header, source bar | the header (Load Patch is the patch box's Browse…, the patch's name and what it misses are the patch box's); Load WAV is each OSC panel's Load…; OSC 1/2 goes, both OSCs show; On and Type are the OSC panel's |
| Perform column | each OSC panel's big knobs and its line (Robot under the pad, for the OSC the pad follows) |
| Oscillator tab | the OSC panels and their DETAILS |
| Envelopes, LFOs tabs | SELECTED for each block, and ENVELOPES and LFOs |
| COSM tab | the COSM panels and their DETAILS; LFO 3 and 4 in SELECTED; MOD is a block of its own; Structure is on the path |
| Amp & Pan tab | AMP and its DETAILS |
| Play tab | the PLAY page as it is |
| MSM, Matrix tabs | the MODULATION page; a block's Matrix routes in SELECTED too |
| Zones tab | the ZONES page as it is; the zones' ranges on the footer's keyboard |
| Effects tab | the FX page; ON or OFF of each block on the path |

### The steps

D is new code in files of its own (`Ui*`), beside today's editor, which stays as it stands until D takes its place: the
plugin keeps today's editor until the last step, and a test program of its own draws and checks D as it grows. Each step
is a commit `UI step n: …` on `ui-redesign`, with D's snapshots and its layout check.

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
4. The MODULATION, PLAY, ZONES and FX pages; the footer's keyboard with the zones.
5. Landing: the plugin's editor is D, today's editor files go, `TEST_UI_LAYOUT` and the plants hold D, these sections are
   the only description, a version number.

After that, each with the user's yes where it needs one: Inter and JetBrains Mono built in (a folder for font files); the
wave drawn from the loaded wave; the pad sending its controllers; the keyboard playing; dragging a source onto a knob to
make a Matrix route; the controls only a host reaches today (the OSC envelopes' velocity and key, the COSMs' Env B); the
VC-2 mode's pages.

## The editor as it stands (v144–v195)

The plugin's user interface follows this design from v144 on, by the user's decision (2026-11-04). It came as a tabbed editor cut from v130, and v144 merged it with the effects' UI that came after. New controls go where this says.

### The window (today)

960 × 600, resizable from 900 × 600 to 1400 × 900. The two card columns share any extra width. Top to bottom:

- **Header:** wordmark with the build under it, Effects switch, ROM status, ROM button (the Algorithm menu went with the legacy engine, v191).
- **Source bar:** Load WAV, Load Patch, the patch's name, the file's and what the patch asks for that isn't here, OSC 1/2, the OSC's On and Type. It stays in view on every tab, and the Load button and most tabs follow its OSC.
- **Perform column** (left, every tab): Pitch, Time, Formant, Robot. With a ROM these are the selected OSC's, and the title says which.
- **Tabs:**

| Tab | Cards |
|---|---|
| Oscillator | Wave (Vari and Playback Mode on its title line, Start Offset its last row), or Analog oscillator when Type is ANALOG · Loop · Tempo sync · Pitch · Level · Velocity |
| Envelopes | Amp (TVA) · Pitch · Time · OSC n Amp · Formant · COSM n |
| LFOs | LFO 1 or 2, by the OSC · Amp LFO (LFO 5) |
| COSM | COSM 1/2, On, Type · the type's own rows · LFO 3 or 4 · MOD · Structure |
| Amp & Pan | Patch level · Velocity · Pan |
| Play | Portamento · Keys · Arpeggio (On and Hold on its title line; Motif, Octaves, Velocity, Duration) · Tune · Tempo · Arpeggio pattern (Grid, End point, Shuffle and its Resolution; its title says when the pattern has no step to play) |
| MSM | Multi Step Modulator (On on its title line; Key sync, the Track shown) · Track n (On and Loop on its title line; Type, Grid, End point, Direction, Zone, Destination) · Track n steps 1–8 · steps 9–16 (v193; one track at a time, as the Matrix's Controls) |
| Matrix | Matrix, one Control at a time · Controller assign |
| Zones | Zones and their keys (across both columns: the keyboard's keys under the zones that play them, and a line a zone of its keys and what it plays, or why it is silent) · Zones 1-8 · Zones 9-16 (a slider a zone, its Range Upper key; v195) |
| Effects | MFX, Chorus, Reverb, System buttons · the block's card, or Tone and System on the System page · a scrolling list of the type's parameters, or the system's EQ |

### The look (today)

The palette is `namespace Ui` in `plugin/Source/UiParts.h`, with the sizes, the cards (`Ui::Page`) and the joined buttons (`Ui::Segmented`), which `PluginEditor` and `FxPanel` share:

| Token | Colour | Used for |
|---|---|---|
| `surfaceApp` | `1A1A2E` | window |
| `surfaceTitlebar` | `0D0D20` | header |
| `surfaceStrip` | `111128` | source bar, tab bar |
| `surfaceXt` | `15152E` | cards, Perform column |
| `surfaceField` | `0D0D1A` | value boxes, envelope views |
| `line` | `333355` | dividers, card borders |
| `accent` | `00BFA5` | the one interactive colour |
| `title` | `00FF88` | the wordmark |
| `text` / `label` | lightgrey / lightblue | values / names |

Card titles are 11 pt bold capitals in `label`. Row names are 11 pt bold, in full words: "Coarse", not "Crs".

Drawn text goes through `Ui::drawText` with a `Ui::TextStyle` (font, colour, where it sits, whether it is cut with dots), and a filled rectangle through `Ui::fill`: the cards' and the Perform column's titles (`Ui::titleFont`), the tabs and the Zones map share them (v195). The tab bar gives each tab its name's width and an equal share of what's left, 28 at most, so all ten fit at the smallest size.

### Adding a control

1. Build it in the constructor and attach its parameter, as now.
2. Put it in a card in `buildLayout()`: `Ui::Page::Card` holds a title, controls on the title line (`head`) and rows, and `lr(label, control)` makes the usual row. A card goes in column 0, column 1 or across both (`col = -1`). `sameAs` puts a card in another's place, and `fill` runs one to the page's bottom, for a list that scrolls. A page's `leftW` sets column 0's width. On the Effects tab, `FxPanel` is itself a `Ui::Page` and its cards are in `buildCards()`: its controls stay its direct children, which `TEST_FX_PANEL` relies on.
3. If it shows only in some states, set its visibility where the state changes (`showOscType`, `showCosmType`), and give its card `shown` if the whole card goes.
4. If it follows OSC 1/2 or COSM 1/2, its card's title must say so: `titleFn`, and `repaintPages()` where the selection changes. A card that says nothing reads as belonging to the whole patch.

**What `buildLayout()` doesn't place, it removes.** At its end it takes every child off the editor except those in the pages and its `chrome` list. A control left out compiles, and is simply gone. `TEST_UI_LAYOUT` reports that as a parameter with no control.

A page doesn't scroll, so cards must fit: at the smallest size a page is 466 high, and a card is 38 plus 30 a row, less 4. Only the Effects tab's list scrolls. A COSM type's rows go in the shared three-row area instead (`layoutCosmType`, `showCosmType`, and the card's `extra` list).

### Checks

- `TEST_UI_LAYOUT=1` (in `verify.sh`): at the smallest, default and largest sizes, on every tab, both OSC types, every COSM type, every Effects page and every effect type, no control is outside its parent, over another or without a size, none shows on a card that isn't shown (v155), and no text is cut short, the effects' list included, which must show at least 15 rows. Every control is moved, to see that it sets a parameter, and every parameter must have a control. Each tab is opened by a click, and a click at the tab bar's right end, at the smallest width, must open none (v195: the last tab ends inside the bar). The joined buttons are driven both ways. `=list` prints the parameters with and without a control.
- `mutate_panel.py --editor`: planted errors in the layout, to show that check catches them.
- `TEST_SNAPSHOT=file.png`, with `TEST_SNAPSHOT_TAB=n` and `TEST_SNAPSHOT_SIZE=WxH`: the editor drawn offscreen. It draws only the open tab. `TEST_SNAPSHOT_TYPE` and `TEST_SNAPSHOT_COSM2` open the COSM tab, and `TEST_SNAPSHOT_FX` the Effects tab.

### Not shown anywhere yet

92 of the 761 parameters have no control (v190's ten for the arpeggio, v193's 98 for the Multi Step Modulator and v195's sixteen splits all have one). Only a host can reach them (`hostOnly` in `PluginTest.cpp`):

- Scale Tune (13).
- The COSMs' Envelope B, and Envelope A's velocity and key follow (each COSM: 10 and 6).
- The COSMs' LFO offset, delay and fade (each COSM: 4).
- Each OSC's pitch, time and formant envelopes' velocity and key (each OSC: 18, v148). A patch sets them.
- The engine's Pitch, Time and Formant (3). Until v191 these showed on the Perform knobs without a ROM; with the engine
  gone they are kept, unread, for old sessions, and the Perform knobs set the selected OSC's with or without one.

### Still to do

- The Effects tab shows one block at a time, so you can't see at a glance which of MFX, Chorus and Reverb are on.
- Keep the open tab when the plugin's window closes. It goes back to Oscillator now.
- The envelope bars fill from the left, so a bipolar value of 0 looks half full. The envelope drawing is a sketch of the settings: it doesn't draw Depth or the real timing.
- The constructor's colours are still written as numbers, not `Ui` tokens.
- COSM 1/2 is chosen only on the COSM tab, though the COSM n envelope on the Envelopes tab follows it.
