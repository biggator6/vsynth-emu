# build_midi.sh: sourced by build.sh (v462), with its cc, S, O, FW and SH4.  xt_midi: rom.2's edits of the patch from a
# control change (XtMidiPort) against the firmware's own port handler and OS part object, the edit object hooked.
rm -f "$O/xt_midi"
[ -f XtMidiEdits.cpp ] && cc -DXT_MIDI_MAIN XtMidiEditsCheck.cpp XtMidiEdits.cpp "${FW[@]}" XtMultiBandPlayer.cpp \
   XtSoloVoice.cpp "${SH4[@]}" -o "$O/xt_midi" &
