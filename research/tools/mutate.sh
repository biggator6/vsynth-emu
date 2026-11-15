#!/bin/bash
# mutate.sh <name> <old> <new> [cases] [pattern]: a deliberate error in a port, to
# show its firmware check catches it.  Copies plugin/Source/XtCosm.cpp (with
# PORT=analog, XtAnalog.cpp; with PORT=analogcosm, XtCosm.cpp, PORT=analogband, XtMultiBandPlayer.cpp, and PORT=analogfw, XtFirmware.cpp, under xt_analog's
# check (XT_ANALOG_ONLY=pcm, the PCM pass alone, is passed through); with PORT=tone, XtSynth.cpp, PORT=tonecosm, XtCosm.cpp, PORT=toneanalog, XtAnalog.cpp, PORT=toneband, XtMultiBandPlayer.cpp (XtBeat too), PORT=tonecheck, XtToneCheck.cpp, PORT=tonefw, XtFirmware.cpp, PORT=toneosc, XtOsc.cpp, PORT=tonelfo, XtLfo.cpp, PORT=toneenv, XtEnvelope.cpp, and PORT=tonesolo, XtSoloVoice.cpp, PORT=tonelin, XtLinearPlayer.cpp, PORT=tonelite, XtLitePlayer.cpp, PORT=tonepatch, XtPatch.cpp, and PORT=tonefiles, XtFiles.cpp, under xt_tone,
# on seed $SEED, default 2024; with PORT=solo, XtSoloVoice.cpp under xt_fw solocmp; with PORT=enc, XtSoloEncoder.cpp under
# xt_enc on fcheck's waves, which prints only those that differ; with PORT=fx, XtFxChip.cpp under xt_fx; with PORT=fxc, XtFxControl.cpp under xt_fx control, [cases] its events; PORT=fxv, XtFxValueText.cpp the same) with the first <old> replaced by
# <new> (exact text), builds xt_cosm (xt_analog, xt_tone, xt_fw, xt_enc) from it in $WORK/mut/<name>, runs it on
# [cases] (100) and prints the lines matching [pattern] (an extended regex;
# default every result line).  A check that
# still says 0 different missed the error: either the check is too weak, or
# the change is exact (e.g. a product of powers of two regrouped).
# PORT=wavefile: XtWaveFile.cpp (a wave's chunks) under xt_files, as PORT=files; PORT=tonewave: XtWaveFile.cpp under xt_tone.
# PORT=diskimage: XtDiskImage.cpp (the firmware's disk image, v177) under xt_files, as PORT=files.
# PORT=patchstore: XtPatchStore.cpp (the patch store's layout and the descriptors' reads, v187) under xt_files, as
# PORT=files; PORT=tonestore: XtPatchStore.cpp under xt_tone; PORT=tonearp: XtSynthArp.cpp under xt_tone (v189).
# PORT=arp: XtArpeggio.cpp (the arpeggiator's keys and settings, v188), PORT=arpstep: XtArpeggioStep.cpp (its timer and
# step), PORT=arpmotif: XtArpeggioMotifs.cpp (its motifs), PORT=ticks: XtTempoTicks.cpp (the tempo ticks it steps on,
# v189), under xt_arp; [cases] its random cases, after the presets.
# PORT=msm: XtMsm.cpp (the Multi Step Modulator, v192) under xt_msm, [cases] its random cases and SEED its seed;
# PORT=msmtrack: XtMsmTrack.cpp, PORT=msmpatch: XtMsmPatch.cpp, the same.
# PORT=vc2params: Vc2Params.cpp (rom.4's parameter tables, v199) under xt_vc2params, PORT=vc2in: Vc2Unvoiced.cpp (its
# unvoiced detector, v208) and PORT=vc2out: Vc2Consonants.cpp (its consonants, v209) under xt_vc2in, PORT=vc2legacy:
# Vc2LegacyAnalysis.cpp (LEGACY's analysis, v210), PORT=vc2tables: Vc2LegacyTables.cpp and PORT=vc2object:
# Vc2Legacy.cpp (its tables and object, v211) under xt_vc2legacy, on rom.4 ($ROM4).
# PORT=toneknobs, toneenvknobs, toneupdate, tonezones, tonenotes: XtSynthKnobs.cpp, XtSynthEnvKnobs.cpp, XtSynthUpdate.cpp,
# XtSynthZones.cpp, XtSynthNotes.cpp under xt_tone (v193: XtSynth's parts by job); tonefwcpp, tonefwh, tonecases,
# tonecarry: xt_tone's own XtToneFirmware.cpp, XtToneFirmware.h, XtToneCases.cpp, XtToneCarry.cpp; tonepatchids its
# XtTonePatchIds.cpp (v195).  PORT=wire…: under
# xt_tone's MSM mode (`xt_tone rom msm [cases] [seed]`), [cases] its cases: wiremsm XtSynthMsm.cpp; wireknobs,
# wireenvknobs, wireupdate, wirezones, wirenotes, wirearp the files of the tone ports of those names; wirecheck
# XtToneMsm.cpp, wirefwmsm XtToneFwMsm.h.  A header's plant builds every part again, from a copy of the sources.
source "$(dirname "$0")/env.sh"
[ $# -ge 3 ] || { echo "usage: mutate.sh name old new [cases] [pattern]"; exit 2; }
[ -f "$ROM" ] || { echo "mutate: no ROM at $ROM"; exit 2; }   # a run that can't start must never read as a result; no "== name" follows
N=$1; O=$2; W=$3; C=${4:-100}; P=${5:-.}
IMAGE="$ROM"   # what the planted binary runs on: rom.2, but rom.4 for PORT=vc2params (v199) and vc2in (v208)
S="$REPO/plugin/Source"; M="$WORK/mut"; mkdir -p "$M"
source "$(dirname "$0")/objects.sh"   # keptObject: the fx ports' objects, by their source and every header
# A tone port's file, planted under xt_tone; nothing for any other port
tonefile() {
    case "$1" in
        tone) echo XtSynth.cpp ;;
        tonecosm) echo XtCosm.cpp ;;
        toneanalog) echo XtAnalog.cpp ;;
        toneband) echo XtMultiBandPlayer.cpp ;;
        tonecheck) echo XtToneCheck.cpp ;;
        tonefw) echo XtFirmware.cpp ;;
        toneosc) echo XtOsc.cpp ;;
        tonelfo) echo XtLfo.cpp ;;
        toneenv) echo XtEnvelope.cpp ;;
        tonesolo) echo XtSoloVoice.cpp ;;
        tonelin) echo XtLinearPlayer.cpp ;;
        tonelite) echo XtLitePlayer.cpp ;;
        tonepatch) echo XtPatch.cpp ;;
        tonefiles) echo XtFiles.cpp ;;
        tonewave) echo XtWaveFile.cpp ;;
        tonestore) echo XtPatchStore.cpp ;;
        tonearp) echo XtSynthArp.cpp ;;
        toneknobs | wireknobs) echo XtSynthKnobs.cpp ;;
        toneenvknobs | wireenvknobs) echo XtSynthEnvKnobs.cpp ;;
        toneupdate | wireupdate) echo XtSynthUpdate.cpp ;;
        tonezones | wirezones) echo XtSynthZones.cpp ;;
        tonenotes | wirenotes) echo XtSynthNotes.cpp ;;
        wiremsm) echo XtSynthMsm.cpp ;;
        wirearp) echo XtSynthArp.cpp ;;
        wirecheck) echo XtToneMsm.cpp ;;
        wirefwmsm) echo XtToneFwMsm.h ;;
        tonefwcpp) echo XtToneFirmware.cpp ;;
        tonefwh) echo XtToneFirmware.h ;;
        tonecases) echo XtToneCases.cpp ;;
        tonepatchids) echo XtTonePatchIds.cpp ;;
        tonecarry) echo XtToneCarry.cpp ;;
        tonestart) echo XtSynthStart.cpp ;;
        toneoscstart) echo XtSynthOscStart.cpp ;;
        tonetrip) echo XtSynthTrip.cpp ;;
        toneblock) echo XtSynthBlock.cpp ;;
        tonetva) echo XtSynthTva.cpp ;;
        toneclock) echo XtSynthClock.cpp ;;
        toneconsts) echo XtSynthConstants.h ;;
        toneextin) echo XtExtIn.cpp ;;
        toneout) echo XtSynthOut.cpp ;;   # its output, busOut and render (v204)
    esac
}
source "$(dirname "$0")/mutate_parts.sh"   # harnessParts, fxParts: a harness's kept objects (moved as they stood, v208)
TF=$(tonefile "${PORT:-cosm}")
if [ -n "$TF" ]; then   # XtSynth's wiring, against xt_tone (the rest built once)
    F=$TF
    D=XT_TONE_MAIN; ARGS=("${SEED:-2024}" "$WORK/inputs")
    OBJ="$M/tone_sw_obj"   # with the research switches, as build.sh's xt_tone (v193)
    mkdir -p "$OBJ"
    L=()
    H=$(ls -t "$S"/*.h | head -1)   # a changed header rebuilds them all
    PARTS=(XtToneCheck XtSynth XtSoloEncoder XtSoloVoice XtLinearPlayer XtLitePlayer XtMultiBandPlayer XtEnvelope XtLfo
           XtOsc XtCosm XtAnalog XtFirmware XtFwMachine Sh4Interp Sh4Setup XtFiles XtWaveFile XtDiskImage XtPatchStore
           XtPatch XtSynthRom XtSynthArp XtArpeggio XtArpeggioStep XtArpeggioMotifs XtTempoTicks XtArpeggioPatch
           XtSynthZones XtSynthNotes XtSynthUpdate XtSynthKnobs XtSynthEnvKnobs XtSynthMsm XtMsm XtMsmTrack XtMsmPatch
           XtToneFirmware XtToneCases XtToneCarry XtToneMsm XtTonePatchIds Vc2Params XtFxValueText Vc2Unvoiced
           XtSynthStart XtSynthOscStart XtSynthTrip XtSynthBlock XtSynthTva XtSynthClock XtExtIn XtSynthOut XtSinc
           Vc2LegacyAnalysis Vc2LegacyTables Vc2HiresoTables)
    TONE=(-DXT_TONE_MAIN -DXT_TONE_CHECK -DXT_HARNESS_SWITCHES)
    for f in "${PARTS[@]}"; do
        [ "$f.cpp" = "$F" ] && continue
        L+=("$OBJ/$f.o")
        [ "$OBJ/$f.o" -nt "$S/$f.cpp" ] && [ "$OBJ/$f.o" -nt "$H" ] && continue
        "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 "${TONE[@]}" -c "$S/$f.cpp" -o "$OBJ/$f.o" 2>&1 | grep -E " error"
    done
    EXTRA=(-DXT_TONE_CHECK "${HARNESS_SWITCHES[@]}")
    # PORT=wire…: the Multi Step Modulator's wiring, against xt_tone's MSM mode, [cases] its cases (v193); TONE_MODE=extin:
    # any tone port's plant against xt_tone's EXT IN mode (`xt_tone rom extin [cases] [seed] [inputs]`, v203)
    [[ "$PORT" == wire* ]] && ARGS=("$C" "${SEED:-2024}") && C=msm
    [ "${TONE_MODE:-}" = extin ] && ARGS=("$C" "${SEED:-2024}" "$WORK/inputs") && C=extin
elif [ "$PORT" = solo ]; then      # XtSoloVoice, on the solocmp cases of fcheck's baseline; [cases] is not used
    F=XtSoloVoice.cpp; D=XT_FW_MAIN; L=("$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/Sh4Interp.cpp"); EXTRA=(-ffp-contract=off)
    L+=("$S/Sh4Setup.cpp")   # the interpreter's setup, its own file from v197
elif [ "$PORT" = fx ]; then        # XtFxChip against the reference; [cases] is its samples per type, and XT_FX_STOP stops at the first difference
    F=XtFxChip.cpp
    fxParts XtFxControl XtFxValueText XtFxInput XtExtIn
elif [[ "$PORT" =~ ^(fxc|fxv|fxi|fxe)$ ]]; then   # XtFxControl (fxv: its values' text, XtFxValueText.cpp, v199; fxi:
    # the input's way in, XtFxInput.cpp, v204; fxe: the input's ring and its copy without the chip, XtExtIn.cpp, v205)
    # against its checks in xt_fx; [cases] is the events of "xt_fx control"; the others of the four are kept objects
    case "$PORT" in
        fxv) F=XtFxValueText.cpp; KEEP=(XtFxControl XtFxInput XtExtIn) ;;
        fxi) F=XtFxInput.cpp; KEEP=(XtFxControl XtFxValueText XtExtIn) ;;
        fxe) F=XtExtIn.cpp; KEEP=(XtFxControl XtFxValueText XtFxInput) ;;
        *) F=XtFxControl.cpp; KEEP=(XtFxValueText XtFxInput XtExtIn) ;;
    esac
    fxParts XtFxChip "${KEEP[@]}"
elif [[ "$PORT" =~ ^(arp|arpstep|arpmotif|ticks)$ ]]; then   # XtArpeggio against the firmware's object; XT_ARP_STOP stops it
    case "$PORT" in
        ticks) F=XtTempoTicks.cpp ;;
        arpstep) F=XtArpeggioStep.cpp ;;
        arpmotif) F=XtArpeggioMotifs.cpp ;;
        *) F=XtArpeggio.cpp ;;
    esac
    harnessParts XT_ARP_MAIN arp_obj XtArpeggio XtArpeggioStep XtArpeggioMotifs XtArpeggioCheck XtArpeggioCases \
                 XtTempoTicks XtTempoTicksCheck XtArpeggioPatch
elif [[ "$PORT" =~ ^msm(track|patch)?$ ]]; then
    # The Multi Step Modulator against xt_msm: XtMsm.cpp, with PORT=msmtrack XtMsmTrack.cpp, with PORT=msmpatch
    # XtMsmPatch.cpp; [cases] its random cases, SEED its seed (default 2024), XT_MSM_STOP its first case that differs
    case "$PORT" in
        msmtrack) F=XtMsmTrack.cpp ;;
        msmpatch) F=XtMsmPatch.cpp ;;
        *) F=XtMsm.cpp ;;
    esac
    ARGS=("${SEED:-2024}")
    harnessParts XT_MSM_MAIN msm_obj XtMsm XtMsmTrack XtMsmPatch XtMsmCheck XtMsmCases
elif [[ "$PORT" =~ ^(vc2params|vc2in|vc2out|vc2legacy|vc2tables|vc2object|vc2warp|vc2hitables)$ ]]; then
    # rom.4's ports, on rom.4: its parameter tables, Vc2Params.cpp, against xt_vc2params (v199), [cases] its values an ID;
    # the unvoiced detector, Vc2Unvoiced.cpp (v208), and its consonants, Vc2Consonants.cpp (vc2out, v209), against
    # xt_vc2in; LEGACY's analysis, Vc2LegacyAnalysis.cpp (v210), its tables, Vc2LegacyTables.cpp (vc2tables), and its
    # object, Vc2Legacy.cpp (vc2object, v211), against xt_vc2legacy; HIRESO's warp, Vc2HiresoWarp.cpp (vc2warp, v213),
    # and its tables, Vc2HiresoTables.cpp (vc2hitables), against xt_vc2hireso; [cases] their cases; SEED any's
    IMAGE="$ROM4"
    ARGS=("${SEED:-2024}")
    VC2IN=(Vc2Unvoiced Vc2Consonants Vc2UnvoicedCheck)
    VC2LEG=(Vc2LegacyAnalysis Vc2LegacyTables Vc2Legacy Vc2LegacyCheck Vc2LegacySynthCheck)
    VC2HI=(Vc2HiresoWarp Vc2HiresoTables Vc2HiresoCheck)
    case "$PORT" in
        vc2warp) F=Vc2HiresoWarp.cpp; harnessParts XT_VC2HIRESO_MAIN vc2hireso_obj "${VC2HI[@]}" ;;
        vc2hitables) F=Vc2HiresoTables.cpp; harnessParts XT_VC2HIRESO_MAIN vc2hireso_obj "${VC2HI[@]}" ;;
        vc2in) F=Vc2Unvoiced.cpp; harnessParts XT_VC2IN_MAIN vc2in_obj "${VC2IN[@]}" ;;
        vc2out) F=Vc2Consonants.cpp; harnessParts XT_VC2IN_MAIN vc2in_obj "${VC2IN[@]}" ;;
        vc2legacy) F=Vc2LegacyAnalysis.cpp; harnessParts XT_VC2LEGACY_MAIN vc2legacy_obj "${VC2LEG[@]}" ;;
        vc2tables) F=Vc2LegacyTables.cpp; harnessParts XT_VC2LEGACY_MAIN vc2legacy_obj "${VC2LEG[@]}" ;;
        vc2object) F=Vc2Legacy.cpp; harnessParts XT_VC2LEGACY_MAIN vc2legacy_obj "${VC2LEG[@]}" ;;
        *) F=Vc2Params.cpp; harnessParts XT_VC2PARAMS_MAIN vc2_obj Vc2Params Vc2ParamsCheck XtFxValueText ;;
    esac
elif [ "$PORT" = enc ]; then       # XtSoloEncoder, on the xt_enc waves of fcheck's baseline; [cases] is not used
    F=XtSoloEncoder.cpp; D=XT_ENCODER_MAIN; L=("$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp"); EXTRA=(-ffp-contract=off)
    L+=("$S/Sh4Setup.cpp")   # the interpreter's setup, its own file from v197
elif [[ "$PORT" =~ ^(files|wavefile|diskimage|patchstore)$ ]]; then
    # XtFiles, or with PORT=wavefile XtWaveFile (a wave's chunks), with PORT=diskimage XtDiskImage (the firmware's disk
    # image), with PORT=patchstore XtPatchStore (the store's layout), against fcheck's baseline of xt_files; [cases] is
    # not used.  The others, xt_files' checks among them, are built as they are.
    case "$PORT" in
        wavefile) F=XtWaveFile.cpp ;;
        diskimage) F=XtDiskImage.cpp ;;
        patchstore) F=XtPatchStore.cpp ;;
        *) F=XtFiles.cpp ;;
    esac
    D=XT_FILES_MAIN
    L=()
    for f in XtFiles.cpp XtWaveFile.cpp XtDiskImage.cpp XtPatchStore.cpp XtPatchStoreCheck.cpp XtWaveFileCheck.cpp; do
        [ "$f" = "$F" ] || L+=("$S/$f")
    done
    L+=("$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/XtSoloEncoder.cpp" "$S/Sh4Interp.cpp")
    L+=("$S/Sh4Setup.cpp")   # the interpreter's setup, its own file from v197
elif [ "$PORT" = filesfw ]; then   # the same, with the error in XtFirmware's routines for them
    F=XtFirmware.cpp; D=XT_FILES_MAIN
    OBJ="$M/files_obj"; mkdir -p "$OBJ"
    [ "$OBJ/XtFiles_main.o" -nt "$S/XtFiles.cpp" ] && [ "$OBJ/XtFiles_main.o" -nt "$S/XtFiles.h" ] && [ "$OBJ/XtFiles_main.o" -nt "$S/XtFirmware.h" ] || { "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -DXT_FILES_MAIN -I"$S" -c "$S/XtFiles.cpp" -o "$OBJ/XtFiles_main.$$.o" && mv "$OBJ/XtFiles_main.$$.o" "$OBJ/XtFiles_main.o"; }
    L=("$OBJ/XtFiles_main.o" "$S/XtWaveFile.cpp" "$S/XtDiskImage.cpp" "$S/XtPatchStore.cpp" "$S/XtPatchStoreCheck.cpp"
       "$S/XtWaveFileCheck.cpp" "$S/XtMultiBandPlayer.cpp"
       "$S/XtSoloVoice.cpp" "$S/XtSoloEncoder.cpp" "$S/Sh4Interp.cpp")
elif [ "${PORT:-cosm}" = analog ]; then
    F=XtAnalog.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogcosm ]; then
    F=XtCosm.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogband ]; then
    F=XtMultiBandPlayer.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtFirmware.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
elif [ "$PORT" = analogfw ]; then
    F=XtFirmware.cpp; D=XT_ANALOG_MAIN
    L=("$S/XtOsc.cpp" "$S/XtCosm.cpp" "$S/XtAnalog.cpp" "$S/XtEnvelope.cpp" "$S/XtMultiBandPlayer.cpp" "$S/XtSoloVoice.cpp" "$S/Sh4Interp.cpp")
else
    F=XtCosm.cpp; D=XT_COSM_MAIN; L=("$S/XtOsc.cpp" "$S/XtEnvelope.cpp" "$S/Sh4Interp.cpp")
fi
# The machine XtFirmware's routines run on (v175) is built wherever they are; the tone port has it among its PARTS
if [ -z "$TF" ] && { [ "$F" = XtFirmware.cpp ] || [[ " ${L[*]} " == *"/XtFirmware.cpp "* ]]; }; then
    L+=("$S/XtFwMachine.cpp")
fi
# A header's plant (PORT=tonefwh, wirefwmsm; v193) goes into a copy of the sources
OUT="$M/$N.cpp"
if [[ "$F" == *.h ]]; then
    rm -rf "${M:?}/${N:?}.src"
    cp -R "$S" "$M/$N.src"
    OUT="$M/$N.src/$F"
fi
"$PY" - "$S/$F" "$OUT" "$O" "$W" <<'PY' || exit 1
import sys
s = open(sys.argv[1]).read()
if s.count(sys.argv[3]) == 0: sys.exit('mutate: the old text is not in ' + sys.argv[1])
open(sys.argv[2], 'w').write(s.replace(sys.argv[3], sys.argv[4], 1))
PY
# and every part is built from the copy, which the line below links, its own file left empty
if [[ "$F" == *.h ]]; then
    : > "$M/$N.cpp"
    mkdir -p "$M/$N.obj"
    L=()
    for f in "${PARTS[@]}"; do L+=("$M/$N.obj/$f.o"); done
    printf '%s\n' "${PARTS[@]}" | xargs -P "${JOBS:-8}" -I{} "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 \
        "${TONE[@]}" -c "$M/$N.src/{}.cpp" -o "$M/$N.obj/{}.o" 2>&1 | grep -E " error" && exit 1
fi
"$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -D$D "${EXTRA[@]}" -I"$S" "$M/$N.cpp" "${L[@]}" -o "$M/$N" 2>&1 | grep -E " error" && exit 1
[ -f "$IMAGE" ] || { echo "mutate: no ROM at $IMAGE"; exit 2; }   # again: a build takes long enough for it to go
echo "== $N"
if [ "$PORT" = solo ]; then
    B="$REPO/research/tools/fcheck_baseline.txt"; T="$M/$N.out"; rm -rf "${T:?}"; mkdir -p "$T"; i=0
    cd "$WORK/inputs"
    while IFS= read -r spec; do
        i=$((i+1))
        ( l="solo $spec: $("$M/$N" "$ROM" solocmp $spec 2>/dev/null | tail -1)"
          if grep -qxF -- "$l" "$B"; then echo "same: $spec"; else echo "DIFFERENT: $l"; fi ) > "$T/$i" &
    done < <(sed -n 's/^solo \(.*\): [0-9]* records.*/\1/p' "$B")
    wait
    cat "$T"/* | grep -E "$P"
    exit
fi
if [[ "$PORT" =~ ^(files|filesfw|wavefile|diskimage|patchstore)$ ]]; then
    # the baseline's whole xt_files section, as xt_files gains lines (v148, v155): a line the run prints that isn't in it differs, and one of it the run doesn't print
    B="$REPO/research/tools/fcheck_baseline.txt"; T="$M/$N.base"; awk '/^== /{s = ($0 == "== xt_files"); next} s' "$B" > "$T"
    "$M/$N" "$ROM" 2>/dev/null > "$M/$N.all"; st=$?
    grep -vxF -f "$T" "$M/$N.all" > "$M/$N.out"
    grep -vxF -f "$M/$N.all" "$T" | sed 's/^/missing: /' >> "$M/$N.out"
    [ "$st" -ge 128 ] && echo "x: the run died, signal $((st - 128))" >> "$M/$N.out"
    grep -E "$P" "$M/$N.out"
    echo "files: $(grep -c . "$M/$N.out") lines differ from the baseline"
    exit
fi
if [ "$PORT" = enc ]; then
    B="$REPO/research/tools/fcheck_baseline.txt"; T="$M/$N.base"; grep '^enc ' "$B" > "$T"
    cd "$WORK/inputs"
    sed -n 's/^enc \([^:]*\(:2\)\{0,1\}\): .*/\1/p' "$T" | xargs -n 1 -P "${JOBS:-8}" "$M/$N" "$ROM" cmp 2>/dev/null > "$M/$N.all"
    grep -vxF -f "$T" "$M/$N.all" > "$M/$N.out"
    while IFS= read -r w; do grep -qF "enc $w: " "$M/$N.all" || echo "enc $w: no result" >> "$M/$N.out"; done < <(sed -n 's/^enc \([^:]*\(:2\)\{0,1\}\): .*/\1/p' "$T")   # a wave the run died on differs too
    grep -E "$P" "$M/$N.out"
    echo "enc: $(grep -c . "$M/$N.out") waves differ from the baseline"
    exit
fi
[[ "$PORT" =~ ^(fxc|fxv|fxi|fxe)$ ]] && ARGS=("$C") && C=control
# a run the error hangs is stopped as solocmp's alarm stops one (MUT_TIMEOUT seconds, default 1500): it counts as one that died
timeout "${MUT_TIMEOUT:-1500}" "$M/$N" "$IMAGE" "$C" "${ARGS[@]}" | grep -v "^  " | grep -E "$P"
st=${PIPESTATUS[0]}; [ "$st" = 124 ] && st=$((128 + 14)); [ "$st" -ge 128 ] && echo "$N: the run died, signal $((st - 128))"
