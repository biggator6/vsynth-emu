# V-Synth Controllers — MIDI Surface and Emulation Requirements

Reference for the V-Synth / V-Synth XT physical controllers as seen from MIDI, and what
the plugin needs in order to reproduce them.  Derived from the **MIDI Implementation**
appendix of the V-Synth Owner's Manual (dated Nov. 20 2002, chart version 1.00),
cross-checked against the V-Synth XT Owner's Manual and XT Supplementary Manual.
Sources are listed at the bottom.

Every SysEx address below comes from the **V-Synth** manual.  The XT Owner's Manual
carries only the MIDI Implementation Chart, not the parameter address map — Roland ship
the XT's full MIDI Implementation as a separate download.  The XT is assumed to share
these addresses (same OS, same parameter set) but that is **not verified**.

Current scope: the **Time Trip Pad**.  D Beam, C1/C2 (E1–E8 on the XT) and the pedal
inputs share the same assignable-CC machinery and are stubbed in below for later.


## Time Trip Pad

### What it is, as a signal source

An XY touch surface with touch detection, a patch-level mode switch (`Pad Mode`:
`X-Y` / `TIME-TRIP`) and a `Hold` latch.  It produces six internal signals, of which
five are exposed as Matrix Control sources:

| Internal signal | Matrix source | Manual's wording                          |
|-----------------|---------------|-------------------------------------------|
| X, bipolar      | `PAD-X`       | "Time Trip pad (horizontal direction)"    |
| Y, bipolar      | `PAD-Y`       | "Time Trip pad (vertical direction)"      |
| abs(X) from centre | `+PAD-X`   | "horizontal direction **from the center**" |
| abs(Y) from centre | `+PAD-Y`   | "vertical direction **from the center**"   |
| Radius          | `TRIP-R`      | "**center from the circumference**"       |
| **Angle θ**     | *not exposed* | consumed internally by the Time Trip function |
| **Touch gate**  | *not exposed* | front-panel indicator only; implied by value 0 in MIDI |

The key asymmetry: **the pad works in polar coordinates but transmits Cartesian ones.**
θ never leaves the instrument, and θ is what drives the actual Time Trip scrub.

### MIDI surface — four assignable CCs

`SYSTEM → TT Pad/Knob` holds four independent CC assignments, backed by SysEx in the
**System Controller** block (base `02 00 40 00`):

| Parameter   | SysEx offset | Range                        | Default        |
|-------------|--------------|------------------------------|----------------|
| `X Assign-XY` → *Pad Assign X*       | `00 10` | `OFF, CC01–31, CC33–95` (0–95) | CC80 (GP Ctrl 5) |
| `Y Assign-XY` → *Pad Assign Y*       | `00 11` | same                           | CC81 (GP Ctrl 6) |
| `X Assign-TT` → *Time Trip Assign X* | `00 12` | same                           | CC82 (GP Ctrl 7) |
| `Y Assign-TT` → *Time Trip Assign Y* | `00 13` | same                           | CC83 (GP Ctrl 8) |

The default assignments are confirmed by Roland's own **MIDI Implementation Chart**
(p. 170), which names each one outright:

| CC | Transmitted       | Recognized | MIDI name                    |
|----|-------------------|------------|------------------------------|
| 80 | O (`TT Pad XY-X`) | X          | General purpose controller 5 |
| 81 | O (`TT Pad XY-Y`) | X          | General purpose controller 6 |
| 82 | O (`TT Pad TT-X`) | X          | General purpose controller 7 |
| 83 | O (`TT Pad TT-Y`) | X          | General purpose controller 8 |

Two separate CC pairs, one per mode, so XY and Time Trip data can coexist in a single
sequence without collision.

### Reception is not symmetric with transmission

Note the `Recognized: X` column above.  Both columns carry chart footnote *1, "O X is
selectable", so the pad's CCs are **transmitted by default and not recognised by
default**, with recognition contingent on settings.  The only documented route in is
Matrix Control — the receive chapter says an incoming CC takes effect "if the
corresponding Controller number is selected for the Patch Control Source 1–8 parameter".

That carries a consequence worth testing before building around it.  Matrix Control
destinations are oscillator, COSM, TVA, LFO and effect parameters: **there is no
destination for the Time Trip playback pointer.**  So on hardware the Time Trip scrub
looks like a local-only gesture — CC82/83 go out for recording, but feeding them back in
has no documented path to the scrub engine.

A third-party source (Sequence 15) states the opposite: that recorded pad data plays back
and reproduces the gesture.  Roland's chart and receive chapter do not support that.
**Open question, needs a bench test** — send CC82/83 into a V-Synth running a VariPhrase
patch and watch whether the playback pointer moves.  The answer decides whether the
plugin's Time Trip input is a MIDI-reachable parameter or a UI-only gesture.

### Mode, hold and gating — SysEx only, no CC path

**Patch Controller** block, `10 00 08 00` for temporary patch part 1:

| Offset  | Parameter   | Values                            |
|---------|-------------|-----------------------------------|
| `00 00` | Beam Type   | `OFF, 1–4` (option 1 = Time Trip) |
| `00 01` | **Pad Mode** | `X-Y, TIME-TRIP`                 |
| `00 02` | **Pad Hold** | `OFF, ON`                        |

Per-oscillator gating lives in the **Patch Oscillator** block (`00 10 00` offset for
Zone 1):

| Offset  | Parameter                  |
|---------|----------------------------|
| `00 43` | OSC 1 Time Trip Sw         |
| `00 44` | OSC 1 Time Trip Beat Keep  |
| `00 77` | OSC 2 Time Trip Sw         |
| `00 78` | OSC 2 Time Trip Beat Keep  |

Time Trip does nothing unless the oscillator is running VariPhrase (`Vari Sw` ON).

### What MIDI cannot carry

Four gaps the emulation has to solve rather than inherit:

1. **No touch gate.**  Release is encoded as value 0 on both CCs of the active pair —
   indistinguishable from a genuine touch at the bottom-left corner.  The hardware papers
   over this with a physical touch sensor; a plugin receiving CC only cannot.
2. **No angle.**  The receiver must re-derive θ from X/Y, unwrap it, and accumulate Δθ.
3. **7-bit only.**  No MSB/LSB pair is defined for CC80–83, unlike GP1–4
   (CC16–19 / CC48–51), which do have 14-bit pairs.
4. **Hold sends no CC.**  The hardware emits SysEx on hold on/off when Tx Edit is
   enabled, which is why recorded pad passes can leave Hold stuck on.

The 7-bit limit is worse than it first looks: θ quantisation degrades towards the centre
of the pad.  At a radius of 4 counts out of ~63, one count of X is roughly 14° of θ, so
circular gestures near the middle scrub in audible steps.

### Controller inventory for a recreation

Hardware-faithful minimum — six controls:

| # | Control                    | Mapping                                        |
|---|----------------------------|------------------------------------------------|
| 1–2 | Pad X / Y, XY mode       | CC80 / CC81                                    |
| 3–4 | Pad X / Y, Time Trip mode | CC82 / CC83                                   |
| 5 | Pad Mode                   | CC switch (0–63 / 64–127) or SysEx `10 00 08 01` |
| 6 | Pad Hold                   | CC switch or SysEx `10 00 08 02`               |

Plus four more for per-oscillator parity (OSC1/OSC2 Time Trip Sw and Beat Keep).  Matrix
routing itself is patch data, not controllers: 8 slots, `Source` (0–108, including the
five pad sources), two destinations and two sens values each, sens −63…+63.

**Recommended instead:** drive the pad over **MPE**.  One note-on gives a real touch gate
and a touch-down timestamp; per-note slide (CC74) and pressure give X and Y at higher
effective resolution; note-off is an unambiguous release.  Expose CC80–83 purely as a
compatibility input layer so existing V-Synth sequences still play back.  If MPE is off
the table, use 14-bit CC pairs (e.g. CC16/48 and CC17/49) plus a dedicated gate CC.

### Maths the host must implement

```
r      = hypot(x - 64, y - 64) / 63          // 0 at centre, 1 at edge
TRIP-R = 1 - r                               // "center from the circumference"
θ      = atan2(y - 64, x - 64)               // then unwrap across wraps
Δθ    -> playback-pointer offset             // CW = forward, CCW = reverse
```

`TRIP-R`'s polarity is inferred from the manual's wording and is **unverified against
hardware**.

Three behaviours the manual specifies that fall out of this:

- **Touch-down latches position.**  "The currently sounding wave will stop at the current
  playback location" — the pointer freezes on contact, before any movement.
- **Pitch is decoupled.**  Unlike scratching a turntable, scrubbing does not change
  pitch; keyboard note determines pitch throughout.  This is the whole point of
  VariPhrase here.
- **Beat Keep** decides whether, after release, playback chases to where it *would* have
  been had the pad never been touched.

`Hold` freezes the last X/Y "as if a finger remained on the pad" — a latch on the
coordinate pair, not a bypass.


## Other controllers (not yet researched in detail)

Same assignable-CC machinery, **System Controller** block (base `02 00 40 00`):

| Offset        | Parameter               | Range                  |
|---------------|-------------------------|------------------------|
| `00 06`/`00 07` | Beam Sens L / R       | 0–200                  |
| `00 08`–`00 0F` | Beam 1–4 Assign L / R | `OFF, CC01–31, CC33–95` |
| `00 14`/`00 15` | Knob 1 / 2 Assign     | `OFF, CC01–31, CC33–95` |
| `00 17`/`00 19` | Pedal 1 / 2 Assign    | `OFF, CC01–31, CC33–95`, BEND UP/DOWN, AFTERTOUCH |

D Beam Type `1` is Time Trip, so the scrub has a second control source on the hardware.


## Correction to earlier project notes

`ARCHITECTURE.md` and `vsynth_project.md` both previously listed a "ribbon controller"
as needing MPE or custom mapping.  The V-Synth and XT have **no ribbon**.  The expressive
controllers are the Time Trip Pad, the twin D Beam, and C1/C2 (E1–E8 on the XT).  The MPE
argument written for the ribbon applies — and applies well — to the pad.


## Sources

Local copies live in `research/`, with line offsets into the text extractions in
`research/README.md`.

- **V-Synth Owner's Manual** — MIDI Implementation appendix (p. 153 onward), Parameter
  List, and the MIDI Implementation Chart (p. 170–171).  Primary source for every address,
  range and default above.
- **V-Synth XT Owner's Manual** — Controller Settings (p. 128–129) confirms the XT
  carries the same four `X/Y Assign-XY` and `X/Y Assign-TT` parameters with the same
  `OFF, CC01–31, CC33–95` range.  Its chart (p. 201–202) is coarser than the V-Synth's — one
  `1–31, 33–95` row rather than per-CC rows — but agrees: transmitted O, recognized X.
- **V-Synth XT Supplementary Manual** — Matrix Control source list.
- Sequence 15, "Inside the V-Synth, Part 5" (2008) — the "transmits X-Y while working in
  R-θ" observation, and the contested claim that pad data plays back into the engine.
- Sound On Sound, Roland V-Synth review — single-channel controller transmit.
