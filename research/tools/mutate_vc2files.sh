# mutate_vc2files.sh: each rom.4 check's files, without .cpp, for mutate_vc2ports.sh's rows, which source it (moved out
# of it as they stood, v391, when it reached the kit's 300 lines). A check's new file goes in its list here.
# Each check's files, without .cpp
VC2PARAMS=(Vc2Params Vc2ParamsCheck XtFxValueText)
VC2IN=(Vc2Unvoiced Vc2Consonants Vc2UnvoicedCheck Vc2CheckWords)
VC2LEG=(Vc2LegacyAnalysis Vc2LegacyTables Vc2Legacy Vc2LegacyCheck Vc2LegacySynthCheck Vc2CheckWords)
VC2HIPORTS=(Vc2HiresoWarp Vc2HiresoTables Vc2HiresoRate Vc2HiresoAnalysis Vc2Hireso Vc2HiresoSynth)
VC2HI=("${VC2HIPORTS[@]}" Vc2Unvoiced Vc2HiresoCheck Vc2HiresoRateCheck Vc2HiresoAnalysisCheck Vc2HiresoObjectCheck
       Vc2CheckWords)
VC2VOC=(Vc2Vocoder Vc2VocoderTables Vc2Unvoiced Vc2Consonants Vc2LegacyAnalysis Vc2Legacy Vc2LegacyTables
        "${VC2HIPORTS[@]}" Vc2VocoderCheck Vc2CheckWords)
VC2CAR=(Vc2Carrier Vc2CarrierTables Vc2CarrierCheck Vc2VocoderOsc Vc2VocoderOscCheck Vc2Math Vc2OscConvert
        Vc2OscConvertTables Vc2OscConvertCheck Vc2OscAnalogConvert Vc2OscConvertDraws)
VC2TRACK=(Vc2Tracker Vc2TrackerTables Vc2TrackerAnalysis Vc2TrackerEstimate Vc2TrackerCheck Vc2TrackerSignal)
VC2K6=(Vc2Kind6 Vc2Kind6Core Vc2Kind6Tables Vc2Math Vc2Kind6Check Vc2TrackerSignal)
VC2AN=(Vc2AutoNote Vc2AutoNoteTables Vc2AutoNoteCheck Vc2Tracker Vc2TrackerTables Vc2TrackerAnalysis Vc2TrackerEstimate
       Vc2TrackerSignal)
VC2MIX=(Vc2PartMixer Vc2MixerBlock Vc2PartMixerTables Vc2Math Vc2PartMixerCheck Vc2PartMixerFakes Vc2MixerTasksCheck)
VC2TONE=(Vc2Tone Vc2ToneNoteOn Vc2ToneCheck Vc2ToneSides Vc2ToneFakes Vc2TonePortParts Vc2ToneEvents Vc2ToneModulators
         Vc2ToneEnvelope Vc2ToneLfo Vc2ToneModCheck XtEnvelope XtLfo Vc2ToneMix Vc2ToneMixRender Vc2ToneMixTables
         Vc2ToneMixCheck Vc2FreeLfos Vc2FreeLfoCheck Vc2ToneLists Vc2ToneMethods Vc2ToneMethodCheck Vc2ToneHost
         Vc2Tva Vc2TvaTables Vc2TvaDraws Vc2ToneTvaCheck Vc2CosmObject Vc2CosmPools Vc2CosmNoteOn Vc2CosmAdapters
         Vc2CosmAdapterWords Vc2CosmAdapterBlocks Vc2CosmTbAdapter Vc2CosmBufferedAdapters Vc2CosmDsp Vc2CosmGrowl
         Vc2CosmTables Vc2Math XtCosm Vc2CosmNoteOnDraws Vc2ToneCosmCheck Vc2CosmConvert XtOsc Vc2ToneHostOsc
         Vc2ToneOscCheck Vc2OscVoice Vc2OscVoiceGet Vc2OscVoiceKinds Vc2OscTables Vc2OscNoteOn Vc2OscTargets
         Vc2PitchRamp Vc2PitchRampTick Vc2RampTables Vc2OscPlayers Vc2OscLite Vc2OscAnalog Vc2OscAnalogLa
         Vc2OscAnalogSub Vc2OscAnalogShape Vc2OscAnalogUpdate Vc2OscVibrato Vc2Carrier Vc2CarrierTables Vc2Kind6
         Vc2Kind6Core Vc2Kind6Tables Vc2OscDraws Vc2OscPartDraws Vc2OscRecords Vc2OscFakes Vc2OscPortParts
         Vc2VocoderOsc XtExtIn Vc2OscVariOffRender Vc2OscBeat Vc2OscHeads XtLitePlayer
         Vc2OscModeOne Vc2OscWalk Vc2OscWalkEvents Vc2OscWalkDraws Vc2OscWalkStart Vc2OscModeOneStart Vc2OscVari
         Vc2OscVariDraws Vc2OscBands Vc2OscBandDraws Vc2OscBandVpdt Vc2OscBandSetUp Vc2OscSolo Vc2OscSoloFrame
         Vc2OscSoloRecords Vc2OscSoloSetUp Vc2OscPlayerSteers Vc2OscRecordBlock Vc2OscFrameSteps Vc2OscJump
         Vc2OscJumpRecords Vc2OscAnalogRender XtAnalog Vc2OscAnalogRenderDraws)
VC2OSC=(Vc2OscVoice Vc2OscVoiceGet Vc2OscVoiceKinds Vc2OscTables Vc2OscNoteOn Vc2OscTargets Vc2PitchRamp
        Vc2PitchRampTick Vc2RampTables Vc2OscPlayers Vc2OscCheck Vc2OscEvents Vc2OscGetters Vc2OscFakes
        Vc2OscPortParts Vc2RampCheck Vc2OscRecords Vc2OscLite Vc2OscAnalog Vc2OscAnalogLa Vc2OscAnalogSub
        Vc2OscAnalogShape Vc2OscAnalogDraws Vc2OscAnalogUpdate Vc2OscVariOffRender Vc2VariOffCheck Vc2OscKindsCheck
        Vc2Carrier Vc2CarrierTables Vc2Kind6 Vc2Kind6Core Vc2Kind6Tables Vc2Math Vc2OscVibrato Vc2OscVibratoCheck
        Vc2OscDraws Vc2OscPartDraws Vc2VocoderOsc XtExtIn Vc2OscBeat Vc2OscHeads XtLitePlayer
        Vc2OscModeOne Vc2OscWalk Vc2OscWalkEvents Vc2OscWalkDraws Vc2OscModeOneCheck Vc2OscWalkStart
        Vc2OscModeOneStart Vc2OscVari Vc2OscVariDraws Vc2OscBands Vc2OscBandDraws Vc2OscBandVpdt Vc2OscBandsCheck
        Vc2OscBandSetUp Vc2OscSolo Vc2OscSoloFrame Vc2OscSoloRecords Vc2OscSoloSetUp Vc2OscPlayerSteers
        Vc2OscRecordBlock Vc2OscFrameSteps Vc2OscJump Vc2OscJumpRecords Vc2OscAnalogRender XtAnalog XtCosm XtOsc
        XtEnvelope Vc2OscAnalogRenderDraws)
VC2TVA=(Vc2Tva Vc2TvaTables Vc2TvaCheck Vc2TvaEvents Vc2TvaFakes Vc2TvaDraws)
VC2COSM=(Vc2CosmGrowl Vc2CosmTables Vc2Math Vc2CosmCheck Vc2CosmConvert Vc2CosmTvf Vc2Matrix Vc2MatrixTables
         Vc2CosmFrameCheck Vc2CosmConverters Vc2CosmFrameDraws Vc2CosmFilters Vc2CosmDynamics Vc2CosmDrive
         Vc2CosmBalanced Vc2CosmSideBand Vc2MatrixCheck Vc2MatrixSources Vc2MatrixSetup Vc2MatrixSetupCheck
         Vc2MatrixSetupDraws Vc2CosmObject Vc2CosmObjectCheck Vc2CosmPools Vc2CosmNoteOn Vc2CosmNoteOnCheck
         Vc2CosmNoteOnDraws Vc2CosmAdapters Vc2CosmAdapterWords Vc2CosmAdapterBlocks Vc2CosmTbAdapter
         Vc2CosmBufferedAdapters Vc2CosmDsp Vc2CosmDspCheck Vc2CosmDspKinds Vc2CosmDspSlots XtCosm XtOsc XtEnvelope)
VC2LIBM=(Vc2LibMath Vc2LibMathTables Vc2LibMathCheck Vc2ToneMath Vc2ToneMathCheck XtOsc XtEnvelope)
VC2VOICES=(Vc2PartVoices Vc2PartVoicesTables Vc2PartVoicesCheck Vc2PartVoicesEvents Vc2PartVoicesFakes
           Vc2Assigner Vc2AssignCheck Vc2AssignEvents Vc2AssignRelease Vc2AssignNotes Vc2AssignReleaseCheck
           Vc2AssignReleaseEvents Vc2Keys Vc2KeysTables Vc2VoiceClass Vc2VoiceQueue Vc2VoiceClassCheck
           Vc2VoiceClassEvents Vc2VoiceQueueCheck Vc2AssignListenersCheck Vc2UpdateTask Vc2UpdateTaskCheck)
VC2NOTES=(Vc2Notes Vc2NotesCheck Vc2NotesEvents Vc2NotesFakes)
VC2KEYS=(Vc2Keys Vc2KeysTables Vc2KeysCheck Vc2KeysEvents Vc2PartNodes Vc2PartNodesCheck)
VC2MIDI=(Vc2OsMidi Vc2OsMidiHandler Vc2OsMidiTables Vc2KeysTables Vc2OsMidiCheck Vc2OsMidiEvents)
VC2KERNEL=(Vc2Kernel Vc2KernelRig Vc2KernelWords Vc2KernelCheck)   # v353: rom.1's event services; v355 its words
# xt_vc2synth's (v314): every VC-2 port the plugin's build lists but the .p02 reader (v450) and rom.4's own load of the
# waves and its files (v503), which runs at the start, not in the part (from v345 rom.4's parameter tables and the patch
# store, which the part reads); then the rom.2 ports they lean on, the shared compares and the check
NOT_SYN=" Vc2PatchFile Vc2BootFiles Vc2WaveBoot Vc2WaveBootKernel Vc2WavesInUse "
VC2SYNTH=()
while read -r f; do
    [[ "$NOT_SYN" == *" $f "* ]] || VC2SYNTH+=("$f")
done < <(sed -n 's|^ *Source/\(Vc2[A-Za-z0-9]*\)\.cpp$|\1|p' "$REPO/plugin/Vc2Sources.cmake")
VC2SYNTH+=(Vc2SynthCheck Vc2SynthReadsCheck Vc2SynthLoadCheck Vc2SynthNotesCheck Vc2SynthTasksCheck Vc2CheckWords
           XtLfo XtEnvelope XtCosm XtOsc XtExtIn XtLitePlayer XtAnalog XtFxValueText XtTempoTicks
           Vc2SynthWholeCheck Vc2SynthWholeEventsCheck Vc2KernelRig Vc2KernelWords Vc2MachineRig)   # v345: harnessParts adds the store's,
           # firmware's; v351 the tasks' steps' file; v354 the tempo ticks; v355 the whole part on rom.1's kernel;
           # v360 the whole part's events' file
VC2SETS=(Vc2Sets Vc2NoteMatrix Vc2MatrixSetup Vc2MatrixTables Vc2Matrix Vc2MatrixSources Vc2SetsCheck Vc2SetsDraws
         Vc2SetsEvents Vc2SetCommon Vc2SetCommonCheck Vc2SetEnvelope Vc2SetEnvelopeTables Vc2SetEnvelopeCheck
         Vc2LibMath Vc2LibMathTables Vc2SetChildHead Vc2SetLfo Vc2SetLfoCheck Vc2SetMix Vc2SetMixCheck
         Vc2SetTva Vc2SetTvaCheck Vc2SetCosmInput Vc2SetCosmInputCheck Vc2CosmConvert Vc2CosmConverters Vc2CosmGrowl
         Vc2CosmTvf Vc2CosmFilters Vc2CosmDynamics Vc2CosmDrive Vc2CosmBalanced Vc2CosmSideBand Vc2CosmTables Vc2Math
         Vc2CosmFrameDraws Vc2SetOscBlock Vc2SetOscKinds Vc2SetOscBlockCheck Vc2OscConvert Vc2OscAnalogConvert
         Vc2OscConvertTables Vc2VocoderOsc Vc2Carrier Vc2CarrierTables Vc2OscExtinConvert Vc2SetOscBlockDraws
         Vc2OscPcmConvert Vc2OscPcmLite Vc2OscPcmVari Vc2WaveStore Vc2SetOscStoreDraws Vc2PartSettingsWords
         Vc2WaveSets Vc2SetMemoryReads)
VC2SETTINGS=(Vc2PartSettings Vc2PartSettingsCheck Vc2SetChildHead Vc2PartSettingsTables Vc2PartSettingsWords
    Vc2PartSettingsOsc Vc2PartSettingsOscCheck Vc2PartSettingsOscBlock Vc2PartSettingsVocoder
    Vc2PartSettingsVocoderCheck Vc2LibMath Vc2LibMathTables Vc2PartSettingsModes Vc2PartSettingsModesCheck
    Vc2PartSettingsDispatch Vc2PartSettingsDispatchCheck Vc2MatrixSetup Vc2MatrixTables Vc2Matrix Vc2MatrixSources
    Vc2PartSettingsCategory Vc2PartSettingsCategoryCheck Vc2StaticMatrix Vc2StaticMatrixCheck
    Vc2PartSettingsSystem Vc2PartSettingsSystemCheck Vc2PartSettingsMessages Vc2PartSettingsMessagesCheck
    Vc2PartNodes Vc2Keys Vc2KeysTables Vc2PartSettingsReceive Vc2PartSettingsReceiveCheck
    Vc2PartSettingsRoutingCheck Vc2PartReloads Vc2PartReloadsCheck Vc2PartControllers Vc2PartControllersCheck
    Vc2PartNumbers Vc2PartNumbersCheck Vc2PartHandlersCheck Vc2PartStartUp Vc2PartStartUpCheck Vc2PartVoices
    Vc2PartVoicesTables Vc2TimeTrip Vc2TimeTripCheck Vc2PartChannel Vc2PartChannelCheck Vc2MidiDispatch
    Vc2MidiDispatchCheck Vc2NotesTask Vc2NotesTaskCheck)
VC2PATCH=(Vc2PatchStore Vc2PatchFile Vc2PatchStoreCheck Vc2PatchFileCheck Vc2Params XtFxValueText)
# xt_vc2waves' (v500): the wave loaders and the sets and store they fill; (v503) rom.4's own load and its files
VC2WAVES=(Vc2WavesCheck Vc2WaveIndexCheck Vc2WaveIndex Vc2WaveSets Vc2WaveStore Vc2PartSettingsWords Vc2WavePool
          Vc2WavePoolCheck Vc2WavePoolDraws Vc2WaveRecords Vc2WaveRecordsCheck Vc2WaveBoot Vc2WaveBootKernel
          Vc2BootFiles Vc2WaveBootCheck Vc2WaveLookupCheck)
