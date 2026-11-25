# V-Synth Test Recording Guide
<!-- audience: outside -->

This file is the master recording reference.  After each session add new files to the
checklist and update `RESEARCH_LOG.md` with results.

---

## Effects captures (added 2026-11-03, do this next)

**Why:** the XT's effects run on a separate chip that nothing but a real XT can execute, so these
recordings are the reference the emulated chip gets checked against (research/HARDWARE_FIRMWARE.md,
"The effect captures").  Unlike the takes further down, these are about levels and exact samples, and
a script drives the XT, so it is mostly waiting.  **Not yet tried on an XT**: step 5 finds out in 15 seconds.

**Files** are in `analysis/test_files/fx/`, and the commands below run from the repository's root.  They aren't in
git, because the runs hold firmware defaults.  To make them again: `research/tools/fxcapture.py`, and `fxplay` with the
build line at the top of `research/tools/fxplay.cpp`.

**1. Pick how to record.**  The first of these you can do:

| Path | What it needs | What it allows |
|---|---|---|
| **USB** | A USB cable and no interface, if a current Mac sees the XT (settings in step 4). Record from the XT at 44.1 kHz | Sample by sample, the best there is, if it works: the service notes put the USB chip on the same bus as the DAC, ahead of the converter below. Its width, and whether a Mac sees it, aren't known yet |
| **Digital** | XT COAXIAL or OPTICAL OUT into an interface's digital input. System: Digital Output Freq **44.1 kHz**. Interface clocked from that input, recording at 44.1 kHz, 24-bit | One clock, 24 bits, no analog noise: levels, times and responses at their best. Not sample by sample as it stands: the firmware keeps the XT's sample-rate converter in the digital outs at every rate, 44.1 kHz included |
| **Analog** | MAIN OUT into line inputs, as for the older takes, at 44.1 kHz, 24-bit. Set the trim for a full-scale XT and leave it alone between runs | Levels, times and responses only |

Digital and USB together would be better still, if your interface allows it: run 1 through both shows what the converter does.

**2. Load the wave.**  Copy `fxprobe.wav` into the project's `.prj` folder, the way you got `Emu_test.prj` off the XT (USB
Storage).  Then Disk, Import Files, Wave (manual p. 133).  It needs no encoding, and no saving unless you want it past a
restart.  In SAMPLE, Info: Original Pitch should read C4 (60) and Original Fine Tune 0; set them if not.

**3. Make the patch.**  In Patch mode, on any patch: <▼ MENU>, PATCH Init, then OSC 1 Type PCM, Wave `fxprobe`.  A run sets
everything else, in memory only, including the system's EQ, Master Level and Output Gain, so **don't write the patch or the
system afterwards**; restarting the XT undoes it all.

**4. Connect MIDI.**  Either MIDI IN from an interface, or USB:
- SYSTEM Com USB Setting: USB Mode **MIDI** (or COMPOSITE if it offers it), then [OK].  If a USB-MIDI Driver Type shows, **GENERIC**: VENDOR needs Roland's old driver.
- To record over USB as well, SYSTEM Com Audio I/F: USB Audio Input Source **MAIN**, USB Audio Input Monitor Sw **OFF**, USB Audio Output Dest **OFF** (or the Mac's sounds can reach MAIN).

Either way:
- SYSTEM Com MIDI/USB: Rx Sys-Ex **ON**, Device ID **17**, USB-MIDI Thru Sw **OFF**.
- Part MIDI: Part 1 Rx Sw **ON**, Part 1 Rx Ch **1**.  (For another ID or channel, make the files again with `fxcapture.py --dev N --ch N`.)

Run `analysis/test_files/fx/fxplay` alone to list the MIDI outputs.  Below, use any part of yours in place of "V-Synth".

**5. Test, 15 seconds.**  Go to the screen that shows MFX Type, and turn the monitors down.  Then:
```bash
analysis/test_files/fx/fxplay analysis/test_files/fx/fx_test.mid "V-Synth"
```
After the 5 s countdown:
- MFX Type changes to Parametric EQ, and back to Thru at the end.  (If the screen doesn't follow, leave it and come back.)
- You hear three clicks, then a rising sweep.
- The sweep drops in level near its end.  That's Master Level, so a System write works too.

If nothing happens, check the output's name and the cables.  If there's sound but no change of type or level, check Rx Sys-Ex and
Device ID.  If there's a change but no sound, check the wave in step 3 and Part 1's channel.  If it still doesn't work, or only some of it
does, tell Claude what you saw: the XT may want something the V-Synth's manual doesn't say.

**6. Run 1, 8 minutes.**  Keep the monitors low: it's clicks, sweeps and noise, and the last two takes clip on purpose.  Start
recording, stereo, no processing.  Then:
```bash
analysis/test_files/fx/fxplay analysis/test_files/fx/fx_run_1.mid "V-Synth"
```
Leave the XT alone until it says done, then stop.  Save as `analysis/test_files/fx/fx_run_1.wav`.  **This one matters most**, so check it before run 2, which takes a minute:
```bash
.venv/bin/python3 research/tools/fxcut.py analysis/test_files/fx/fx_run_1.wav
```
The last line says whether to go on.  FAIL lines say what to fix, and LOOK lines are worth reading but stop nothing.  It also cuts the takes into a folder beside the recording.  Or tell Claude and it'll run this and read the rest.

**7. Run 2, 22 minutes.**  The same with `fx_run_2.mid`, saved as `fx_run_2.wav`, and the same check.  If a run gets interrupted, `--from <seconds>`
before the file name picks it up, with a `next_s` from `fx_takes.csv`; save that part as a separate recording and give `fxcut.py` the same `--from <seconds>`.

**8. Tell Claude** which path you used, the interface and its settings.  No normalizing, fades, dither or trimming: the takes are
found by their timing and the wave's first click.

**If SysEx can't be made to work:** `fx_takes.csv` lists every take's settings in the panel's words.  By hand, the takes `main`,
`multi_off`, `multi_thru`, `multi_off_m64`, `peq_mid_plus15`, `sdelay_stereo` and `wet_reverb` are the ones to do, one recording each,
named after the take.  After step 3, set: Vari Sw, Tempo Sync, Robot, Loop and Time Trip OFF; OSC 2, both COSMs, MOD and TVA off;
4 Band EQ off; MFX Send 127, CHO Send 0, REV Send 0.  Then play C4 and hold it for the wave's 17 s.  `fxcut.py <file> --take <name>` checks each one.

**Exact without any interface, by hand (not tried):** Resampling records what goes to MAIN OUT inside the XT, ahead of the converter, as a 16-bit wave
you can copy off with USB Storage.  In the SAMPLING General screen: Input Source **RESAMPL**, Sampling Type **STEREO L R**, Trigger Mode **NOTE**, Pre Gain **0 dB**, Pre Trigger **0**; and Pre-Effect Type **OFF**.
Set a take up as above, then play C4 and hold it for the wave's 17 s.  There's room for about six takes (115 s of stereo in the factory project), so do the list above.  If USB and
digital both fail to give single-sample clicks, this is the way to get exact data, up to the 16th bit.

---

## VC-2 captures (added 2026-11-22)

**Why:** the VC-2's ports are held to rom.4 bit for bit, but the firmware can't show what comes from outside it: how loud the
input arrives, which input side the VC-2 hears as the mic, and what rom.4's own effect programs sound like on the real chip.
These runs play a known signal into the XT's input, in VC-2 mode, while a MIDI run sets the patch take by take.  What each run
settles, and where it was asked: research/VC2_MODE_PLAN.md, step 5.  **Not tried on an XT**, and nor have the effects captures
above.  Steps 4 and 5 find out in a few minutes whether the XT takes the messages in VC-2 mode at all.

**Patches on the card (the plan's step 5, 2026-11-25).**  In VC-2 mode the XT has a SysEx map of its own, and 23 of the
settings these runs change have no address in it (research/HARDWARE_FIRMWARE.md, "rom.4's SysEx: the VC-2's own address
map").  21 of them are patch settings: the vocoder's type, depth and level, the OSC types and switches and more.  Those go in
patch files on the XT's card, made by the VC-2's own save code.  Each take selects its patch by Bank Select and Program
Change, then the run sends by SysEx only what differs.  The other two, **Output Gain** and **External In Type**, are system
settings that no patch holds and SysEx can't reach.  Run 1 pauses 20 s before each take that changes one: fxplay prints
what to set, and you set it at the panel.

**Files** are in `analysis/test_files/vc2/`, made by the commands below from the repository's root.  They aren't in git, as the
runs and patches hold firmware defaults.  To make them (the first builds the patch writer, about 10 s):
```bash
bash research/tools/build_vc2patchwrite.sh
```
```bash
.venv/bin/python3 research/tools/vc2capture.py
```
```bash
clang++ -std=c++17 -O2 research/tools/fxplay.cpp -framework CoreMIDI -framework CoreFoundation -framework AudioToolbox -framework CoreAudio -o analysis/test_files/vc2/fxplay
```
That gives:
- the input signal (`vc2probe.wav`, 32.5 s);
- the card's folder (`card/`: `VC-2 Project.ini` and the project folder `RUNS.vc2`, holding the 20 patches `RUN 001` to
  `RUN 020`);
- a 23-second test (`vc2_test.mid`);
- four VC-2 runs (`vc2_run_1.mid` to `vc2_run_4.mid`: 12, 16, 18 and 10 minutes);
- one V-Synth-mode run (`vc2_vsynth.mid`, 2 minutes);
- the take list (`vc2_takes.csv`).

`fxplay` plays a run's MIDI to the XT and its input to your interface together, on one clock.

**1. The connections** (questions d-1 to d-5 ask which of these you have).  Three cables' worth:
- **The input**: your interface's outputs 1 and 2 into the XT's rear **INPUT** jacks, L and R.  Both sides matter: the test
  signal has a click on the left alone and one on the right alone, and which ones come back answers which side the VC-2 hears.
  The front **MIC** jack would carry one side only, at the **INPUT** knob's unmarked gain.
- **The recording**: the XT's output back into the interface, as for the effects captures (step 1 there: USB, digital or
  analog).  Stereo, 24-bit, **44.1 kHz**, with no processing.
- **MIDI both ways**: the runs go to the XT, and the map check in step 4 listens to what the XT sends.  USB-MIDI or a MIDI
  interface's IN and OUT.

**With the Ferrofish Pulse 16 over MADI** (your rig, 2026-11-22), wired back to back:

| Pulse 16 | XT | For |
|---|---|---|
| analog out 1, 2 | rear **INPUT** L, R | the test signal into the XT |
| analog in 1, 2 | **MAIN OUT** L, R | the recording the checker reads |
| analog in 3, 4 (if you can spare them) | **DIRECT OUT** L, R | what DIR carries on the VC-2, which nobody has seen: by rom.4's programs, nothing |
| analog in 5, 6 (the same) | the signal going into the XT, split off or routed back | the signal itself, on the same converters, so the XT's own gain separates from the Pulse's |

- **Levels.**  The Pulse's analog level is set from −8 to +20 dBu at 0 dBFS, in 1 dB steps.  Outputs 1 and 2: **+8 dBu**, so the
  signal's −18 dBFS sweep reaches the XT at −10 dBu, the rear inputs' nominal level (XT manual, specifications).  Inputs: **+20
  dBu**, so nothing the XT sends clips the Pulse; the direct sound may then read low, which the checker only notes.  Write both
  settings down: with them the input's level reads in volts.  If the XT's PEAK light comes on during the sweep (step 6), lower the
  outputs a dB at a time and write down where you stopped.
- **Rate.**  The Pulse, the MADI interface and the recorder at **44.1 kHz**.  The XT's own clock runs beside them, so the checker
  will say "two clocks, or an analog path": that's expected here.
- **The input's channels.**  fxplay plays the signal on the Mac's MADI interface as a stereo pair: the channels macOS's Audio MIDI
  Setup lists as that device's stereo pair (Configure Speakers), 1 and 2 unless you change it.  Route those MADI channels to the
  Pulse's analog outs 1 and 2.  `fxplay --audio-devices` gives the name for `--device`.
- **The recording.**  Record the Pulse's inputs over MADI in your DAW, and export each run at 44.1 kHz, 24-bit, with MAIN OUT as
  channels 1 and 2.  More channels in the same file are fine: the checker reads the first two, and Claude reads the rest.
- **The cables.**  The XT's INPUT and DIRECT OUT jacks are unbalanced 1/4-inch phone jacks, its MAIN OUT balanced.  Use the same
  kind of cable on the loopback pair as on the pair into the XT, so the two levels compare.
- **MIDI.**  Through your MIDI interface, both ways: its OUT to the XT's **MIDI IN**, the XT's **MIDI OUT** to its IN.  Leave USB
  Mode as it is.  Turn off any thru from the interface's IN to its OUT, in its own settings or the DAW's, at least for step 4:
  there the XT sends a message for every edit, and a thru would send each one straight back.  Close the DAW's hold on the
  interface's ports if fxplay can't open them.
- **Digital.**  Not used: the Pulse's optical ports are ADAT only, so the XT's COAXIAL and OPTICAL OUT stay unplugged, and
  everything goes through the Pulse's analog converters.  That gives levels, sides, pitches, times and responses: everything
  in runs 1 and 2, and the effects' responses in runs 3 and 4, but not sample by sample.

**2. The card, then VC-2 mode.**  The VC-2 reads its project from the card when it starts, so copy the card's folder first:
1. With the XT off (or the card out of it), copy what's in `analysis/test_files/vc2/card/` to the card's top folder:
   `VC-2 Project.ini` and the folder `RUNS.vc2`.  If the card already has a `VC-2 Project.ini`, copy that somewhere first: it
   names your own VC-2 project, and putting it back afterwards returns the VC-2 to it.
2. Card in, XT on, then press **[V-CARD]** and choose the VC-2 (XT manual, p. 22).
3. **Check one patch by ear.**  Choose patch **RUN 001** at the panel.  Play `vc2probe.wav` into the input from any player and
   hold C4: you hear the input through the tone, since RUN 001 has OSC 1 on as EXT IN.  Then choose **RUN 002** and do the
   same: nothing, since its OSC 1 is off and its direct sound at 0.  If RUN 001 isn't in the list or is silent too, the VC-2
   didn't take the card's project: check that `VC-2 Project.ini` is on the card's top folder, not inside another folder,
   and tell Claude.

The runs set the rest in memory only, so **don't write a patch or the system afterwards**.  Writing a patch would overwrite
one of the card's.  Restarting the XT undoes it all.

**3. The system, by hand, in VC-2 mode.**  On the SYSTEM pages:
- **INPUT Jack Switch ON**, **INPUT Jack Gain 0dB**, **MIC Jack Switch OFF**, so the rear jacks alone are the input (XT manual,
  p. 126).
- **Receive Exclusive ON**, **Device ID 17**, **Part 1 Receive Switch ON**, **Part 1 Receive Channel 1** (for another ID or
  channel, make the files again with `vc2capture.py --dev N --ch N`).  For USB-MIDI: **USB Mode MIDI**; a **USB-MIDI Driver
  Type** of GENERIC.
- **USB External In Type OFF**, so the Mac's sounds can't reach the input.
- **Output Gain 0dB**: SYSTEM, the **Common** group (the buttons at the screen's foot), the **IO** tab (on the left), Output
  Gain.  Every run starts from it.  Run 1 changes it by hand in its pause and puts it back; after an interrupted run, set
  it back before the next.
- **External In Type**: the VC-2's code draws it on no page (research/HARDWARE_FIRMWARE.md, "The VC-2's system settings at
  its panel"), so it should stay at the VC-2's default, **MONO-L+R**.  Look for it on SYSTEM > Common > IO all the same.
  **Write down whether you found it**, and where: the check of run 1 reads its four External In Type takes by that.  If
  your XT shows it, set it to MONO-L+R.  If not, run 1's four External In Type pauses have nothing to set: let them
  pass, and give the check `--extin-not-found` (step 7).
- **Receive Program Change ON** and **Receive Bank Select ON**, for the patches.  The runs send these two themselves, so
  this only checks them.

Check each of these in VC-2 mode itself: whether its SYSTEM settings are the V-Synth's or its own isn't known.

**4. Check the map, 2 minutes.**  The runs set the VC-2's parameters by SysEx at the addresses the VC-2's own code uses
(model 00 00 0D), read from its firmware and run there, but never yet on an XT, so ask the XT itself:
1. SYSTEM: **Transmit Edit Data ON**.
2. On the Mac, with any part of the name of your MIDI input from the XT in place of "V-Synth" (run
   `analysis/test_files/vc2/fxplay --in x` to list the inputs):
   ```bash
   analysis/test_files/vc2/fxplay --in "V-Synth" 120 > analysis/test_files/vc2/learn.txt
   ```
3. Within those 2 minutes, at the panel, move each of these one step and back: **Direct Sound Level**, **MFX Type**,
   **Reverb Type**, **Master Level**, then **Vocoder Type** and **External In Type**.
4. Then:
   ```bash
   .venv/bin/python3 research/tools/vc2capture.py --learn analysis/test_files/vc2/learn.txt
   ```
   Each line names the parameter the runs would set at that address.  The first four should give lines naming what you moved
   (a type's change may bring its parameters' lines with it); Vocoder Type and External In Type should give none, as the VC-2
   has no address for them.  If a line says "not a DT1 of model 00 00 0D", names a different parameter, or either of the last two sends
   something, stop and tell Claude: the map isn't the XT's.  If nothing at all arrived, check the MIDI cable from the XT and
   that Transmit Edit Data stayed on.
5. **Transmit Edit Data OFF** again.

**5. Test, 23 seconds.**  Turn the monitors down.  Run `analysis/test_files/vc2/fxplay --audio-devices` for your interface's
name, then, with yours in place of "Interface" and "V-Synth":
```bash
analysis/test_files/vc2/fxplay --audio analysis/test_files/vc2/vc2probe.wav --device "Interface" analysis/test_files/vc2/vc2_test.mid "V-Synth"
```
After the 5 s countdown the XT's screen changes to patch RUN 001 (the test selects it from the card), then you should hear the
test signal through the XT: three clicks, then a rising sweep, dropping in level halfway (Master Level, so a System write
works).  MFX Type changes to Parametric EQ on its screen, and back at the end.  If there's no sound: the cable into the INPUT
jacks, INPUT Jack Switch, or the interface's own output routing.  If there's sound but no change of level or type: Receive
Exclusive and Device ID.  If the patch doesn't change: Receive Program Change, Receive Bank Select and Part 1 Receive Channel.

**6. The level.**  Set your interface's output so the XT's **PEAK** light stays off through the sweep, and lights at most during
the last second of the 1 kHz steps (each take's steps end at 0 dBFS on purpose, to find where the input clips).  Then leave the
interface's output level alone for every run, and write it down (question d-4).

**7. Run 1, 12 minutes.**  Start recording, then:
```bash
analysis/test_files/vc2/fxplay --audio analysis/test_files/vc2/vc2probe.wav --device "Interface" analysis/test_files/vc2/vc2_run_1.mid "V-Synth"
```
Six times in this run fxplay prints a line such as `>>> by hand at the XT's panel, now: SYSTEM > Common > IO > Output Gain:
+6 dB` and the run pauses 20 s.  Set what it says before the pause ends; the run carries on by itself.  Two of the pauses
set Output Gain; the other four, External In Type, which step 3 says may have nothing to set.  Otherwise leave the XT alone
until it says done, then stop, and save as `analysis/test_files/vc2/vc2_run_1.wav`.  A setting made late shows in the check
below as that take's LOOK or FAIL: then run 1 again.  Check it before run 2:
```bash
.venv/bin/python3 research/tools/vc2cut.py analysis/test_files/vc2/vc2_run_1.wav
```
If step 3 found no External In Type, add `--extin-not-found`:
```bash
.venv/bin/python3 research/tools/vc2cut.py analysis/test_files/vc2/vc2_run_1.wav --extin-not-found
```
Then its four External In Type takes are read as repeats of one setting, each held to the others' sides and level.  If
they differ all the same, a LOOK says the XT took a setting after all.  The last line says whether to go on.  FAIL lines say what to fix; LOOK lines are worth reading and stop nothing.  A finding that
only the XT can give, such as which input side the direct sound carries, is an INFO or a LOOK, never a FAIL.  The tool also cuts
the takes into a folder beside the recording.  Or tell Claude, and it'll run this and read the rest.

**8. Runs 2, 3 and 4.**  The same, with `vc2_run_2.mid` (the vocoder, 16 minutes), `vc2_run_3.mid` (rom.4's MFX, 18 minutes)
and `vc2_run_4.mid` (its Chorus and Reverbs, 10 minutes), each saved under its own name, each checked before the next.

**9. The V-Synth-mode run, 2 minutes.**  Press [V-CARD] back to the V-Synth.  On any patch, PATCH Init; the run sets OSC 1 to EXT
IN itself.  SYSTEM: Rx Sys-Ex ON, Device ID 17, Part 1 Rx Sw ON, Rx Ch 1, as for the effects captures.  Then `vc2_vsynth.mid`
as in step 7, saved as `vc2_vsynth.wav`, and checked the same way.

**10. Tell Claude** the paths you used, the interface and its output level, and anything the checks said.  No normalizing, fades
or trimming: the takes are found by their timing and the input's clicks.

**If a run gets interrupted:** `--from <seconds>` before `--audio` picks it up, with a `next_s` from `vc2_takes.csv`.  It sends
the patch choices and SysEx it skips first, each patch with its load.  It can't make what's set by hand: before you start, set
Output Gain and External In Type as the take you start from lists them (step 3's when it lists neither).  Save that part as a
separate recording, and give `vc2cut.py` the same `--from <seconds>`.

**From a DAW instead of fxplay:** `vc2capture.py --input-wavs` also writes each run's whole input as `vc2_run_N_in.wav` (about
150 to 280 MB each).  Put it and the run's `.mid` at the same start, the MIDI to the XT and the audio to the outputs feeding the
INPUT jacks, and record the XT.  The DAW must send the file's SysEx, Bank Select and Program Change.  It won't show run 1's
pauses: their times are the `hand:` markers in the file, and the take list's settings say "by hand".

**If SysEx can't be made to work:** `vc2_takes.csv` lists every take's settings in the panel's words.  By hand, the takes that
answer the most are run 1's `main`, `direct_64`, `extin_stereo`, `extin_monol` and `extin_monor`, and run 2's `main` and
`legacy`.  For each: choose the patch its settings begin with (`patch 002` is RUN 002), set what the take lists, then play
`vc2probe.wav` into the input from any player, holding C4 for
its 32.5 s.  Run 2's takes are marked: Direct Sound Level 127 until the input's sweep ends (11.5 s in), then as the take lists, and
C4 held only from then on.  Record each take on its own, and check it with `vc2cut.py <file> --run vc2_run_N --take <name>`.

---

## Recording Chain

```
DAW (playback) → Import and encode PCM sample →  V-Synth PCM oscillator → V-Synth audio output → DAW (capture)
```

- Sample rate: **48000 Hz** (V-Synth native)
- Bit depth: **24-bit PCM** (current test files are 24-bit; 16-bit also handled)
- Channels: **stereo** (capture as-is; renderer downmixes to mono automatically)
- Trim: set so the passthrough signal peaks around **−3 to −6 dBFS** — not too quiet, not clipping

---

## V-Synth Patch: [EMU_TEST]

Document current settings here; update if anything changes between sessions.

| Parameter | Setting | Note |
|---|---|---|
| Oscillator 1 type | **External (EXT)** | Routes audio input through VariPhrase |
| Oscillator 2 | Off | Avoids contamination from onboard PCM |
| VariPhrase mode | **SOLO** | Single-voice; deterministic behavior |
| Effects (FX) | **ALL OFF** | Isolates VariPhrase algorithm |
| COSM | **OFF** | |
| Reverb / Chorus | **OFF** | |
| TVF (filter) | Open / flat | Set cutoff to max, resonance 0 |
| TVA (amp) | Unity gain | No envelope modulation |
| VariPhrase Pitch | As needed | Set per test case |
| VariPhrase Time | As needed | Set per test case |
| VariPhrase Formant | As needed | Set per test case |
| Formant (PCM) page: Energy | **unknown — please read and note** | The references fit Energy **on**, about 28 by the decoded curve (see "Energy check and calibration" below) |
| Formant (PCM) page: Formant KF, Fmt LFO Depth, formant envelope depth | **unknown — please read and note** | A non-zero value would explain the formant offset in the May sine takes |

**Critical:** zero all VariPhrase parameters first, confirm passthrough is clean (null test
should show > 40 dB null when comparing passthrough to input), THEN set the test parameter.

---

## File Naming Convention

```
{signal}_{parameter}_{value}.wav
```

Examples:
- `vocal_aah_formant_upmax.wav`    (formant +12 st)
- `drum_hit_time_2x.wav`           (time stretch × 2)
- `chord_Cmaj_pitch_up7st.wav`     (pitch +7 st)

For the matching passthrough (V-Synth bypassed or all params at zero), use the same base name
with `_passthrough` suffix:
- `vocal_aah_passthrough.wav`
- `drum_hit_passthrough.wav`
- `chord_Cmaj_passthrough.wav`

---

## Directory Layout

```
analysis/test_files/
├── passthrough/      ← all unprocessed reference inputs (one per signal type)
└── sustained/        ← all V-Synth processed outputs
```

Every processed file in `sustained/` must have a matching passthrough file in `passthrough/`
with the same base name.  The offline renderer takes the passthrough as its `--input`.

---

## Completed Recordings

### Sustained sine (6 files — done)

| File | Location | Status |
|---|---|---|
| sine_440_pitch_up7st.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_pitch_down12st.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_time_2x.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_time_halfspeed.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_formant_upmax.wav (+12 st) | sustained/ + passthrough/ | ✅ done |
| sine_440_formant_downmax.wav (−12 st) | sustained/ + passthrough/ | ✅ done |

### Held vowel "aah" (7 files — done)

| File | Pitch | Time | Formant | Status |
|---|---|---|---|---|
| vocal_aah_passthrough.wav | 0 | 1× | 0 | ✅ done |
| vocal_aah_time_2x.wav | 0 | **2×** | 0 | ✅ done |
| vocal_aah_time_halfspeed.wav | 0 | **0.5×** | 0 | ✅ done |
| vocal_aah_pitch_up7st.wav | **+7 st** | 1× | 0 | ✅ done |
| vocal_aah_pitch_down12st.wav | **−12 st** | 1× | 0 | ✅ done |
| vocal_aah_formant_up4st.wav | 0 | 1× | **+4 st** | ✅ done |
| vocal_aah_formant_upmax.wav | 0 | 1× | **+12 st** | ✅ done |
| vocal_aah_formant_downmax.wav | 0 | 1× | **−12 st** | ✅ done |

### Single drum hit (4 files — done)

| File | Pitch | Time | Formant | Status |
|---|---|---|---|---|
| drum_hit_passthrough.wav | 0 | 1× | 0 | ✅ done |
| drum_hit_time_2x.wav | 0 | **2×** | 0 | ✅ done |
| drum_hit_time_4x.wav | 0 | **4×** | 0 | ✅ done |
| drum_hit_time_halfspeed.wav | 0 | **0.5×** | 0 | ✅ done |
| drum_hit_pitch_up7st.wav | **+7 st** | 1× | 0 | ✅ done |

### Chord C major (3 files — done)

| File | Pitch | Time | Formant | Status |
|---|---|---|---|---|
| chord_Cmaj_passthrough.wav | 0 | 1× | 0 | ✅ done |
| chord_Cmaj_time_2x.wav | 0 | **2×** | 0 | ✅ done |
| chord_Cmaj_pitch_up7st.wav | **+7 st** | 1× | 0 | ✅ done |
| chord_Cmaj_formant_max.wav | 0 | 1× | **+12 st** | ✅ done |

---

## Sine Re-record (added 2026-10-18) — do this first

**Why:** in 5 of the 6 May sine takes (all except formant +63), the decoded
firmware SOLO player only matches if the formant sits an octave low (read
speed ½). Pitch −12 came out as a *pure* 220 Hz tone, which the SOLO player
produces only with formant at its minimum; at formant 0 it keeps the 440 Hz
content. The takes were made changing one parameter at a time, so something
else was probably lowering the formant: **Formant KF** (key follow,
relative to C4), the formant envelope or the formant LFO (PCM Formant
screen, manual p. 92). The vocal takes show no such offset.
(notes.txt, "SINE REFERENCES EXPLAINED")

**Setup:** wave 003 (SOLO), PCM oscillator with Vari ON. On the Formant (PCM)
screen set **Formant 0, Formant KF 0, Fmt LFO Depth 0**, formant envelope depth
0, Energy OFF; Robot voice off. Play the key that sounds 440 Hz and **note
which key**. Same capture chain as the vocal takes (48 kHz stereo); ~0.5 s
lead-in; record each take to its natural end (time ½ runs about 8 s; the May take
stopped at 4 s). Save with an `_r2` suffix so the May takes stay for
comparison.

| Take | Pitch | Time | Formant | Note |
|---|---|---|---|---|
| `sine_440_identity_r2` | 0 | 1× | 0 | **Decisive.** Should be a pure 440 Hz sine; overtones (2nd harmonic ≈ −18 dB) mean the formant is still offset |
| `sine_440_pitch_down12st_r2` | −12 | 1× | 0 | |
| `sine_440_pitch_up7st_r2` | +7 | 1× | 0 | |
| `sine_440_time_2x_r2` | 0 | 2× faster | 0 | |
| `sine_440_time_halfspeed_r2` | 0 | ½ speed | 0 | full length |
| `sine_440_formant_downmax_r2` | 0 | 1× | −63 | optional: same chain as the rest |
| `sine_440_formant_upmax_r2` | 0 | 1× | +63 | optional |

If the identity take is clean but the May takes are reproduced when you put
back a setting (e.g. Formant KF), record which setting it was: that tells us
how the firmware combines KF with the knob.

## Energy check and calibration (added 2026-10-28) — no new takes needed for step 1

**Why:** the output-chain study (v40) found that every vocal reference has
its fundamental 1.6–1.9 dB low and the content below it cut. That pattern
follows F0 under pitch shift, which is what the SOLO player's **Energy**
high-pass does when Energy is on. The engine reproduces it with a small
internal amount (g ≈ 0.05). The firmware maps the Energy knob (OFF, 1–127)
to g = (k/127)² (v51), so that fit says Energy ≈ 28.
(research/HARDWARE_FIRMWARE.md, "Output chain" and "The VariPhrase
converter")

**Step 1 — read the EMU-TEST patch (no recording).** On the XT, open the
EMU-TEST patch, then OSC → **Formant (PCM)** page (manual p. 92). Note these
values in the patch table above:

| Parameter | Value |
|---|---|
| Energy | |
| Formant KF | |
| Fmt LFO Depth | |
| Formant envelope depth (formant envelope screen) | |

**Step 2 — Energy calibration takes (now optional: they check the decoded
mapping, g = 0, 0.063, 0.254 and 1 for the four values below).** Use wave 004 (the SOLO vocal), the
same capture chain as the other vocal takes (48 kHz stereo), and ~0.5 s of
lead-in. Pitch **−12** for every take, because the Energy amount only
changes the sound when the pitch is shifted. Keep every other setting as in
the original vocal takes and change only Energy:

| Take | Pitch | Time | Formant | Energy |
|---|---|---|---|---|
| `vocal_aah_pitch_down12st_energy_off` | −12 | 1× | 0 | OFF |
| `vocal_aah_pitch_down12st_energy_032` | −12 | 1× | 0 | 32 |
| `vocal_aah_pitch_down12st_energy_064` | −12 | 1× | 0 | 64 |
| `vocal_aah_pitch_down12st_energy_127` | −12 | 1× | 0 | 127 |

Optional: repeat at pitch **+7** with the same four Energy values. The +7
damping depends on g differently (1.2 − 0.5g against 1.2 − g at −12), so
those takes cross-check the mapping.

Save them in `sustained/`. The plugin's Energy knob now uses the decoded
curve, so they can be rendered at the same knob values and compared.

## Repeat-budget check (added 2026-10-28) — one take

**Why:** each SOLO record carries a repeat budget (v38: the XT's encoder
writes it; our encoder now reproduces it bit-exactly, v39). A budget only
changes the sound when a record has to repeat three or more times, i.e. at
slowdowns of about 4× or more. Every take so far is at ½ speed or faster, so
the budget behaviour has only been checked against the firmware emulator,
not the hardware. (research/HARDWARE_FIRMWARE.md, "How the encoder sets the
repeat budget")

**Setup:** wave 004 (SOLO vocal), with the same patch settings and capture
chain as the other vocal takes (including the Energy setting, whatever it
is). Pitch 0, Formant 0, Time at **¼ speed (0.25×, 4× slower)**. Record to
the natural end: about 14 s of audio for the 3.5 s source.

| Take | Pitch | Time | Formant | Note |
|---|---|---|---|---|
| `vocal_aah_time_quarterspeed` | 0 | **0.25×** | 0 | full length |

`render_suite.py` already maps it (`--time 4.0`), so it scores automatically
once it's in `sustained/`.

## Re-record the drum references (added 2026-10-28)

**Why:** the existing drum takes don't match their settings, so they can't
judge the drum engine.
- `drum_hit_pitch_up7st` has the same spectral peaks as the dry sample
  (62 / 218 / 669 Hz): it was not transposed.
- `drum_hit_time_2x` and `drum_hit_time_halfspeed` have peaks that are
  neither the dry sample's nor a time-stretched copy's.

Rendered through the XT's own firmware BACKING chain, the drum transposes
correctly but scores only level with the old engine against these takes.

**Steps:**
1. Import `raw/drum_hit_original.wav` and encode it as **BACKING**. Note
   the **Encode Depth**; any value is fine, the firmware finds one attack
   at every depth.
2. With the patch's other settings at their EMU-TEST values, record:
   `drum_hit_pitch_up7st` (pitch **+7** exactly), `drum_hit_time_2x`,
   `drum_hit_time_4x` and `drum_hit_time_halfspeed`.
3. Before each take, check on the display that the Pitch and Time values
   are as named.

## XT encodes of the chord and drum (added 2026-10-28) — no audio capture needed

**Why:** the BACKING/ENSEMBLE work needs the XT's own analysis of our exact
inputs, as waves 003/004 gave for SOLO (where the encoder port now
reproduces the XT bit-exactly). Every BACKING/ENSEMBLE wave in Emu_test.prj
(362, 376, 377, 380, 381, 415) is factory content stamped "V-Synth Ver.1.50"
— encoded on an original V-Synth in 2003, not on the XT — and that version's
encoder is measurably different (see "Factory waves" in
research/HARDWARE_FIRMWARE.md). So these four encodes are the only possible
ground truth for the XT's multi-band encoder. With them we can:
- run the XT's BACKING and ENSEMBLE players on exactly what the hardware
  stored, and score them against the existing chord and drum references;
- validate the emulated multi-band encoder.

(research/HARDWARE_FIRMWARE.md, "BACKING / ENSEMBLE: first findings")

**Question first:** which **Encode Type** did you use for the existing drum
takes (BACKING or ENSEMBLE), and which **Encode Depth**? *(The chord is
answered: rendered through the XT's own firmware, ENSEMBLE scores 87.1 and
BACKING 62.9 against its references, so the chord was ENSEMBLE. The drum
scores below the current engine either way, 66.7 BACKING / 69.4 ENSEMBLE,
so its settings, for example attack events from Encode Depth, matter.)*

**Steps:** import `raw/chord_Cmaj_original.wav` and
`raw/drum_hit_original.wav` into the XT (note whether the XT converts them
from 48 kHz). Encode each **twice**, once as BACKING and once as ENSEMBLE,
keeping Encode Depth at whatever you normally use (note the value). Then save
the project, as with Emu_test.prj, and copy it into `analysis/projects/`.

| Wave | Source | Encode Type |
|---|---|---|
| `chord_backing` | raw/chord_Cmaj_original.wav | BACKING |
| `chord_ensemble` | raw/chord_Cmaj_original.wav | ENSEMBLE |
| `drum_backing` | raw/drum_hit_original.wav | BACKING |
| `drum_ensemble` | raw/drum_hit_original.wav | ENSEMBLE |

## Amp envelope timing (added 2026-10-29) — two short takes

**Why (optional since v46):** the plugin's amp envelope is the XT's own
generator and TVA, matched against the firmware. Its 10 ms update clock
is now confirmed in the firmware: the TVA ramps over exactly 441 samples
per update (research/HARDWARE_FIRMWARE.md, "The envelope generator").
These two takes are a final check against the hardware: one for the stage
times, and one for the shape of a slow ramp.

**Setup:** an INIT patch with OSC1 playing a plain sustained PCM wave (a
sine or any steady tone), VariPhrase OFF, no LFO or filter movement. Set
the envelope on **OSC1's amp envelope** (OSC → TVA envelope). If it is
easier, use the patch TVA envelope instead and note which one you used.
Hold C4 for 6 s, then release, and record 10 s in all, as for the other
takes.

| File | A | D | S | R | What it shows |
|---|---|---|---|---|---|
| `env_64_64_64_64` | 64 | 64 | 64 | 64 | each stage should take 2.13 s; sustain about −12 dB |
| `env_slow_attack` | 100 | 0 | 127 | 64 | a 10.4 s ramp: smooth or in 10 ms steps |

## Next Recording Session — Priorities

**Superseded by the "Round 2 — Recording Wishlist" section at the end of this
file** (updated after v27).  Optional extras kept from the earlier list, worth
grabbing only if time permits after the Round-2 priorities:

| File | Pitch | Time | Formant | Why |
|---|---|---|---|---|
| `vocal_aah_time_1_5x_dry`/ref | 0 | **1.5×** | 0 | mid-ratio quality curve |
| `chord_Cmaj_time_halfspeed` | 0 | **0.5×** | 0 | subband engine at compression |
| `chord_Cmaj_pitch_down12st` | **−12 st** | 1× | 0 | ENSEMBLE pitch, down direction |

---

## Rendering Plugin Outputs

(v191 removed the legacy engine this renderer is built from. `analysis/variphrase_render` still runs
as last built; to rebuild it, run the command below in a checkout of v190, 164ac15. The firmware's
own path renders with the XtSynth CLI, `research/tools/ab.sh`.)

After recording, rebuild the renderer (from the project root):

```bash
cd plugin/Source && g++ -std=c++20 -O2 -DOFFLINE_RENDERER_MAIN \
    OfflineRenderer.cpp VariphraseEngine.cpp \
    PhaseVocoder.cpp SourceFilterModel.cpp \
    -o ../../analysis/variphrase_render
```

Then render each new test case.  **Always use `--algo hybrid`** — the hybrid algorithm
runs the offline encode pass (`analyzeContent`) automatically and picks the best
sub-algorithm for the content type:

```bash
BASE=/path/to/vsynth-emu/analysis
R="$BASE/variphrase_render"
VER=hybrid_v17c      # increment for each new batch

mkdir -p "$BASE/plugin_outputs/$VER/sustained"

# Time stretch — WSOLA will be selected automatically for BACKING/ENSEMBLE content
"$R" --input  "$BASE/test_files/passthrough/chord_Cmaj_passthrough.wav" \
     --output "$BASE/plugin_outputs/$VER/sustained/chord_Cmaj_time_2x.wav" \
     --time 2.0 --algo hybrid

# Pitch shift — PV or LPC selected based on content type + voiced detection
"$R" --input  "$BASE/test_files/passthrough/vocal_aah_passthrough.wav" \
     --output "$BASE/plugin_outputs/$VER/sustained/vocal_aah_pitch_up7st.wav" \
     --pitch 7 --algo hybrid

# Formant shift — LPC always selected when |formant| > 0.5 st
"$R" --input  "$BASE/test_files/passthrough/vocal_aah_passthrough.wav" \
     --output "$BASE/plugin_outputs/$VER/sustained/vocal_aah_formant_upmax.wav" \
     --formant 12 --algo hybrid
```

**Current hybrid routing summary (v27 — see research/PATENTS.md):**
| Content type | Operation | Engine |
|---|---|---|
| SOLO (speech/melody) | any | pitch-synchronous granular (US6421642) |
| LITE (pure tones) | time, pitch-up | LPC resynthesis |
| LITE | pitch-down | Phase Vocoder |
| LITE | formant | LPC source-filter |
| ENSEMBLE (chords) | time-only | subband stretch (US6564187) |
| ENSEMBLE | pitch/formant | Phase Vocoder / LPC |
| BACKING (drums) | time-only | event-based stretch (attacks verbatim) |
| BACKING | pitch | Phase Vocoder |

The content type (LITE / SOLO / ENSEMBLE / BACKING) is printed to stdout during render:
```
Content: BACKING  medianConf=0.895  peakToMean=7.80
```

Then run the full batch test:

```bash
cd "$BASE" && python3 batch_test.py \
    --ref-dir    test_files/sustained \
    --plugin-dir plugin_outputs/$VER/sustained \
    --output     results/$VER
```

---

## Tips for Clean Recordings

1. **Zero all VariPhrase parameters before each take.** Confirm with a passthrough null
   test before setting the test parameter.
2. **Record a few seconds of silence before playing the signal.** This lets the batch
   test measure the noise floor and gives the analyzeContent pass enough samples to
   classify content correctly.
3. **Use the same audio interface and cable for all takes in a session** so the output
   chain is consistent.
4. **Label takes immediately** — do not rely on memory.  Record the V-Synth panel display
   settings in a photo alongside the audio file.
5. **Check the recording with compare.py before moving on** — a quick null test against
   the passthrough reveals miswired settings immediately.
6. **Verify content classification** after recording by running a quick render and checking
   the `Content:` line in the output.  If classification looks wrong, check signal length
   (< 2048 samples returns LITE by default) and silence threshold.

---

## Round 2 — Recording Wishlist (updated after v27, score 51.5)

Session 15 ended with all three Roland VariPhrase patents implemented in
their correct roles (see research/PATENTS.md): pitch-synchronous granular for
SOLO (US6421642), quarter-octave subband stretch for ENSEMBLE (US6564187),
and event stamps for BACKING.  The remaining ceiling is **coverage- and
metric-bound**: the suite is 20 cases from 4 source recordings, several
behaviours are calibrated against a single example, and the open questions
below can only be answered with new hardware captures.

**If hardware time is limited, record ① + ⑥ only** (nine takes of one vocal
source) — they answer the two most important open questions: the true
formant transfer curve and the metric ceiling.

### Ground rules for every take

- **Keep the RAW source of every sample in `test_files/raw/`** — the file
  you sampled into the V-Synth.  The engine must render from the raw source,
  never from a V-Synth capture: the V-Synth is not transparent at zero
  settings (Session 16: using the vocal passthrough capture as input cost
  ~9 points).  Chord and drum originals: `raw/*_original.wav` (identical to
  their old passthrough files).  Still needed: a real V-Synth LITE capture
  of the sine at zero settings (`passthrough/sine_400hz_raw_lite.wav` is a
  copy of the raw sine).
- **Capture the passthrough first** for every new source (all VariPhrase
  parameters zeroed).  Reference levels are recording-chain gain staging
  (the sine refs sit ~7 dB below their dry input; vocal/chord at unity);
  a passthrough per source pins down the unit's level law.
- **Name dry inputs with a `_dry` suffix.**  The sine set's identical names
  across passthrough/ and sustained/ caused a wrong-input render and an
  inflated score in Session 15.
- **Short, consistent lead-ins (~0.5 s)** and **≥ 4 s of steady content** for
  sustained sources (analysis windows reach 65 k samples).
- **Note the exact displayed parameter values** — the panel-to-DSP mapping is
  part of what is being reverse-engineered.

### Priority ① — Formant-knob calibration sweep (7 takes)

Vocal "aah" at formant settings:
```
−9, −6, −3, +2, +3, +6, +9   (semitone display values, everything else zero)
```
Now does double duty:
- Maps the full formant transfer curve (currently measured from only two
  points: "+12 st" moves F1 ~×1.67; downward tracks nominal).
- **Directly tests the patent's window clamp** (`wl ≤ ppw`, US6421642), which
  replaced the empirical 0.75× calibration in the granular engine — the
  engine now makes a specific, falsifiable prediction at every knob setting.
- Resolves whether the down-shift's odd upper structure (reference peaks at
  3170/4172 Hz that don't fit ×0.5 scaling) is real behaviour or artifact.

### Priority ② — A second vowel and a higher voice (6 takes)

All vocal behaviour is tuned on ONE low-pitched "aah".
- **"ee" vowel, sustained** (high F2 ≈ 2.3 kHz, low F1 — the hardest envelope
  configuration): formant +12, −12, pitch +7.
- **Higher-F0 voice** (female or falsetto, F0 ≈ 200–250 Hz), sustained "aah":
  same three takes.  Stresses the granular engine's period tracker and
  integer-subharmonic octave guard.

### Priority ③ — Drum loop with multiple onsets (4 takes)

A 2–4 bar kick/snare/hat loop at a known tempo: time 2×, time 0.5×,
pitch +7 st, passthrough.  The event-stamp system has only ever been tested
against a SINGLE drum hit; a loop exercises multi-onset detection, the
2048-sample refractory window, inter-event segment stretching, and cumulative
timing placement.

### Priority ④ — Sustained pad chord (4 takes)

A slow-attack polyphonic sustain (organ/strings patch): time 2×, pitch +7,
formant +12, passthrough.  Two jobs:
- Tests the ENSEMBLE/BACKING classifier boundary (peakToMean > 9, set from
  one strummed chord at 7.8 vs one drum at 11.4) without the borderline
  attack transient.
- **Direct validation for the new subband engine** (US6564187, v27) on clean
  polyphony; the pitch take informs the planned subband-pitch extension
  (chord_pitch is a queued target).

### Priority ⑤ — Moving-pitch phrase (4 takes)

A 2–4 s sung or spoken phrase with natural pitch movement: time 2×,
time 0.5×, pitch +7, passthrough.  This is what VariPhrase exists for, and
the real test of the granular encoder's grain tracking — everything in the
current suite is stationary.

### Priority ⑥ — Duplicate takes (cheap; do alongside anything)

Record the SAME setting twice without touching anything (e.g. vocal formant
+12, two takes).  Scoring take A against take B through compare.py measures
the recording-chain + metric noise floor — the best score ANY render can
achieve.  With the suite at 51.5, knowing whether the ceiling is 65 or 85
determines how much algorithm work actually remains.
