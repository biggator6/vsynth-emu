# The next issues: the user's list (2026-11-27)

The user's list, given while they test the plugin (2026-11-27), in their words, with what the code has today (read at
vc2-mode 28a58c8), what each needs, and the decisions that are the user's (d-1 … d-9, each with a recommendation). Nothing
here has started. VC2_MODE_PLAN.md's step 6 (the V-Synth and the VC-2 as two plugins) is the other work waiting on the
user's testing.

**While the user tests.** Every build of the plugin installs into ~/Library (`COPY_PLUGIN_AFTER_BUILD TRUE` in both
targets, with no option to skip it), and verify.sh builds the plugin, so work that builds it changes what the user is
testing. What can run meanwhile is the firmware's side: reading rom.2 and rom.4, harnesses (`build.sh`'s `xt_*`, which
install nothing), and the test targets alone. The editor's side waits on the user's UI notes, so the editor changes once.
- **d-1. When.** Recommended: the firmware research of items 2–6 now, in sessions that build no plugin; the editor's work
  after the user's UI notes and step 6 (the split settles which plugin holds which parameter, and MIDI learn maps those).
- **d-2. A build that doesn't install** (an option, `VSE_INSTALL_PLUGINS` or the like, on by default, off in a worktree),
  so sessions can build and test the plugin while the user tests the installed one. Recommended: yes, as its own small
  version first, with verify.sh's installed-copies line saying "not installed (this build)" instead of failing.

## Minor changes

### 1. MIDI learn and a map of the controls
"midi learn/map of controls". Today: no MIDI learn. The XT's own CC receive is the firmware's and stays as it is (CC 1, 7,
10, 11, 64, the Matrix's CC sources, the Time Trip pad's pair; HARDWARE_FIRMWARE.md). Learn is the plugin's own layer:
a map of CC to parameter ID, set from a control (right-click: Learn, Forget), kept in the session, applied in processBlock
ahead of the firmware's MIDI, and shown as a list (the map).
- **d-3. A learned CC and the firmware.** Recommended: a learned CC is the plugin's alone and doesn't also reach the
  firmware, so one CC never does two things; an unlearned CC reaches the firmware as now.
- **d-4. Where the map lives.** Recommended: in the session first; a default map for new sessions later if wanted.

### 2. Controls that follow the values live
"controls should real-time reflect values. Move as control information or modulators are sent." Today: a parameter's
control follows the parameter (host automation moves it, through JUCE's attachments; to check for every kind of control).
What doesn't move it: a value the firmware changes from MIDI inside XtSynth (the XT's own CC receive changes the edit
values, as its display shows; which ones is the research), and the modulators (LFOs, envelopes, the Matrix, the Multi
Step Modulator), which move a value at run time without changing what's stored.
- Research first: which of the firmware's MIDI paths change a stored value (the edit buffer) and which only modulate.
- **d-5. What a control shows.** Recommended: a stored value changed by MIDI moves the control itself (and the host's
  parameter with it); a modulator shows as a ring or bar beside the control, so the control always says what's stored.
  UI_DESIGN.md takes the ring's design.

### 3. The arpeggiator's pattern
"arpeggiator only reads from patches - no ability to program patterns. with no pattern, it should still handle live input
for repeat/up/down/etc". Today: the ten settings are host parameters (PluginArp.cpp, v190), among them the motif, the
eight of the firmware's table (UP, DOWN, NOTE-ORDER, PHRASE, RHYTHM and the rest; XtArpeggioMotifs.cpp), which choose the
key a step plays. The pattern (16 lines × 32 steps: each line's State, NOTE or CTRL, and Note/CC, then each step REST,
TIE or a velocity) is a patch's data, kept in the session (`arpPattern`), drawn on the PLAY page (`UiArpPattern`) but not
editable.
- Research first, names first: the XT's own pattern edit (the manual's pages and what rom.2 does on each edit); how a
  line's Note and the motif share the keys held; what an empty pattern plays on the XT.
- The work: the pattern editable on its card, by the XT's rules, written to the session as a patch's is.
- **d-6. Live input with no pattern.** Recommended: the engine stays the firmware's; "repeat/up/down" come from the motif
  with starter patterns given as data (e.g. a step on every beat, a line a key), which the user picks and then edits. If
  the XT plays nothing with an empty pattern, an empty pattern does the same here.

### 4. A patch init
"need a patch init". Today: none. The XT has its own PATCH initialize (and rom.4 its own, by Patch Category, for the
VC-2), load-time code, so it runs in the interpreter as the loaders do (CLAUDE.md) and is applied as a patch is
(`loadPatch`). The work: find the routine (names first: the manual's Initialize), run it, a button in the patch box; with
step 6, each plugin its own init.

### 5. Start Offset in time, and a line on the wave
"start offset - change to ms/sec rather than samples - display a vertical line when editing". Today: Start Offset is
0…16777215 (2^24 − 1, the firmware's word), its text the firmware's, which the panel check holds with every other box's
(verify.sh: "5948 of the boxes' texts as the firmware's").
- Research first: the offset's unit in the firmware's seek (frames of the wave at its own rate, or of the encoded wave),
  so the time it shows is the time it plays from.
- **d-7. The text.** Recommended: the time (ms below a second, then s), from the wave's own rate, the stored value as
  now; the panel check takes Start Offset's text as the plugin's own, by name, with its law tested. **Decided (the
  user, 2026-11-27): ms, then s.** The rate is 44.1 kHz on every wave, the XT's (HARDWARE_FIRMWARE.md {#wave-edit-read}).
- The line: drawn on the OSC panel's wave display (`UiWaveDisplay`) while the control is held, so it rides with item 6's
  display.

## Improvements

### 6. A wave display with editing of start, end, loop and events
"need a wav display with editing of start/end/loop/events (eliminate the need for v-producer companion software)". Today:
the OSC panel's display (`UiWaveDisplay`, v200), the loaders (.vpw, the .w00 links), the encoders run in the interpreter,
the EVENT playback mode and SOLO's events. The XT's own wave edit (points, loop, truncate, encode) is its firmware's, so
it runs in the interpreter; the events' edit is V-Producer's, a PC program, so its laws come from its files and its
results (V-Trainer runs natively here), held to what the XT plays.
- Research first, names first: the XT's wave edit pages and their routines; where a wave's start, end, loop and events
  sit in a .vpw and a .w00; the XT's own writer for each.
- **d-8. Where an edit goes.** Recommended: into the session's copy first (the wave as loaded, edited, saved with the
  session), then written to a file by the firmware's own writer, as its own version. **Decided (the user, 2026-11-27):
  the session, then a file** by rom.2's writer (`0x89496590`, HARDWARE_FIRMWARE.md {#wave-edit-read}).
- **d-8a. Start and end** (asked when the research found the XT has no start or end point of a wave). **Decided (the
  user, 2026-11-27): start is the OSC's Start Offset on the display, end the XT's Edit Start/End with Truncate** cutting
  the session's copy; both loops (FWD, BWD) drawn and dragged.
- **d-9. The order.** Recommended: the points first (start, end, loop: the XT's own edit), then the events (V-Producer's),
  each a version or more; item 5's line comes with the first. **Decided (the user, 2026-11-27): the points, then the
  events**, which the research found are the XT's own edit too (its Encode screen's add and delete).
