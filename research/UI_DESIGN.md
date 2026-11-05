# The editor's design

The plugin's user interface follows this design from v144 on, by the user's decision (2026-11-04). It came as a tabbed editor cut from v130, and v144 merged it with the effects' UI that came after. New controls go where this says.

## The window

960 × 600, resizable from 900 × 600 to 1400 × 900. The two card columns share any extra width. Top to bottom:

- **Header:** wordmark with the build under it, Effects switch, Algorithm, ROM status, ROM button.
- **Source bar:** Load WAV, Load Patch, the patch's name, the file's and what the patch asks for that isn't here, OSC 1/2, the OSC's On and Type. It stays in view on every tab, and the Load button and most tabs follow its OSC.
- **Perform column** (left, every tab): Pitch, Time, Formant, Robot. With a ROM these are the selected OSC's, and the title says which.
- **Tabs:**

| Tab | Cards |
|---|---|
| Oscillator | Wave (Vari on its title line), or Analog oscillator when Type is ANALOG · Loop · Tempo sync · Pitch · Level · Velocity |
| Envelopes | Amp (TVA) · Pitch · Time · OSC n Amp · Formant · COSM n |
| LFOs | LFO 1 or 2, by the OSC · Amp LFO (LFO 5) |
| COSM | COSM 1/2, On, Type · the type's own rows · LFO 3 or 4 · MOD · Structure |
| Amp & Pan | Patch level · Velocity · Pan |
| Play | Portamento · Keys · Tune · Tempo |
| Matrix | Matrix, one Control at a time · Controller assign |
| Effects | MFX, Chorus, Reverb, System buttons · the block's card, or Tone and System on the System page · a scrolling list of the type's parameters, or the system's EQ |

## The look

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

## Adding a control

1. Build it in the constructor and attach its parameter, as now.
2. Put it in a card in `buildLayout()`: `Ui::Page::Card` holds a title, controls on the title line (`head`) and rows, and `lr(label, control)` makes the usual row. A card goes in column 0, column 1 or across both (`col = -1`). `sameAs` puts a card in another's place, and `fill` runs one to the page's bottom, for a list that scrolls. A page's `leftW` sets column 0's width. On the Effects tab, `FxPanel` is itself a `Ui::Page` and its cards are in `buildCards()`: its controls stay its direct children, which `TEST_FX_PANEL` relies on.
3. If it shows only in some states, set its visibility where the state changes (`showOscType`, `showCosmType`), and give its card `shown` if the whole card goes.
4. If it follows OSC 1/2 or COSM 1/2, its card's title must say so: `titleFn`, and `repaintPages()` where the selection changes. A card that says nothing reads as belonging to the whole patch.

**What `buildLayout()` doesn't place, it removes.** At its end it takes every child off the editor except those in the pages and its `chrome` list. A control left out compiles, and is simply gone. `TEST_UI_LAYOUT` reports that as a parameter with no control.

A page doesn't scroll, so cards must fit: at the smallest size a page is 466 high, and a card is 38 plus 30 a row, less 4. Only the Effects tab's list scrolls. A COSM type's rows go in the shared three-row area instead (`layoutCosmType`, `showCosmType`, and the card's `extra` list).

## Checks

- `TEST_UI_LAYOUT=1` (in `verify.sh`): at the smallest, default and largest sizes, on every tab, both OSC types, every COSM type, every Effects page and every effect type, no control is outside its parent, over another or without a size, and no text is cut short, the effects' list included, which must show at least 15 rows. Every control is moved, to see that it sets a parameter, and every parameter must have a control. Each tab is opened by a click, and the joined buttons are driven both ways. `=list` prints the parameters with and without a control.
- `mutate_panel.py --editor`: planted errors in the layout, to show that check catches them.
- `TEST_SNAPSHOT=file.png`, with `TEST_SNAPSHOT_TAB=n` and `TEST_SNAPSHOT_SIZE=WxH`: the editor drawn offscreen. It draws only the open tab. `TEST_SNAPSHOT_TYPE` and `TEST_SNAPSHOT_COSM2` open the COSM tab, and `TEST_SNAPSHOT_FX` the Effects tab.

## Not shown anywhere yet

92 of the 631 parameters have no control. Only a host can reach them (`hostOnly` in `PluginTest.cpp`):

- Scale Tune (13).
- The COSMs' Envelope B, and Envelope A's velocity and key follow (each COSM: 10 and 6).
- The COSMs' LFO offset, delay and fade (each COSM: 4).
- Each OSC's pitch, time and formant envelopes' velocity and key (each OSC: 18, v148). A patch sets them.
- The engine's Pitch, Time and Formant (3). These show on the Perform knobs without a ROM.

## Still to do

- The Effects tab shows one block at a time, so you can't see at a glance which of MFX, Chorus and Reverb are on.
- Keep the open tab when the plugin's window closes. It goes back to Oscillator now.
- The envelope bars fill from the left, so a bipolar value of 0 looks half full. The envelope drawing is a sketch of the settings: it doesn't draw Depth or the real timing.
- The constructor's colours are still written as numbers, not `Ui` tokens.
- COSM 1/2 is chosen only on the COSM tab, though the COSM n envelope on the Envelopes tab follows it.
