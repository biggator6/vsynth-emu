# Trusting the V-Synth Emu plugins on macOS and Windows

V-Synth Emu isn't signed with an Apple or Microsoft developer certificate, and it isn't notarized. A copy you
build on your own computer is trusted already. A copy that came from somewhere else (a download, a zip, AirDrop, a
USB stick) is marked as untrusted, and the system or the host may refuse to load it. Here's how to clear that.

Only do this for files from a source you trust. These marks are what protect you from tampered copies.

## What gets installed, and where

| | macOS (for you alone) | Windows |
|---|---|---|
| Instrument, AU | `~/Library/Audio/Plug-Ins/Components/V-Synth Emu.component` | (no AU on Windows) |
| Effect, AU | `~/Library/Audio/Plug-Ins/Components/V-Synth Emu FX.component` | (no AU on Windows) |
| Instrument, VST3 | `~/Library/Audio/Plug-Ins/VST3/V-Synth Emu.vst3` | `C:\Program Files\Common Files\VST3\V-Synth Emu.vst3` |
| Effect, VST3 | `~/Library/Audio/Plug-Ins/VST3/V-Synth Emu FX.vst3` | `C:\Program Files\Common Files\VST3\V-Synth Emu FX.vst3` |

On macOS, the same folders under `/Library` (no `~`) install for every user of the Mac. Each `.component` and
`.vst3` is a folder (a bundle): copy and treat it whole. The project doesn't publish a Windows build yet, so the
Windows part applies to a VST3 built on Windows from this project.

## macOS

**Built on this Mac:** nothing to do. The build signs the plugins ad hoc and nothing is quarantined. Checked with
`codesign --verify --strict`, a locally built AU says "code has no resources but signature indicates they must be
present". That's the linker's signature, and the AU still loads and passes `auval`. Re-sign it as below only if a
host won't load it.

**From anywhere else:** every bundle needs it, the VST3s as well as the AUs. This does all four in Terminal, skipping
any you didn't install:

```bash
cd ~/Library/Audio/Plug-Ins
for b in "Components/V-Synth Emu.component" "Components/V-Synth Emu FX.component" \
         "VST3/V-Synth Emu.vst3" "VST3/V-Synth Emu FX.vst3"; do
  [ -e "$b" ] || continue
  xattr -cr "$b"
  codesign --force --deep --sign - "$b"
  codesign --verify --deep --strict --verbose=2 "$b"
done
```

For a copy installed for every user, start with `cd /Library/Audio/Plug-Ins` instead, and put `sudo` before each of
the first two commands in the loop. For one bundle alone, the same three commands, for example the instrument's VST3:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/"V-Synth Emu.vst3"
```

```bash
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/"V-Synth Emu.vst3"
```

```bash
codesign --verify --deep --strict --verbose=2 ~/Library/Audio/Plug-Ins/VST3/"V-Synth Emu.vst3"
```

- `xattr -cr` removes the quarantine mark and the other file attributes a download or copy adds. It must come
  first: with them still there, `codesign` stops with "resource fork, Finder information, or similar detritus not
  allowed".
- `codesign --force --deep --sign -` signs the bundle ad hoc for this Mac. Apple silicon Macs won't load code with
  no valid signature, and a copy from another Mac may have lost or broken its signature on the way.
- The last command should say "valid on disk" and "satisfies its Designated Requirement" for each bundle.

**Without Terminal:** load the plugin once in your host. When macOS says it couldn't verify it, go to System
Settings, Privacy & Security, scroll to Security, and click Open Anyway for the plugin. Then load it again. On
macOS 15 and later, the old Control-click and Open route no longer gets past this. If Open Anyway doesn't appear or
the host still won't load the plugin, use the Terminal steps.

**Make the host look again:**
- AU: macOS caches the list of AUs. Run `killall -9 AudioComponentRegistrar`, then check each one with
  `auval -v aumu VsEm Gato` (the instrument) and `auval -v aumf VsEf Gato` (the effect). Each should end with
  "AU VALIDATION SUCCEEDED".
- Logic Pro: Settings, Plug-in Manager, select the plugin, Reset & Rescan Selection.
- REAPER: Settings, Plug-ins, VST, then Re-scan (or "Clear cache/re-scan" if it remembers a failed load). AUs are
  under Plug-ins, AU.
- Other hosts: their plug-in manager's rescan.

## Windows

1. **Before unzipping:** right-click the `.zip`, choose Properties, tick Unblock at the bottom of the General tab,
   click OK, then extract. Otherwise Windows marks every extracted file as coming from the internet.
2. **Install:** copy the whole `.vst3` folder into `C:\Program Files\Common Files\VST3\`. Windows asks for
   administrator permission there.
3. **If it was already extracted:** clear the mark from PowerShell, run as administrator:

   ```powershell
   Get-ChildItem -LiteralPath "C:\Program Files\Common Files\VST3\V-Synth Emu.vst3" -Recurse | Unblock-File
   ```

4. **Smart App Control** (Windows 11) blocks unsigned code, plugins included. If Windows Security shows a Smart
   App Control block when the host loads the plugin, the only way past it is to turn Smart App Control off (Windows
   Security, App & browser control, Smart App Control settings). That's your call to make: Microsoft has said it
   can't simply be switched back on afterwards, so read their current note first.
5. **Antivirus:** if Microsoft Defender or another antivirus removed or blocked the file, restore it and allow it
   (Windows Security, Virus & threat protection, Protection history), then copy it in again if it was deleted.
6. **Rescan in the host:** REAPER: Options, Preferences, Plug-ins, VST, Re-scan. Other hosts: their plug-in
   manager's rescan.

## Still not loading

- Check that the copy the host loads is the one you think it is. A host can find an older copy in another folder
  (for example `/Library` as well as `~/Library` on macOS) and load that one instead. Remove the copies you don't
  want, then rescan.
- The plugin plays nothing without your own V-Synth XT firmware image (`V-SynthXT.rom.2.bin`), chosen with the
  ROM... button in the editor's header. That isn't a trust problem: the plugin loads, but stays silent until it has
  the image.
