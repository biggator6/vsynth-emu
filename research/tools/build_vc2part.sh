# build_vc2part.sh, sourced by build.sh in plugin/Source with its cc, S, O, FW and SH4 (moved out of it as they stood,
# v353): the VC-2 part's checks of its voices, notes, keys and OS MIDI, the part whole, and rom.1's kernel's services.
# The part's voices and their lists (v272) against rom.4's own routines, the destroys, end test and cost hooked
[ -f Vc2PartVoicesCheck.cpp ] && cc -DXT_VC2VOICES_MAIN Vc2PartVoicesCheck.cpp Vc2PartVoicesEvents.cpp \
   Vc2PartVoicesFakes.cpp Vc2PartVoices.cpp Vc2PartVoicesTables.cpp \
   $( [ -f Vc2AssignCheck.cpp ] && echo Vc2AssignCheck.cpp Vc2AssignEvents.cpp Vc2Assigner.cpp ) \
   $( [ -f Vc2AssignReleaseCheck.cpp ] && echo Vc2AssignReleaseCheck.cpp Vc2AssignReleaseEvents.cpp \
      Vc2AssignRelease.cpp Vc2AssignNotes.cpp Vc2Keys.cpp Vc2KeysTables.cpp ) \
   $( [ -f Vc2VoiceClassCheck.cpp ] && echo Vc2VoiceClassCheck.cpp Vc2VoiceClassEvents.cpp Vc2VoiceClass.cpp \
      Vc2VoiceQueue.cpp Vc2VoiceQueueCheck.cpp ) \
   $( [ -f Vc2AssignListenersCheck.cpp ] && echo Vc2AssignListenersCheck.cpp ) \
   $( [ -f Vc2UpdateTask.cpp ] && echo Vc2UpdateTask.cpp Vc2UpdateTaskCheck.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2voices" &
# The part's notes W (v275) against rom.4's own routines, every tone method they reach hooked
[ -f Vc2NotesCheck.cpp ] && cc -DXT_VC2NOTES_MAIN Vc2NotesCheck.cpp Vc2NotesEvents.cpp Vc2NotesFakes.cpp \
   Vc2Notes.cpp "${SH4[@]}" -o "$O/xt_vc2notes" &
# The part's keys (v276) against rom.4's own routines, the OS clock hooked
[ -f Vc2KeysCheck.cpp ] && cc -DXT_VC2KEYS_MAIN Vc2KeysCheck.cpp Vc2KeysEvents.cpp Vc2Keys.cpp Vc2KeysTables.cpp \
   $( [ -f Vc2PartNodesCheck.cpp ] && echo Vc2PartNodes.cpp Vc2PartNodesCheck.cpp ) \
   "${SH4[@]}" -o "$O/xt_vc2keys" &
# The OS MIDI task's handler and the channel states (v313) against rom.4's own, the OS's listeners and routines hooked
[ -f Vc2OsMidiCheck.cpp ] && cc -DXT_VC2MIDI_MAIN Vc2OsMidiCheck.cpp Vc2OsMidiEvents.cpp Vc2OsMidi.cpp \
   Vc2OsMidiHandler.cpp Vc2OsMidiTables.cpp Vc2KeysTables.cpp "${SH4[@]}" -o "$O/xt_vc2midi" &
# rom.1's kernel's event services (v353) against rom.1's own, rom.1 beside rom.4, its switch and timers hooked
[ -f Vc2KernelCheck.cpp ] && cc -DXT_VC2KERNEL_MAIN Vc2KernelCheck.cpp Vc2KernelRig.cpp Vc2Kernel.cpp "${SH4[@]}" \
   -o "$O/xt_vc2kernel" &
# Vc2Synth's start (v314) against rom.4's own static initializers: every VC-2 port but the .p02 reader (v450) and rom.4's
# own load of the waves (v503); from v345 rom.4's parameter tables and the patch store, which the part reads, with theirs
NOT_SYN=" Vc2PatchFile.cpp Vc2BootFiles.cpp Vc2WaveBoot.cpp Vc2WaveBootKernel.cpp Vc2WavesInUse.cpp "
SYNX=(XtFxValueText.cpp XtPatchStore.cpp XtSoloVoice.cpp "${FW[@]}")
VSYN=()
while read -r f; do
  [[ "$NOT_SYN" == *" $f "* ]] || VSYN+=("$f")
done < <(sed -n 's|^ *Source/\(Vc2[A-Za-z0-9]*\.cpp\)$|\1|p' ../Vc2Sources.cmake)
[ -f Vc2SynthCheck.cpp ] && cc -DXT_VC2SYNTH_MAIN Vc2Synth*Check.cpp Vc2CheckWords.cpp "${VSYN[@]}" XtLfo.cpp \
   XtEnvelope.cpp XtCosm.cpp XtOsc.cpp XtExtIn.cpp $( [ -f Vc2OscBeat.cpp ] && echo XtMultiBandPlayer.cpp ) \
   $( [ -f Vc2OscHeads.cpp ] && echo XtLitePlayer.cpp ) $( [ -f Vc2OscAnalogRender.cpp ] && echo XtAnalog.cpp ) \
   "${SYNX[@]}" "${SH4[@]}" -o "$O/xt_vc2synth" &
