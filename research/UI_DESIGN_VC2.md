# The VC-2 mode's pages: a draft (2026-11-22) {#vc2-pages}

A draft for the user to decide, not a decision: nothing here is built, and no editor code comes with it. Building the
pages is step 4.5's second half (VC2_MODE_PLAN.md, step 5), once step 4.3's `Vc2Synth` is whole. The open choices are
the d-n questions at the end ("The questions", {#vc2-questions}), each with a recommendation.

It is a file beside UI_DESIGN.md, not a section of it, for two reasons: it is longer than UI_DESIGN.md itself (a table of
IDs for every card of six pages), and it is a draft. Once the user has answered, what is decided goes into UI_DESIGN.md's
own sections as the pages are built, as D's design did (UI_DESIGN.md, "The steps", step 0), and this file goes.
UI_DESIGN.md's "The VC-2 mode's pages" points here until then.

The mock-up is `UI_DESIGN_VC2.html` beside it: four of the pages drawn at 1200 × 750 in D's palette (SYNTH on zone 2,
VOCODER with the mic silent, MODULATION, PLAY), a tab for each. FX and ZONES are D's own pages with rom.4's IDs and are
not drawn.

## What it rests on {#vc2-ground}

- **D** as UI_DESIGN.md describes it: the regions, the rack, the lower panel, the pages' cards, and "Adding a control".
  Its sizes, from the code: a knob is a cell 64 wide and 57 high (a 30 dial and two 13-high lines of text); a box is two
  cells (136) and 36 high; a switch is two cells and 24 high; cells and rows are 8 apart, a row as high as its tallest
  (`ControlGrid`). n cells take 72n − 8 across. A card is its body plus 56 (12 of padding at each edge, a 24 title, 8
  under it), and 24 wider than its body (`Card`). A page has 1200 × 628 under the page bar, 1176 × 604 inside its 12
  inset.
- **The set**: `vc2HostParams()` (PluginVc2Params.cpp), 1,332 parameters: the patch's 134, each zone's 590 (the tone's
  155, 13 envelopes of 10, 5 LFOs of 7, two COSMs of 135) for zones 1 and 2, and the system's 18. Their host IDs are
  `vc2_<hex>` for zone 1 and COSM 1, with `z2` for zone 2 and `c2` for COSM 2 (`vc2z2c2_927` is zone 2's COSM 2's
  first typed ID).
- **The names.** The XT's manual says nothing about the VC-2's parameters; it sends the reader to the V-Card owner's
  manual, which the repo doesn't hold (VC2_VOCAL_DESIGNER.md, "Open"). So what the VC-2 shares with the V-Synth takes
  the manual's names, as D's controls already do, and the VC-2's own (the vocoder block, the VOCODER OSC, Harm Formant,
  the Growl) take rom.4's names as `Vc2Params` reads them, shortened to fit a knob the way D shortens the V-Synth's.
- **A row is zone 1's ID.** Every table below gives zone 1's (and COSM 1's) rom.4 ID, as D's give OSC 1's names. The
  control is made for the selected zone and the panel's COSM by `vc2HostParamId(id, zone, cosm)`, as D's OSC and COSM
  panels are made by `Params::oscId` and `Params::cosmId` from an index. No table names zone 2 or COSM 2.
- **OSC 2's IDs** are OSC 1's plus 0x34 for 0x2e4…0x317, then their own runs: Detune and the sub oscillator +3
  (0x350…0x352), the VOCODER OSC's +0xe (0x362…0x36f), Harm Formant +5 (0x376…0x37a), the vibrato +4 (0x380…0x383),
  the second Wave Number +4 (0x38d). An envelope is 13 IDs on (0x390 + 13k), an LFO 8 (0x439 + 8k).

## 1. The pages {#vc2-page-list}

In V-SYNTH mode nothing changes: D's five pages and the V-Synth's 763 parameters stay as they are. In VC-2 mode the
header, the page bar and the footer are D's, and the pages under the page bar are the VC-2's, each made from its own
tables with D's parts:

| Page | What it holds | Parameters |
|---|---|---|
| SYNTH | the selected zone (1 or 2): the path, the rack and the lower panel, as the V-Synth's SYNTH page; the zone choice; the Time Trip column's Pad Mode and Pad Hold | 1,183 |
| VOCODER | the vocoder block (Vocoder Sw to Unvoice Spread) and the mic's state, with External In Type | 20 |
| MODULATION | the Matrix, the controller assigns and Beam Type | 55 |
| FX | the effects' switches, Types and levels, the System card, the patch EQ | 20 |
| PLAY | the arpeggio and its pattern, the patch's tune, keys, portamento, tempo and Category, Scale Tune | 38 |
| ZONES | the sixteen zones' Range Upper on the keys | 16 |

The page bar reads SYNTH, VOCODER, MODULATION, FX, PLAY, ZONES. What the VC-2 mode differs in, against D's V-Synth pages:

- **The header**: the mode switch at VC-2 (lit, no longer greyed); the patch box a `.p02`'s name (`vc2PatchName`), its
  Browse… loading a `.p02` (`loadVc2Patch`); the tempo the Patch Tempo, 0x13a; a MIC chip between the patch box and the
  tempo ("6. The mic's state"); the Effects switch and the ROM's state as they are, the state rom.4's.
- **No Multi Step Modulator**: rom.4 has none, so MODULATION has no MSM card.
- **No loaded waves**: a VC-2 OSC on PCM plays the VC-2's library by its Wave Number, so the OSC panel has no Load…
  and DETAILS no Key, Loop Start, Loop End or Original Tempo (the plugin's own settings for a loaded file).
- **The VOCODER page and block**: the vocoder takes the zones' tones and the mic, after AMP, before MFX (the part
  mixer: VC2_VOCAL_DESIGNER.md, "The part mixer, run"), so the path gets a VOCODER block there, which opens the
  VOCODER page as MFX opens the FX page.
- **The footer** is D's: its keys play the VC-2's part, and the zones' ranges above them are the patch's 0x141…0x150.

## 2. Each page's cards {#vc2-cards}

### SYNTH {#vc2-synth}

D's SYNTH page, for the selected zone.

**The zone choice.** Two chips at the path's left end, Z1 and Z2, the selected one filled in its zone's colour (the
footer's: zone 1 the accent, zone 2 `mod`), bound to Current Zone (0x140, ZONE1 or ZONE2; d-3). The path, the rack and
the lower panel are that zone's; the block selected stays selected across a change of zone.

**The path**: Z1 Z2, OSC 1, OSC 2, MOD, COSM 1, COSM 2, AMP, VOCODER, MFX, CHORUS, REVERB, and the Structure box
(0x2d3, shown 1…3). A COSM chip's setting is its type by the manual's short names (d-9); VOCODER's is its Type, OFF,
or why it is silent ("NO MIC", "NO NOTE"; "6. The mic's state").

**The Time Trip column** (184 wide, D's rows): the title with OSC 1 and OSC 2 to follow; the pad; one row of Pad Mode
(0x4a7, X-Y or TIME-TRIP, two halves of 40) and Pad Hold (0x4a8); the followed OSC's Time Trip Sw (0x316) and Beat Keep
(0x317). In the VC-2's set Pad Mode and Pad Hold are the patch's, so Pad Hold is the parameter, not the editor's own
as on the V-Synth, and the pad sends the pair Pad Mode names: Pad Assign X and Y under X-Y, Time Trip Assign X and Y
under TIME-TRIP, as the XT's own pad does (d-10).

**An OSC panel** has a face for each OSC Type: ANALOG 0, PCM 1, EXTIN 2, VOCODER 3. Each is D's shape: the box beside
Type, four big knobs (44), six small (30), PCM's line, and the display. IDs are OSC 1's:

| Face | Box | Big knobs | Small knobs | Display |
|---|---|---|---|---|
| ANALOG | Waveform 0x2f1 | Pitch 0x2e7, Pulse W 0x2f3, Impact 0x2f2, Level 0x2ee | Coarse 0x2eb, Fine 0x2ec, PW KF 0x2f4, PW LFO 0x2f5, PW Env 0x2f6, Vel Sens (Amp env's 0x3be) | three cycles, as D's |
| PCM | Wave 0x2fe | Pitch 0x2e7, Time 0x307, Formant 0x30c, Level 0x2ee | Coarse 0x2eb, Fine 0x2ec, Time KF 0x308, Energy 0x315, Vel Sens (0x3be), Pitch KF 0x2e8 | the wave, its name from the VC-2's library |
| EXTIN | none | Pitch 0x2e7, Harm Fmt 0x371, Coarse 0x2eb, Level 0x2ee | Fine 0x2ec, Harm KF 0x372, Harm Style 0x375, Pitch KF 0x2e8, Gain 0x2e6, Vel Sens (0x3be) | the mic's last cycles, or why there are none |
| VOCODER | none | Pitch 0x2e7, Balance 0x35f, Growl 0x35b, Level 0x2ee | Coarse 0x2eb, Fine 0x2ec, Saw 0x355, Saw LPF 0x354, Noise 0x358, Noise LP 0x356 (Saw Wave Level and LPF cutoff, Noise Level and LPF cutoff, named short to fit 47) | the carrier: the saw's and the noise's bands by their cutoffs and levels, under the balance |

PCM's line is Vari 0x314, Playback Mode 0x312 and Tempo Sync 0x310, as D's. Vel Sens is the OSC's Amp envelope's
Velocity Sens, as on D (its home is that envelope's tab; 0x397 + 13·3 for OSC 1). The Pitch, Level, Coarse and Fine on
every face are one parameter each, a control on each face, as D's Pitch is on both of its faces.

**A COSM panel** has a face for each value of COSM Type (0x463, 0…16), each its type's own IDs (rom.4's typed IDs, in
the second descriptor bank), the box beside Type, up to five knobs (30) and the display, as D's `kFaces`; DETAILS has
the rest. The title holds COSM Switch (0x462) and Type.

| Type | IDs | Box | Knobs | DETAILS |
|---|---|---|---|---|
| 0 THRU | none | | | "THRU" on the display |
| 1 OD/DS | 0x927…0x92f | Type 0x927 | Drive 0x929, Tone 0x92c, Level 0x928, Drive Env 0x92b, Drive LFO 0x92a | Tone KF 0x92d, Tone LFO 0x92e, Tone Env 0x92f |
| 2 W-SHAPE | 0x930…0x938 | Type 0x930 | Drive 0x932, Tone 0x935, Level 0x931, Drive Env 0x933, Drive LFO 0x934 | 0x936, 0x937, 0x938 (Tone KF, LFO, Env) |
| 3 AMP | 0x939…0x940 | Amp Type 0x939 | Gain 0x93e, Bass 0x93a, Mid 0x93b, Treble 0x93c, Level 0x93d | Gain LFO 0x93f, Gain Env 0x940 |
| 4 SPEAKER | 0x941 | Speaker Type 0x941 | none | a line: nothing more |
| 5 RESONATOR | 0x942…0x949 | Filter Type 0x942 | Size 0x943, Balance 0x947, KF 0x944, Env 0x946, LFO 0x945 | Bal LFO 0x948, Bal Env 0x949 |
| 6 SBF1 | 0x94a…0x957 | none | HPF 0x94a, LPF 0x94b, Detune 0x94f, Width 0x953, KF 0x94c | Octave 0x94e, Sub 0x94d, Det LFO 0x950, Det Own 0x951, Det Env 0x952, Width KF 0x954, Wid LFO 0x955, Wid Own 0x956, Wid Env 0x957 |
| 7 SBF2 | 0x958…0x964 | none | Q 0x958, LPF 0x959, Detune 0x95c, Width 0x960, KF 0x95a | Octave 0x95b, Det LFO 0x95d, Det Own 0x95e, Det Env 0x95f, Width KF 0x961, Wid LFO 0x962, Wid Own 0x963, Wid Env 0x964 |
| 8 GROWL | 0x965…0x96b | none | Intensity 0x965, Speed 0x966 | 0x967…0x96b, dimmed (d-6) |
| 9 DUAL | 0x96c…0x976 | Filter Type 0x96c | Freq 1 0x96d, Q 1 0x96f, Freq 2 0x972, Q 2 0x974, KF 1 0x96e | LFO 1 0x970, Env 1 0x971, KF 2 0x973, LFO 2 0x975, Env 2 0x976 |
| 10 TVF | 0x977…0x97e | Filter Type 0x977 | Cutoff 0x97b, Reso 0x978, KF 0x97c, Env 0x97e, LFO 0x97d | Reso Env 0x97a, Reso LFO 0x979 |
| 11 DYN-TVF | 0x97f…0x988 | Filter Type 0x97f | Cutoff 0x984, Reso 0x980, KF 0x985, Env 0x988, LFO 0x986 | Reso LFO 0x981, Reso Own 0x982, Reso Env 0x983, Cut Own 0x987 |
| 12 COMP | 0x989…0x98b | none | Attack 0x989, Sustain 0x98a, Level 0x98b | a line: nothing more |
| 13 LIMITER | 0x98c…0x990 | Ratio 0x98d | Thresh 0x98c, Attack 0x98e, Release 0x98f, Level 0x990 | a line: nothing more |
| 14 F-SHIFT | 0x991…0x997 | none | Depth 0x991, Balance 0x995, KF 0x992, Env 0x994, LFO 0x993 | Bal LFO 0x996, Bal Env 0x997 |
| 15 LO-FI | 0x998…0x9a0 | none | Bit 0x998, Rate 0x999, Filter 0x99a, Balance 0x99e, KF 0x99b | Filt LFO 0x99c, Filt Env 0x99d, Bal LFO 0x99f, Bal Env 0x9a0 |
| 16 TB-FILTER | 0x9a1…0x9ab | none | Cutoff 0x9a1, Q 0x9a5, KF 0x9a2, Env 0x9a4, Gain 0x9ab | LFO 0x9a3, Q Follow 0x9a6, HPF 0x9a7, HPF KF 0x9a8, HPF LFO 0x9a9, HPF Env 0x9aa |

The 15 types the VC-2 shares with the V-Synth have D's faces, knob for knob, by rom.4's IDs. The GROWL is the one new
face: it is in the Comb Filter's place, and of its seven IDs only Intensity and Speed reach the sound (Vc2CosmGrowl.h:
"the type's other five parameters do nothing"). Its display draws the growl's rate, 5…110 Hz by Speed, as a wobble, its
depth Intensity.

**AMP**: Level 0x2da, Pan 0x2dd, MFX 0x2e0, Cho 0x2e1, Rev 0x2e2, five knobs (24) with the name and value beside each,
where D's has four. MFX Send is the one added: the part mixer takes the vocoder's output to the MFX's input at zone 1's
MFX Send, so on the VC-2 it is the vocoder's level (d-13).

**The lower panel** is D's, the list SELECTED, DETAILS, ENVELOPES, LFOs:

- **SELECTED, envelopes.** The envelopes are each block's, in the order the V-Synth's are (XtPatch.cpp: OSC n's are
  4n…4n+3, Pitch, Time, Formant, Amp; COSM 1's 8 and 9, A and B; COSM 2's 10 and 11; the TVA's 12). An envelope k's
  bars: Attack 0x390+13k, Decay 0x392+13k, Sustain 0x393+13k, Release 0x395+13k, Velo Curve 0x396+13k, Velo
  0x397+13k, A-, D- and R-Velo 0x398…0x39a+13k, Time KF 0x39b+13k; and the Depth bar, the block's: Pitch's 0x2ea,
  Time's 0x30a, Formant's 0x30f (on ANALOG PW Env 0x2f6 and Fat Env 0x2fa, which the panel and DETAILS hold, as on D).
- **SELECTED, LFO.** OSC n's LFO n, COSM n's LFO 2+n, AMP's LFO 5, as D's: Wave 0x439+8k, Rate 0x43b+8k, Offset
  0x43c+8k, Delay 0x43d+8k, Fade Mode 0x43e+8k, Fade 0x43f+8k, Key Sync 0x440+8k.
- **SELECTED, what reaches the block.** The LFO's depths: an OSC's Pitch 0x2e9 and Level 0x2f0 on every face, Time
  0x309 and Formant 0x30e on PCM; AMP's Level 0x2dc and Pan 0x2df. The other faces' own depths are on their panel and
  DETAILS, as a COSM's are on D. The Matrix's routes landing on the block, as D's.
- **DETAILS**, by face (OSC 1's IDs):

| Kind and face | DETAILS |
|---|---|
| OSC, ANALOG | Fat 0x2f7, Fat KF 0x2f8, Fat LFO 0x2f9, Fat Env 0x2fa, Detune 0x34d, Sub Octave 0x34e, Sub Level 0x34f, Pitch KF 0x2e8, Random 0x2ed, Level KF 0x2ef, Gain 0x2e6, Vibrato Type 0x37c, Rate 0x37d, Depth 0x37e |
| OSC, PCM | Time Offset 0x30b, Fmt KF 0x30d, Start 0x306, Loop 0x313, Robot 0x311, Random 0x2ed, Level KF 0x2ef, Gain 0x2e6, Wave Start 0x37f (dimmed, d-6), Wave No. 2 0x389 (dimmed, d-6), Vibrato 0x37c…0x37e |
| OSC, EXTIN | Random 0x2ed, Level KF 0x2ef, Vibrato 0x37c…0x37e, Harm LFO 0x373 and Harm Env 0x374 (dimmed, d-6) |
| OSC, VOCODER | Noise HPF 0x357, Dyn Thresh 0x359, Dyn Sens 0x35a, Growl KF 0x35c, Growl LFO 0x35d, Growl Env 0x35e, Bal LFO 0x360, Bal Env 0x361, Pitch KF 0x2e8, Random 0x2ed, Level KF 0x2ef, Gain 0x2e6, Vibrato 0x37c…0x37e |
| MOD | MOD 0x2d4, Type 0x2d5, Level 0x2d6, Attack 0x2d7, Release 0x2d8 |
| COSM | the type's DETAILS, in the table above |
| AMP | TVA 0x2d9, Level KF 0x2db, Pan KF 0x2de, Output 0x2e3 |

  The vibrato is read under every Type (VC2_VOCAL_DESIGNER.md, "The VOCODER OSC's IDs, run"), so every face's DETAILS
  has it. Which envelope the VOCODER's Growl Env and Bal Env ride on is kind 5's binding, step 4.3's to say; until then
  they are the face's own, in DETAILS, as a COSM's depths are.
- **ENVELOPES** (13 tiles) and **LFOs** (5), as D's.

### VOCODER {#vc2-vocoder}

The patch's vocoder block and the mic, the same for both zones. In the mock-up's layout:

- **MIC IN**, down the left (340 × 604): External In Type (0x7a, STEREO, MONO-L+R, MONO-L, MONO-R) at its top, which
  channel of the plugin's input the mic is; then the state ("6. The mic's state"): the input's level, VOICED or
  UNVOICED, the tracked pitch and whether it is locked, Auto Note's note, the notes sounding, the output's level, and
  the checklist with its status line.
- **VOCODER**, at the top right (828 × 300): its title line holds Vocoder Type (0x194, LEGACY or HIRESO, a box) and
  Vocoder Sw (0x193); its body the display of the bands (480 × 244; d-14) and a grid of four cells across: Sound
  0x196, Depth 0x197, Direct 0x195, Robot 0x1a3 (robot human depth); Attack 0x19a, Release 0x19b (Env Attack and
  Release), Stereo 0x1a4 (Stereo Process Sw).
- Under it, four cards across, 201 wide each:
  - **FORMANT**: Shift 0x19f, Template 0x1a0, and the warp drawn (each side's ratio about its pivot, as HIRESO's slot 4
    maps it: `vc2HiresoMapOf`).
  - **CONSONANTS**: Detect 0x198 (Unvoice Detect), Level 0x199 (Unvoice Level), Spread 0x1a5 (Unvoice Spread switch).
  - **KEYS & HOLD**: Key Mode 0x1a2 (Modulator Key Mode, NORMAL or ALWAYS-ON), Split 0x1a1 (Split Point, as a note),
    Dump 0x19e (Hold Dump Time).
  - **AUTO NOTE**: Auto Note 0x19c (On Switch), Retrigger 0x19d.

LEGACY and HIRESO share the page (d-2): there is one block of IDs and Vocoder Type picks the engine. What the chosen
engine doesn't read is dimmed with a word saying which engine reads it. By the ports as they stand (each engine's slot
5, `Vc2Legacy::settings` and `Vc2Hireso::settings`, and `Vc2Vocoder`'s settings path): LEGACY reads Formant Shift's
first side and Hold Dump Time; HIRESO reads Env Attack and Release, Formant Template, the second side's pivot and Depth;
the consonants' level is Unvoice Level × Depth only under HIRESO. Which controls dim is to be held to the vocoder
converter's settings once `Vc2Synth` wires it, not to this list.

Two of the block's values are out of their descriptors' ranges, and stay so: Modulator Key Mode's HOLD and Auto Note
On Switch's third, empty state (VC2_VOCAL_DESIGNER.md, "The patch's vocoder block"). The controls give each
descriptor's range, as the host parameters do.

### MODULATION {#vc2-modulation}

- **MATRIX**, down the left (640 wide, where D's Multi Step Modulator is): D's Matrix view, its eight Controls as rows,
  Control n's Source 0x16a+5n, Destination 1 0x16b+5n, Sens 1 0x16c+5n, Destination 2 0x16d+5n, Sens 2 0x16e+5n, a row
  made for each Control by index. The destinations are rom.4's list (0…78), its text rom.4's.
- **CONTROLLER ASSIGNS**, at the right (528 wide): fifteen boxes, three across: Beam 1 L 0x89 … Beam 4 R 0x90, Pad X
  0x91, Pad Y 0x92, Trip X 0x93, Trip Y 0x94, Knob 1 0x95, Knob 2 0x96, and Beam Type 0x4a6.

### FX {#vc2-fx}

D's FX page, `FxPanel`, on rom.4's description (`XtFxValueText` already reads either image's): its four pages MFX,
Chorus, Reverb and System.

- **MFX**: its switch 0x13d, Type 0x1b7 (42 types), Master 0x1b8, To Chorus 0x1b9, To Reverb 0x1ba. **Chorus**: 0x13e,
  Type 0x23c (9), Master 0x23d, To Reverb 0x23e. **Reverb**: 0x13f, Type 0x270 (15), Master 0x271.
- **The types' own parameters** in the list at the right, edits by ID as the V-Synth's are, not host parameters (the
  user's scope of 2026-11-21: VC2_MODE_PLAN.md, step 5).
- **System**: the System card, Master 0x40 and Output Gain 0x78; and the PATCH EQ card in the list's place (d-5): EQ 1
  Freq 0x1af, Q 0x1b0, Gain 0x1b1, EQ 2 Freq 0x1b2, Q 0x1b3, Gain 0x1b4, with the two bands' curve over them. On rom.4
  the system's EQ has no mid bands, and EQ 1 and 2 are the patch's (VC2_MODE_PLAN.md, step 4.4).
- D's Tone card (the TVA's sends, Output Assign and Mode) isn't repeated here: on the VC-2 they are each zone's, on
  AMP. External In Type is on the VOCODER page's MIC IN card (d-4).

### PLAY {#vc2-play}

- Down the left (640): **ARPEGGIO**, D's grid: Arpeggio 0x4a9, Hold 0x4aa, Motif 0x4ab, Octaves 0x4b0, Velocity
  0x4af, Duration 0x4ac, Grid 0x4b1, End Point 0x4b2, Shuffle 0x4ad, Resolution 0x4ae; then **ARPEGGIO PATTERN**, the
  patch's data, drawn and not edited as on D (its lines from the `.p02`'s store).
- Down the right (528):
  - **PATCH & TEMPO**: Category 0x12c, Clock 0x43 (Clock Source), Tempo 0x13a.
  - **PORTAMENTO**, its switch 0x132 on its title line: Mode 0x133, Type 0x134, Time 0x135, Velo Sens 0x136.
  - **KEYS & TUNE**: Voices 0x130 (Mono/Poly), Legato 0x131, Octave 0x12f, Coarse 0x12d, Fine 0x12e, Bend Up 0x13b,
    Bend Down 0x13c.
  - **SCALE TUNE**, its switch 0x151 on its title line: twelve upright bars, one a key, C 0x153 to B 0x169 every other
    ID, each ±100 cents from its middle, the MSM steps' bars from D's MODULATION page made for twelve (d-7).

### ZONES {#vc2-zones}

D's ZONES page: the keys' card, the 128 keys with each zone's range, and the zones' card, sixteen cells, zone n's Range
Upper 0x141+n-1 as its bar (d-11). Zones 1 and 2's cells say they play the SYNTH page's zone; zones 3 to 16's what the
`.p02` holds for them, which is the patch's data in the user's scope, or no patch, silent.

## 3. The count {#vc2-count}

Every one of the 1,332 is on one page, its home, against `vc2HostParams()`'s runs (PluginVc2Params.cpp, `kPatch`,
`kZone`, `kCosm`, `kSystem`):

| Page | The patch's | Zone 1 | Zone 2 | The system's | All |
|---|---|---|---|---|---|
| SYNTH | 3 (0x140, 0x4a7, 0x4a8) | 590 | 590 | 0 | 1,183 |
| VOCODER | 19 (0x193…0x1a5) | | | 1 (0x7a) | 20 |
| MODULATION | 41 (0x16a…0x191, 0x4a6) | | | 14 (0x89…0x96) | 55 |
| FX | 18 (0x13d…0x13f, 0x1af…0x1b4, 0x1b7…0x1ba, 0x23c…0x23e, 0x270, 0x271) | | | 2 (0x40, 0x78) | 20 |
| PLAY | 37 (0x12c…0x136, 0x13a…0x13c, 0x151, the 12 Scale Tunes, 0x4a9…0x4b2) | | | 1 (0x43) | 38 |
| ZONES | 16 (0x141…0x150) | | | | 16 |
| All | 134 | 590 | 590 | 18 | 1,332 |

The patch's 134 are `kPatch`'s runs whole: 11 + 24 + 12 + 40 + 19 + 6 + 4 + 3 + 2 + 13. A zone's 590 are the tone's 155
(Structure 1, MOD 5, AMP 11, each OSC 69: 2 on its title, 14 on every face, ANALOG's 13, PCM's 21 with the Time Trip
column's two, EXTIN's 5, VOCODER's 14), the envelopes' 130, the LFOs' 35 and each COSM's 135 (Switch and Type, and the
133 typed IDs, the sixteen types' 9, 9, 8, 1, 8, 14, 13, 7, 11, 8, 10, 3, 5, 7, 9 and 11).

None is left for a later page. As on D, a few have a second control on their own page (the faces' shared Pitch, Level,
Coarse and Fine; Vel Sens on a panel and its envelope's tab; ANALOG's PW Env and Fat Env on the tab too); none is on
two pages. 36 are shown dimmed if d-6 goes that way: the Growl's five on both COSMs of both zones (20), Harm Formant's
LFO and Envelope Depths (8), Wave start offset (4) and the second Wave Number (4).

## 4. The fit {#vc2-fit}

Each page against 1176 × 604, by D's sizes ("What it rests on"):

| Page | Card | Width (body) | Controls, cells | Rows | Height |
|---|---|---|---|---|---|
| SYNTH | the rack and the lower panel | D's | every face D's slots: a box, 4 big, 6 small; a COSM's box and 5 | D's | D's 304 and 272 |
| SYNTH | the path | 1200 | 11 chips, the Structure box | 1 | 52; about 1,100 with common settings, up to about 1,220 with the longest, by D's chip rule (to measure) |
| SYNTH | AMP | 348 × 56 | 5 knobs of 24, 69 each | 1 | D's |
| SYNTH | DETAILS, the most (PCM) | about 1,000 | 16 cells | 2 of 14 | 122 of about 230 |
| VOCODER | MIC IN | 340 (316) | a box, the meters, the checklist | | 604 |
| VOCODER | VOCODER | 828 (804) | a box and a switch on the title; display 480; 7 controls, 8 cells, 4 across | 2 | 56 + 244 = 300 |
| VOCODER | FORMANT, CONSONANTS, KEYS & HOLD, AUTO NOTE | 201 (177) each | 2, 3 (4 cells), 3 (4 cells), 2 switches; 2 cells across | 2 each | 221, 145, 157, 112 |
| VOCODER | the right column | | | | 300 + 8 + 221 = 529 of 604 |
| MODULATION | MATRIX | 640 | 8 rows of 5 | 8 | 56 + 16 + 8 × 30 = 312 |
| MODULATION | CONTROLLER ASSIGNS | 528 (504) | 15 boxes, 3 across | 5 | 56 + 5 × 36 + 4 × 8 = 268 |
| FX | D's FxPanel | D's | the System card 2 rows, PATCH EQ 6 rows | | as D's System page |
| PLAY | ARPEGGIO | 640 (616) | 10 controls, 15 cells, 8 across | 2 | 56 + 122 = 178 |
| PLAY | ARPEGGIO PATTERN | 640 | the 16 lines | | 604 − 178 − 8 = 418, as D's |
| PLAY | PATCH & TEMPO | 528 (504) | 3 controls, 4 cells, 7 across | 1 | 113 |
| PLAY | PORTAMENTO | 528 (504) | switch on the title; 4 controls, 6 cells | 1 | 113 |
| PLAY | KEYS & TUNE | 528 (504) | 7 controls, 9 cells | 2 | 178 |
| PLAY | SCALE TUNE | 528 (504) | switch on the title; 12 bars, 42 each | 1 | 56 + 96 = 152 |
| PLAY | the right column | | | | 113 + 113 + 178 + 152 + 3 × 8 = 580 of 604 |
| ZONES | D's | | 16 cells | 2 of 8 | D's |

Two places are close and want `VSynthEmuEditorTest`'s measure rather than this estimate. The path: a chip is as wide
as its longer line plus 26, so D's V-Synth path is about 1,090 at its longest (two COSMs on "Frequency Shifter" or
"Speaker Simulator"); the VC-2's COSMs show the manual's short names (d-9), about 90 less, and the zone chips and the
VOCODER chip add about 200, so about 1,220 at the longest settings against 1,200. If it doesn't fit, the Structure
box loses the 70-wide label drawn beside it (the box keeps its name as a tooltip), or the zone chips move to the page
bar (d-3's other answer). And AMP's five names beside their knobs, 69 wide each.

## 5. A zone, a COSM and a face as the selected one {#vc2-selection}

- **The zone** is the selected one, never a number in the logic: each rack panel and each lower view is made for "the
  zone", and its controls are attached to `vc2HostParamId(id, zone, cosm)` of the row's zone 1 ID. A change of zone
  makes the panels' controls again for the other zone's parameters (D makes a panel once; here the attachments are
  made again, so one set of controls stands, not two with one hidden). The Z1 and Z2 chips are Current Zone (0x140)'s
  control, so the zone is the patch's: a `.p02` opens on the zone it was saved on, as the XT's panel does (d-3).
- **A COSM** is the panel's index, as D's: COSM n's controls are `vc2HostParamId(id, zone, n)`.
- **A face** follows its Type as D's `FacedPanel` does: every face's controls made, the one its Type's value names
  shown, on the editor's timer. An OSC's face is by OSC Type's value (0…3); a COSM's by COSM Type's (0…16), not D's own
  order of the V-Synth's types, whose COSM Type parameter is the plugin's choice list. A value past the faces shows
  none, with a line saying so.
- **The control's kind.** The VC-2's parameters are integers to a host (v451: the set fixed with or without rom.4), so
  D's rule (a choice a box, a switch a switch) can't tell them apart. The draft takes the kind from rom.4's value text
  (d-8): a value text of two names, OFF and ON or BYPASS and ON, a switch; other names a box; a range a knob, or a bar
  on an envelope. Without rom.4 every one is a stand-in, so a knob.

## 6. The mic's state {#vc2-mic}

A silent VC-2 says why, in three places (d-12):

- **The header's MIC chip**, on every VC-2 page, between the patch box and the tempo (about 84 wide): a dot and the
  input's level, the dot `ready` while the vocoder is heard, in the warning colour (the patch box's, for what a patch
  misses) when it is silent for a reason, the reason as the chip's tooltip. A click opens the VOCODER page.
- **The path's VOCODER chip**: under its name, the Type when it plays, or the reason in a word (OFF, NO INPUT, NO MIC,
  NO NOTE, LEVEL 0).
- **The VOCODER page's MIC IN card**: the input's meter (dBFS), VOICED or UNVOICED, the tracked pitch as a note and its
  lock, Auto Note's note, the notes sounding, the vocoder's output level, and a checklist, each line ticked or not, the
  first one not ticked said in full on the status line under it:

| Check | Silent because | Read from |
|---|---|---|
| Vocoder | Vocoder Sw (0x193) is OFF: only the Direct Sound passes | the parameter |
| Input | the host gives the plugin no input bus | the processor's `hasInput()` |
| Mic signal | the input is quiet on the channels External In Type takes (under −60 dBFS for a second) | the input stage's largest peak of the last eight blocks (`Vc2Unvoiced`'s, rom.4's 0x899d9a5c) |
| Carrier | no note sounds (hold a key, set Modulator Key Mode ALWAYS-ON, or turn Auto Note on) | the part's sounding voices (`Vc2PartVoices`) |
| Level | Vocoder Sound Level or zone 1's MFX Send is 0 | the parameters |

When every line is ticked the status says "Vocoding" and the Type. The checks are the editor's, on a small view the
processor gives it of `Vc2Synth` (the peak, the voiced flag, the tracked period and lock, Auto Note's note, the
voices sounding, the output's peak and the engine's bands for the display), read lock-free on the editor's timer as D
reads the loaded wave's view (`PluginWaveView.cpp`). That view is step 4.5's second half, once step 4.3's `Vc2Synth` is
whole. "Mode off" in the sense of the V-Synth mode chosen needs no line: the VC-2's pages show only in VC-2 mode.

## 7. What building it takes (for step 4.5's second half) {#vc2-build}

- The tables: the VC-2's faces, DETAILS faces, block kinds and page cards as D's are (`kFaces`, `UiDetailTables`,
  `UiBlockKinds`, `Pages::CardRows`), their rows rom.4's IDs, made through `vc2HostParamId`. The V-Synth's tables
  stay as they are; the VC-2's are tables of their own.
- The control's kind from rom.4's text (d-8), and the values rom.4's routine has no format for on their strings' scale
  (d-9), in the editor's text, not in the parameters'.
- The view of `Vc2Synth`'s state for the mic's checks and the bands.
- `VSynthEmuEditorTest`: it stops holding the VC-2's parameters apart (UI_DESIGN.md, "Not shown anywhere"); in VC-2
  mode every page and block at three sizes, every parameter reached (none left, but the dimmed ones if d-6 says
  host-only), each zone's, COSM's and OSC's controls moving their own parameters alone, after a change of zone too,
  and the mic's checks each failing on its own cause. `mutate_panel.py --editor` gets the VC-2's plants.

## The questions {#vc2-questions}

Each with its recommendation first.

- **d-1, the pages.** Recommended: six in VC-2 mode, SYNTH, VOCODER, MODULATION, FX, PLAY, ZONES, the editor opening on
  SYNTH as for the V-Synth. Or five, with MODULATION's Matrix and assigns on PLAY (the VC-2 has no Multi Step Modulator,
  so MODULATION is light), or VOCODER first, opening there.
- **d-2, HIRESO and LEGACY.** Recommended: one VOCODER page for both, the controls the chosen engine doesn't read dimmed
  with a word naming the engine that does. Or every control live whatever the Type, or a tab for each engine.
- **d-3, the zone choice.** Recommended: Z1 and Z2 chips at the path's left end, in the zones' colours, bound to
  Current Zone (0x140), so the zone is the patch's. Or the editor's own choice kept in the session, Current Zone a box
  on PLAY; or the chips in the page bar.
- **d-4, the system's settings.** Recommended: each where what it does is: Master Level and Output Gain on FX's System
  card, Clock Source on PLAY, External In Type on the mic's card, the assigns on MODULATION. Or all 18 on one SYSTEM
  card, on FX's System page, as the V-Synth's FX page holds Master, Output Gain and External In Type together.
- **d-5, the patch EQ.** Recommended: on FX's System page, where the V-Synth's EQ is, as the effect chip runs it. Or on
  the VOCODER page, beside the block it follows in the patch.
- **d-6, the parameters nothing reads.** Recommended: shown in DETAILS, dimmed, with a line saying rom.4 holds them and
  nothing found reads them (the Growl's five, Harm Formant's LFO and Envelope Depths, Wave start offset, and the second
  Wave Number until its reader is known): 36 in all. Or host-only, as the V-Synth's 17 are.
- **d-7, Scale Tune.** Recommended: a card on PLAY, twelve upright bars and the switch on its title line. Or host-only,
  as the V-Synth's 13 are.
- **d-8, a control's kind.** Recommended: from rom.4's value text (OFF/ON or BYPASS/ON a switch, other names a box, a
  range a knob). Or a kind written in each table row.
- **d-9, the values rom.4 can't write.** Recommended: on their strings' own scale, as the seven reverb values were put
  on the manual's: COSM Type by the manual's names with GROWL in COMB's place, Structure 1…3, Split Point as a note,
  the patch EQ in Hz, Q and dB, Beam Type OFF and 1…4, Wave Number by the VC-2's library's names, Harm Formant Style
  bare. Or bare, as rom.4's routine gives them.
- **d-10, the pad.** Recommended: Pad Mode and Pad Hold in the Time Trip column, the pad sending the pair Pad Mode names.
  Or the pad always sending Time Trip Assign X and Y, as on the V-Synth, with Pad Mode a box on MODULATION.
- **d-11, ZONES.** Recommended: sixteen cells, as on the V-Synth, zones 3 to 16 the patch's data. Or two cells, the
  zones the VC-2's patches use.
- **d-12, the mic's state.** Recommended: in three places, the header's MIC chip, the path's VOCODER chip and the MIC IN
  card's checklist. Or on the MIC IN card alone.
- **d-13, MFX Send on AMP.** Recommended: a fifth knob on AMP, as it is the vocoder's output level. Or in AMP's DETAILS,
  AMP keeping D's four.
- **d-14, the VOCODER card's display.** Recommended: the engine's bands as they play (LEGACY's 10, HIRESO's 32), the
  mic's envelope in each band and the output's, which needs `Vc2Synth` to hand them out. Or a picture of the settings
  alone (the levels and the formant warp), which needs nothing from it.
