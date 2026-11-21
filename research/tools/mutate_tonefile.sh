# mutate_tonefile.sh: sourced by mutate.sh, which plants by it, and read by mutate_tone.py's --changed, which picks
# plants by it (2026-11-21, moved from mutate.sh as it stood).
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
