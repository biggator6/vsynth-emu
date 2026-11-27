# Firmware facts worth knowing

Moved here as they stood from CLAUDE.md (2026-11-23, the user's "Yes, all five"). Read before working on XtSynth, a patch's load, the effects or a COSM type.

## Firmware facts worth knowing
- Applying a patch (HARDWARE_FIRMWARE.md, "Applying a patch"; `XtPatch`, `VSynthEmuProcessor::loadPatch`): a new `Controls` field read from a patch needs its line there as well as in `patchOf`, and `setControls` beside `xtControls`. A wave's own fine tuning (`vplp` +0, up to 14 semitones in real waves) is on `XtEncoded`, not a control: it reaches the pitch, the root, the formant ratio and the walk. A patch holds only its type's parameters, read by slot (`xtPatchTyped`). An OSC's external input reads as EXT IN from v206 (OSC Type 2), and `xtPatchMissing`, which says what a patch asks for that isn't here, names nothing now. A patch's Multi Step Modulator (v193) is its common's (`xtMsmSettingsOf`), and its load stops it (`XtSynth::msmPatch`, the firmware's `0x894d9950`) where an edit only reads it. A patch's arpeggio goes on with its tone (v190: `xtWithPatchArp` in applyPatch, its settings onto the parameters and its pattern into the session's state).
- Zones (v154; HARDWARE_FIRMWARE.md, "Zones"; the user's decision, 2026-11-05): zones 2–16 are a patch's data (`XtSynth::Zones`, `setZones`; `loadPatch` encodes their waves on the pool), zone 1 the parameters and editor. A note's zone is the first of the part's table of upper keys (IDs 0x124…, `0x894794f0`) its key isn't above; a legato note keeps its zone. The splits (v195; HARDWARE_FIRMWARE.md, "The zones' splits on the panel") are the patch's common, `Controls::zoneUpper`, sixteen host parameters and the Zones tab: the table is made again at each update (`0x894dbfa0`), so a moved split reaches the next note and a sounding one keeps its zone. Every patch loads all sixteen zones; a zone with no patch's data is silent, the XT's init zone. A zone's IDs (0x293…0x3e8, the COSMs') are read through objects its routines point at its block; the rest is the patch's common, zone 1's (`zoneControls`). The free-running LFOs, a key-unsynced LFO's following and the MOD's block are by zone. Everything a voice reads goes through `zc(v)`: a voice reading `ctl` is a zone bug. `xt_tone`'s getter answers by the tag the eight routines leave in the object; its zone cases are the last section.
- V-Synth's files (HARDWARE_FIRMWARE.md, "V-Synth's files"; `XtFiles`): a `.vpw`'s audio is RDAC-packed, unpacked by the firmware's block routine, and the plugin plays one. A `.p00` is in a file layout: the firmware's defaults under it, then `0x8947dae0`, give the store the descriptors (12 bytes an ID: offset, width, bit, centre, default, min, max) read. A patch is applied by `VSynthEmuProcessor::loadPatch` ("Applying a patch", above). The disk image's files are fragmented, so go through its FAT; 416 preset waves follow its partition, in no file. The arpeggio pattern (IDs 0x449…0x48c) is 16 lines of 34 bytes from store 0x128c, read by line as a zone is (`xtPatchValue(store, id, line)`): State and Note/CC, then Step 1–32; the zones' Knob Sets are at 0x14f2 (v187, "The arpeggio pattern and the knob sets in a patch").
- The arpeggiator (v188; HARDWARE_FIRMWARE.md, "The arpeggiator"; `XtArpeggio`) is an object above the tone generator, not part of a tone: a part's keys go to it, it steps on the OS's tempo timer
  (`0x8930eb00`, 480 ticks a quarter, the count the 24 clocks are every 20th of), and sends MIDI messages to the tone generator's input with a source byte of 3, which gives its notes a key state K of
  their own. A pattern cell is 0 REST, 1 TIE, 2…128 velocity 1…127 (the step code's reading; v187's "128 TIE" is withdrawn). The router (v189, "The arpeggiator in XtSynth"): with the Switch on, played keys go to it and not the part (`0x89307c80`), the Hold pedal to its pedal; the host's notes are the XT's keys, as with Remote Keyboard Switch on, the only way MIDI IN reaches it. Its notes' key state N is source 3's, but the controllers C are shared by sources 1 to 3 (`0x894d6120`); XtSynth keeps one key state, as nothing else reaches the part with the Switch on. Its ticks are the clock object's each ms (`0x894710e0`, `XtTempoTicks`), not XtSynth's 24 clocks. xt_tone can't reach it: it plays the tone generator below the router, so its patch cases play a patch's tone alone: `xtPatchControls` has no arpeggio, `xtWithPatchArp` (XtArpeggioPatch.cpp) adds it. The plugin's side (v190, `PluginArp.cpp`): ten host parameters by the XT's IDs, appended after every other, names, ranges and defaults the firmware's and texts its routine's (End Point shown as the step, 1…32, the manual's); the pattern, a patch's data and no parameter, kept in the session's state (`arpPattern`; before any patch and for a session from before, an initialized patch's from v469, the user's d-6: line 1 at velocity 100 on every step, so the arpeggio plays live keys by its Motif) and read by the audio thread a word at a time; the Play tab's Arpeggio and Arpeggio pattern cards. From v468 the pattern is edited there by the XT's Pattern Edit (`XtArpPatternEdit`, xt_arp's edits section holding it to the firmware's own screen, setter and clears), and a CTRL line's controller is kept 0…127 as the patch holds it, made 95 at most only where the object takes it (`xtArpLaidOut`, `0x8930ca30`; the reads did it before).
- The ports' tables are the process's, read by every instance without a lock (`XtSynthRom.cpp`, v173): written once, by the first image to load,
  and published by an atomic flag only when all of them are in. Another file is taken only if it holds the same bytes, with nothing written;
  a different image is refused, saying why, until the host starts again. A new table goes in `loadTables` there, never written after.
  Two known images from v198, each known by its header's sections (`kVSynth` rom.2, `kVc2` rom.4) and held by its own
  `RomInUse` by that rule; a file of the other image, or of neither, is refused, saying so. rom.4 is found beside the rom.2
  an instance opens, then where rom.2 is looked for (`PluginRom.cpp`); no port reads it yet. The plugin loads both images
  at its start, so switching modes is seamless (the user's decision, 2026-11-14): a VC-2 port's tables go in rom.4's
  loader (`loadVc2`), read at that start, never at a switch; and its file in `research/tools/vc2_tables.sh` (v231), the
  one list every program linking XtSynthRom.cpp takes (build.sh's xt_synth, xt_tone and xt_vc2params, ab.sh, mutate.sh's
  tone ports): a loader missing there links until a kept XtSynthRom.o is built again.
- An image maps at 0x89300000 as its own header lays it out (v197; HARDWARE_FIRMWARE.md, "A firmware image's own
  header"): rom.2's .data/.bss up to 0x8a020000, rom.4's to 0x8a200000 (the interpreter sets them up, `Sh4::image()`
  says where). Calls take r4–r7, then the stack; `Sh4::call(addr, {args}, sp)`.
- The SH-4 runs round-to-zero (FPSCR.RM), FMAC = product then sum (two
  roundings), denormals flushed. `XtFloat` reproduces this exactly (v62; FSQRT, `xtf::fsqrt`, from v215);
  harnesses flush denormals on the host too (`xtf::flushDenormals`, XtFloat.h, from v208; xt_tone, xt_analog and xt_fw
  still have the block inline).
- Decompiles of FPU-heavy code are unreliable (double/single mode confusion):
  read the disassembly for DSP and control code.
- Parameter names by ID: pointers from 0x89790338. Descriptors (centre,
  default, min, max): 12 bytes per ID from 0x897a167c; IDs ≥ 0x55d use a
  second bank, 8 bytes per ID from 0x897a56d8.
  rom.4's (v199, `Vc2Params`; its IDs its own): names 0x898b5798 (getter `0x894f9fc0`, 2476 IDs), descriptors
  0x8950f0b0, the second bank 0x895131a8 from 0x56a, strings 0x898b8154; the text `0x894fa070(id, value, 0)`, rom.2's
  routine by rom.4's IDs, `0x894fa730` its wrapper with the unit. It has no format for many of the VC-2's own IDs
  (VC2_VOCAL_DESIGNER.md, "rom.4's values as text"). A store value of a one-value range reads as stored, in both images.
- The voice buffer is int16 × 65536 on the XT: `XtSoloVoice::renderXt` writes it as it is (`render` is
  that × 2^-31, ±1), the band players' (int16) are ×2^16 from it.
- `xt_fw solocmp` takes R and F as ratios (F 0 = none) and Energy as a gain
  0..1; `bandcmp` type 3 BACKING / 5 ENSEMBLE.
- `XtSoloVoice` is bit for bit: its grains, filter and gain are plain float arithmetic under FE_TOWARDZERO, as `XtMbPlayer`'s
  (HARDWARE_FIRMWARE.md, "SOLO bit for bit"). The rounding mode is set inside `noteOn` and `renderXt`, so nothing called
  from them may use XtFloat's sums: TwoSum needs round to nearest. `XtBeat` does, so it runs under `RoundNearest` there.
- A band player's backward repeat (`0x8950f50a`) can overflow its position to near 2^31, in the port too.
  An Alternate loop reaches it with the walk positive, as the bounce negates the step. The reads then leave the
  wave, which on the XT gets whatever is in RAM. `xt_tone` and `xt_analog` give them zeros, as the port reads
  (`xtFwHookBandReads`; HARDWARE_FIRMWARE.md, "A band read far from its wave", "`xt_analog` past the far band reads").
- SOLO's anchor, grain timer and ages are int32 on the XT and can overflow. `XtSoloVoice` wraps the anchor and keeps the others
  as int32: a backward repeat ends only by overflowing (HARDWARE_FIRMWARE.md, "The anchor's overflow"). A loop ported to `double`
  from int32 may not end; `solocmp` dies by `alarm(120)` so a hang fails the check.
- `xtSoloEncode` gives the records of the firmware's encoder, mono (`0x8951e120(2)`) and stereo (`(3)`, which is not mono on
  `(L+R)>>1`): `xtFwSoloEncode`, `xt_enc cmp` in fcheck, HARDWARE_FIRMWARE.md, "The SOLO encoder against the firmware's" and "The
  SOLO encoder's stereo mode". `solocmp` gives both sides the firmware's records and `xt_tone` both sides the port's, so neither
  shows a difference, bar `xt_tone`'s "the SOLO records of" lines. A rounding in the filter's paths rarely reaches a record, which
  is why `xt_enc` compares memory as well. The project's stored records were written by V-Synth 1.50 or 2.01, not the XT's 2.02:
  27 of 28 waves equal (`xtrecs.py`). Wave 373 differs by one step of the tracker's period at one mark, cause not known.
- The Patch Tempo goes to the clock's object and is read back, its tenths dropped, so 233 BPM reads as 233.4. An LFO's or envelope's value from 128 on is a
  note value. Tempo Sync and Time Trip steer the player's walk by a factor (`XtBeat`, `0x8950c280`), not its position. In `xt_tone` the firmware's clock (`0x894f4da0`)
  runs on XtSynth's, so that comparison can't see the rate, and a section of its own runs the firmware's timer routine against it (HARDWARE_FIRMWARE.md, "Tempo sync and Time Trip").
- Clock Source EXTERNAL (ID 0x41, the clock object's +0x40): one clock to an F8 (`0x894712f0`), and the tempo read is 150000/(+0x48), the whole ms over the last six clocks, so it steps. The plugin's HOST is that, on the host's
  transport (`XtSynth::hostClock`): a clock on each 24th of a quarter it passes; stopped, they run on at its tempo (a choice, not the firmware's). `xt_tone`'s firmware is fed the port's F8s, so a section of its own holds them to the
  transport's position (HARDWARE_FIRMWARE.md, "The host's tempo, by the firmware's external clock").
- Every controller ends at the pad's handler (`0x894d6f10` → `0x894d8360`, `XtSynth::padController`), on Time Trip Assign X and Y, then its queued update: a pad's controller is an ordinary one too. X-Y mode's pair, Pad Assign X and Y, only feeds the Matrix's PAD sources.
  The defaults, CC82, CC83 and CC80, CC81, are Roland's chart; the descriptors say 0, but nothing else writes the pad (HARDWARE_FIRMWARE.md, "Time Trip by its controllers"). Pad Mode is the XT's own pad's choice of pair, and no tone generator code reads it. A SOLO wave taken back past its start stays silent after letting go, as its seek keeps the end flag: that is the firmware's.
- A tone's path (constructor `0x894f5860`, note start, update, the scheduler, what is hooked) is in
  HARDWARE_FIRMWARE.md, "The whole tone against XtSynth". Any change is a tone update (`0x894dc0b0`): all
  the blocks again, COSMs before the envelopes.
- A COSM's new block reads levels it holds from its note-on or last 10 ms update (object +0x104…+0x110),
  not the envelopes' output: `CosmVoice::hold` (v107). So the order of blocks and envelopes in an update has no effect.
- The patch TVA's block (`0x894ea380`, live `0x894eab40`) holds Level, Pan, the sends and CC 7, 11, 10, read from C + 0x84 + cc at each
  conversion pass. Its pan is two things: the law on the output bus's pair, which glides 3% per block, and the LFO's on the gains. A frame
  on the bus is 32 bytes: MAIN, Reverb and Chorus, MULTI, DIR, which `XtSynth::frame` holds and `xt_tone` compares word by word. A sounding tone's DIR pair doesn't update (HARDWARE_FIRMWARE.md, "The patch TVA's level and pan", "The whole frame in XtSynth").
- A controller: the channel keeps numbers below 96 (C + 0x84 + number), then its handler (table built at `0x894d7400`) queues an update of the
  part's notes (`0x894d7760`), for any number, value moved or not. `*K` is N, the part-and-channel state (+0x11c Hold, +0x11d Sostenuto). A note's
  +0x48 is key, Hold, Sostenuto: it stops when all three are clear (HARDWARE_FIRMWARE.md, "Hold, Sostenuto and the mode messages").
- The Matrix is the note's object M (the input table's +0x68), read through the getters `0x894e7010`…`0x894e7860`; a slot is not a Dest (`xtFwMatrixTables`).
  A getter forms its own group of sums and no other. A pad's move (TRIP-R) brings no update, so a key-off, which converts only release times, reaches only the envelopes' groups: `matrixRelease`. A note value (128 on) takes no sum (`withSum`).
  `0x894d96d0` on `0x89cf7634`, which older sections call "the matrix", is the Multi Step Modulator, ported in v192 (`XtMsm`) and in the sound from v193: its terms beside the sums (HARDWARE_FIRMWARE.md, "The Matrix", "The Multi Step Modulator", "The Multi Step Modulator in XtSynth").
- The Multi Step Modulator (v192; HARDWARE_FIRMWARE.md, "The Multi Step Modulator"): part 0's, four tracks on an OS tempo timer that fires every 20 of the 480 ticks a quarter, counted from its start, not the beat. Key Sync OFF starts it at the first note, never stops it, and loops every track whatever its Loop Switch; PART and VOICE restart it at a note and stop it when no note of part 0 sounds. A converter adds `0x894d96d0(obj, dest, part, zone)`: the tracks on its destination and its tone's zone. The coarse tunes' terms go through a curve (±63 to ±24) that reads on past its 64 entries when an edited Grid makes SMOOTH overshoot. `0x894da9f0` is not it (`0x89cf7eac`).
  In XtSynth (v193, `XtSynthMsm.cpp`; HARDWARE_FIRMWARE.md, "The Multi Step Modulator in XtSynth"): settings read in setControls before the knobs' update, its timer on `arpMs`'s tempo ticks and its clocks only when the timer fired, the head in tgNoteOn before the voice, its tone updates a controller's (`newBlocks`), the arpeggio's flag `XtArpeggio::msmFlag`. A conversion pass reads each voice's 72 terms into `Voice::msm` (a key-off only the Releases), and each knob takes its term where the converter does (`XtSynthKnobs.cpp`, `XtSynthEnvKnobs.cpp`; a COSM's in its input words, `xtCosmWordsHold`). A new term site needs a `wire…` plant in `tone_muts.txt`.
- The effects' driver takes messages: flags at `0x89ced200`, read by `0x894ac970` (second byte 0x20 system, 0x10 Attenuation; fourth byte 0x80 patch, 0x40 switches, 0x20 MFX, 0x10 Chorus, 0x08 Reverb), and a reader per block fetches its parameters by ID. `XtFxControl` sends those, so which readers run, and how, stays the firmware's. Its hooks are the bus, the sleep and the two parameter reads. A read is the stored value less the descriptor's centre. Some things depend on how a parameter got there, in the firmware itself: Resonant Filter's Sweep Depth does nothing when edited alone, and Reverb Delay keeps the last type's pre-EQ and the last tempo it heard: HARDWARE_FIRMWARE.md, "XtFxControl".
- A value's text is `0x89461a70(id, value, 1 to add the unit)` (`0x89462120` with 0, `0x89462130` with 1); the screens reach it through `0x89370ca0`. Each ID's string (`0x89792ba0`, from 0x55d `0x89794114`) has names split by 1, a range by 2, the unit after a 3. A value past the range faults. Note values come back as LCD font codes (`XtFxValueText`'s `noteOf`, measured against the driver). The strings are a row out at Non-Linear's last three. The routine has no format for four IDs, Hall 2's, Hall 3's and Garage's Room Size and Plate's Xover Freq, so they run as a sibling's (`formatOf`; the user's decision, 2026-11-04: the manual's scale). The 11 Reverb Times show the driver's time, `0x894b2bf0` (`isReverbTime`; samples at 44.1 kHz; the user's decision): the routine's text is wrong between values 6 and 21 and missing on three types. So the panel parts from the XT's code there on purpose: HARDWARE_FIRMWARE.md, "The effects' values as text", "The seven on the manual's scale", "Reverb Time by the driver's own table".
- The effects' tempo: the driver reads ID 0x11d itself, the Patch Tempo as it stands, not the clock's read-back. With Clock Source (0x41) at 1 it polls the clock's object (`0x894acb10`, `XtFxControl::outsideClock`) and takes a reading only past a margin of 25 + 1%, in tenths of a BPM. The driver's sleep doesn't sleep here, so `XtFxControl::tick` sends a message only once the chip has all of the last, as the XT's task takes them: without it a tempo moving for 10 s leaves the chip 150 s behind (HARDWARE_FIRMWARE.md, "The effects' parameters and panel").
- The effects don't run on the SH-4: MFX, Chorus, Reverb, the system's EQ, Master Level and Output Gain are microcode on a separate chip, Roland's ESP4 (RA0A-101; the JP-8000's instruction set, v123; ported in `XtFxChip`, v126; its delay memory 2^17 words on a 256 K × 16 DRAM; programs in rom.2,
  8 channels by DMA), with no reference but a real XT. Only the SH-4's driver and converters run in `Sh4Interp`. Map, the word as far as decoded, and the proposed
  order: HARDWARE_FIRMWARE.md, "The effects". The tones' `g` (v114) is ID 0x526, Out Gain for TG, default −24; the user's Output Gain is ID 0x76, on the chip. The plugin plays at the XT's level by these numbers (v149, the user's decision; not measured on a real XT): a full-scale wave at ⅛, both in `XtSynth::busOut` and through `XtFxUnit::fromChip`, the chip's full scale being ±1.
- The EXT IN OSC (v203; HARDWARE_FIRMWARE.md, "The EXT IN OSC"): OSC Type 2, a voice of kind 4. Its player (`0x895097d0`) plays words 4 and 5 of each frame the effect chip's DMA returns (`[0x8987c7a0]`), at a gain gliding 1/32 a sample; its note-on and update set the pitch and gain target directly. Nothing is bound to it: its pointers stay on the tone's envelope 0 (at 1) and LFO 0 (at 0), so its OSC's envelopes and LFO hold where their note-on left them, the gain target is gain × level and the pitch takes the pitch envelope's whole depth; its stereo flag is 1 and its root 440. In XtSynth `XtOscVoice::extIn`, its input `XtSynth::input`, which the chip's words 4 and 5 reach through `XtFxUnit` (v204: GRAM 0x98, 0x9a, the input routed by the firmware's own request by External Input Type and Source, IDs 0x78 and 0x7c; HARDWARE_FIRMWARE.md, "The input's way in, the chip's side"), and from v207 the plugin's input bus feeds it (HARDWARE_FIRMWARE.md, "The input's audio": `XtHostInput` at 44.1 kHz behind its latency, `busOut` sending each block into the chip with the Effects on or back as `XtExtInCopy` with them off, so both reach the EXT IN OSCs the next block, a frame late, latency + 33 frames after the host gave it; an EXT IN OSC alone renders with no wave); `xt_tone`'s EXT IN mode (`xt_tone rom extin`) draws it and feeds both sides `xtExtInTestBlock`.
- Vari Sw off (v151): a SOLO wave plays on `XtLinearPlayer` (`0x89509870`/`0x89509f52`), a band wave keeps its player with its walk × the pitch (`0x8950f810`).
  The player's byte 0 takes the block's flags at each update but for 0x20, so Robot follows during a note, its grains at the next event load. Each wave's loops have rate
  factors (`vplp` +0x14, +0x16), Vari on too. HARDWARE_FIRMWARE.md, "Vari off".
- Start Offset (v152, IDs 0x2c6, 0x2fa): samples under RETRIGGER and LEGATO (the block's +0xb4 bit 0x80; STEP and EVENT give event numbers). The converter puts it at +0xb8, the last
  sample at most, compared unsigned; the voice gives each player it × 16 at +0x18: where a note walking forward starts (`0x8950fa60`) and where a walk back stops, Vari off's first
  sample. Read at note-on only. Vari off's stereo hold going backward in an Alternate loop compares its fade with FR11, which the XT never sets there (`XtLinearPlayer::setStaleFr11`:
  the plugin 0, `xt_tone` the firmware's own). HARDWARE_FIRMWARE.md, "Start Offset".
- Playback Mode (v155, IDs 0x2d2, 0x306): LEGATO takes the earlier note's player (the allocator's, 0x894d4c04; 0x894f2d00) per OSC of the same kind, not one at its wave's end
  (its +4 at 1; a LITE player never writes +4, so it is stale slot memory: `OscVoice::staleEnd`, the plugin 0, `xt_tone` the firmware's own). STEP and EVENT play between two events of the wave's
  `vpno` (STEP's count at 0x89cf7210 by part, zone and OSC; EVENT by key from C4), on band waves; a LITE wave's converter doesn't read the mode; SOLO waves' came in v156 (`XtSoloVoice`'s events).
  The player's byte 0 takes the block's flags at each update (0x894f3fb6), so the band player's Loop bit follows the mode during a note (`XtMbPlayer::setLoopBit`). Records past the vpdt
  that leave nothing to step (a seek to the end on the last event, an event note at the last event) mark the player (`readPastWave`, silent from then); `xt_tone` ends the case. HARDWARE_FIRMWARE.md, "Playback Mode".
- LITE waves (v153): type 0, the samples alone, the voice's kind 1 (`0x894e2ec0`), with a block of its own (+0x80 mode, Vari on 2, off 0; +0xc4 the Start Offset, +0xc8 the length),
  note-on `0x8950b5b0`, update `0x8950bbb0` (no 70% steps, no Energy). Vari off is `XtLinearPlayer` (`setLite`); on, `XtLitePlayer`, two heads under windows (`0x8950a7f0`/`0x8950a7fc`).
  Its kernels run through an inline constant, H'0000 then H'3f00, so `Sh4Interp` takes H'0000 as doing nothing. Every update re-installs the players by the block's mode, so `xt_tone`
  holds Vari during a LITE note; and an analog note after a LITE one on a voice reads that note's +0x20c, +0x214 as limits (the check carries 0). A project's LITE wave is a RIFF with
  `vpsc` and no `vpfl`. HARDWARE_FIRMWARE.md, "LITE waves".
- The analog oscillator's map (waveform codes, routines, the converter's
  input, the hidden calibration IDs 0x528…0x55b) is in HARDWARE_FIRMWARE.md,
  "The analog oscillator, and SAW". A patch's waveform number is not the
  routine's code.

## Porting a COSM type (v65–v78 did it this way; all sixteen are done)
The type map (converters, block codes, DSP routines, which types are done) is
in HARDWARE_FIRMWARE.md, "COSM: the Dual Filter, and the map of the types". Each
type also has a stereo routine (v104, "COSM: the stereo routines"): `process(…, stereo)`,
checked in `XT_COSM_MAIN`'s last section.
1. `pnames.py` on the type's IDs; disassemble the converter (`ghidra.sh
   Disasm.java`): it writes the code at +0 and the block. Note its curve
   helpers (several call the firmware's powf/logf/expf; see v68, v69).
2. `cosmcases.py <code>`, then disassemble each case and the mono DSP routine.
   Read the asm, not the decompile. Note which envelope the binding puts in
   slot 1 (it varies by type), what note-on clears (D+0x1c…0x88), and the
   state the routine stores back.
3. Port in XtCosm: Params, Block, `xt…Block` (the converter), and the object
   (`noteOn`, `setBlock`, `update`, `process`). Read rom.2 tables in
   `xtCosmLoadTables`; single precision through XtFloat, constants as `K()`
   bit patterns (rom.2 often stores values a unit off the nearest float).
   Types that depend on absolute level (followers, quantizers, envelopes) take
   `process(lr, scale)` and work in the XT's units.
4. Add a section to `XT_COSM_MAIN`, modelled on the last one: the converter's
   whole block compared byte for byte; the firmware's own note-on
   (`0x894edf70`, with the buffer pool's `0x894ec2b0` hooked for types with
   buffers); updates; new blocks; the filter with its state and buffers
   compared every block. Then `mutate.sh` a few likely mistakes.
5. Wire it: XtSynth (`Controls::Cosm` fields, a type number, the block
   function, `cosmSetBlock`, note-on, update, process), PluginProcessor
   parameters (`cosm…`/`cosm2…`, the XT's defaults), the editor's faces for the type
   (its COSM panel's in `UiCosmPanel.cpp`'s `kFaces`, its DETAILS' in `UiDetailTables.cpp`), `TEST_*` in PluginTest, `XT_*` in the CLI.
   Check `pt.sh` with the type, `VSynthEmuEditorTest`, and a `TEST_SNAPSHOT_TYPE` snapshot.
6. `fcheck.sh --update` (the diff must only add the new lines), `verify.sh`,
   then the docs: a HARDWARE_FIRMWARE.md section (newest first) and its
   type-map row, notes.txt, RESEARCH_LOG.md. Commit `vNN: …`.
