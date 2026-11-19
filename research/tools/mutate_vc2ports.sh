# mutate_vc2ports.sh: rom.4's ports for mutate.sh, which sources it (moved out of it as they stood, v220, as a table).
# Each port plants its file under its check, on rom.4, [cases] the check's cases and SEED any's: rom.4's parameter
# tables (vc2params, v199) under xt_vc2params, [cases] its values an ID; the unvoiced detector (vc2in, v208) and its
# consonants (vc2out, v209) under xt_vc2in; LEGACY's analysis (vc2legacy, v210), tables (vc2tables) and object
# (vc2object, v211) under xt_vc2legacy; HIRESO's warp (vc2warp, v213), tables (vc2hitables), rate filters (vc2rate,
# v214), analysis (vc2hiana, v215), object (vc2hiobj, v216) and synthesis (vc2hisyn, v217) under xt_vc2hireso; the
# vocoder object (vc2voc, v220) and its tables (vc2voctables) under xt_vc2vocoder; the VOCODER OSC's carrier (vc2car,
# v221), its tables (vc2cartables) and its converter's own part and kind 5's update (vc2vocosc, v222), and the OSC
# block's converter (vc2oscconv, v251) and its constants (vc2oscconvtables), under xt_vc2carrier; the pitch tracker's frame (vc2tracker, v223), its tables (vc2tracktables), its analysis's crossings,
# onset check and taking (vc2trackana, v224) and its estimator (vc2trackest) under xt_vc2tracker; voice kind 6 and the
# mic ring (vc2k6, v225), its core (vc2k6core), its tables (vc2k6tables) and the shared math (vc2k6math) under
# xt_vc2kind6, the shared math's sine and cosine (vc2math) under xt_vc2carrier; Auto Note and the pitch source (vc2an,
# v226) and its tables (vc2antables) under xt_vc2autonote; the part mixer's levels (vc2mix, v227), its block
# (vc2mixblock), its tables (vc2mixtables) and the shared clamp (vc2mixmath) under xt_vc2mixer; the tone object's frame
# and its pool (vc2tone, v228) and its note-on (vc2tonenoteon, v229) under xt_vc2tone; the OSC voice's frame (vc2osc,
# v230), its settings and getters (vc2oscget), its kinds' rows (vc2osckinds), its constants (vc2osctables), its note-on
# by kind (vc2oscnoteon, v232), its targets (vc2osctargets) and its pitch ramp (vc2ramp, v236), the ramp's tick
# (vc2ramptick) and constants (vc2ramptables), and its players' installs and VariPhrase's note-on (vc2oscplayers,
# v238), LITE's note-on (vc2osclite, v240), the analog's (vc2oscanalog, its LA way vc2oscanalogla, v242) and its sub
# oscillator's (vc2oscanalogsub, v244) under xt_vc2osc; the TVA (vc2tva, v233) and its
# constants (vc2tvatables) under xt_vc2tva; the Growl (vc2growl, v234), its constants (vc2cosmtables) and the shared
# sine and cosine it takes (vc2cosmmath) under xt_vc2cosm; the converters' frame (vc2cosmconvert, v235), the TVF's
# converter (vc2cosmtvf), the Matrix's readers (vc2matrix) and its limits (vc2matrixtables) under xt_vc2cosm too; the
# first batch of the other converters (v237): the Dual and TB Filters' (vc2cosmfilters), the Amp Simulator's,
# Compressor's and Limiter's (vc2cosmdynamics) and the converters by type (vc2cosmtypes), the Dynamic TVF's with the
# TVF's (vc2cosmtvf), under xt_vc2cosm; the converters on rom.4's libm (v239): OD/Dist's and the Wave Shaper's
# (vc2cosmdrive), the Resonator's, Frequency Shifter's and Lo-Fi's (vc2cosmbalanced), under xt_vc2cosm; the Side Band
# Filters' (vc2cosmsideband, v241), under xt_vc2cosm; the Matrix core's refresh and groups (v243) in vc2matrix's file;
# the Matrix's sources (vc2matrixsources, v245), under xt_vc2cosm; the library's logf and powf (vc2libm) and their
# constants (vc2libmtables) under xt_vc2libm; the part's voices and their lists (vc2voices, v272) and their constants
# (vc2voicestables) under xt_vc2voices; the part's notes W (vc2notes, v275) under xt_vc2notes; the part's keys
# (vc2keys, v276) and their tables (vc2keystables) under xt_vc2keys; the assigner's first part (vc2assign, v277)
# under xt_vc2voices, its second part; its second part (vc2release, vc2nodenotes, v278) under its third; the voice's
# virtuals (vc2class, v279) and the update queue (vc2queue) under its fourth; the conversion sets C (vc2sets, v280) and
# the notes' Matrix objects M (vc2notematrix) under xt_vc2sets; C's common (vc2setcommon, v281) under its second part;
# C's envelopes (vc2setenvelope, v282) and their tables (vc2setenvtables) under its third.

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
VC2MIX=(Vc2PartMixer Vc2MixerBlock Vc2PartMixerTables Vc2Math Vc2PartMixerCheck Vc2PartMixerFakes)
VC2TONE=(Vc2Tone Vc2ToneNoteOn Vc2ToneCheck Vc2ToneSides Vc2ToneFakes Vc2TonePortParts Vc2ToneEvents Vc2ToneModulators
         Vc2ToneEnvelope Vc2ToneLfo Vc2ToneModCheck XtEnvelope XtLfo Vc2ToneMix Vc2ToneMixRender Vc2ToneMixTables
         Vc2ToneMixCheck Vc2FreeLfos Vc2FreeLfoCheck Vc2ToneLists Vc2ToneMethods Vc2ToneMethodCheck)
VC2OSC=(Vc2OscVoice Vc2OscVoiceGet Vc2OscVoiceKinds Vc2OscTables Vc2OscNoteOn Vc2OscTargets Vc2PitchRamp
        Vc2PitchRampTick Vc2RampTables Vc2OscPlayers Vc2OscCheck Vc2OscEvents Vc2OscGetters Vc2OscFakes
        Vc2OscPortParts Vc2RampCheck Vc2OscRecords Vc2OscLite Vc2OscAnalog Vc2OscAnalogLa Vc2OscAnalogSub
        Vc2OscAnalogShape Vc2OscAnalogDraws Vc2OscAnalogUpdate Vc2OscVariOffRender Vc2VariOffCheck Vc2OscKindsCheck
        Vc2Carrier Vc2CarrierTables Vc2Kind6 Vc2Kind6Core Vc2Kind6Tables Vc2Math Vc2OscVibrato Vc2OscVibratoCheck)
VC2TVA=(Vc2Tva Vc2TvaTables Vc2TvaCheck Vc2TvaEvents Vc2TvaFakes)
VC2COSM=(Vc2CosmGrowl Vc2CosmTables Vc2Math Vc2CosmCheck Vc2CosmConvert Vc2CosmTvf Vc2Matrix Vc2MatrixTables
         Vc2CosmFrameCheck Vc2CosmConverters Vc2CosmFrameDraws Vc2CosmFilters Vc2CosmDynamics Vc2CosmDrive
         Vc2CosmBalanced Vc2CosmSideBand Vc2MatrixCheck Vc2MatrixSources Vc2MatrixSetup Vc2MatrixSetupCheck
         Vc2MatrixSetupDraws Vc2CosmObject Vc2CosmObjectCheck Vc2CosmPools Vc2CosmNoteOn Vc2CosmNoteOnCheck
         Vc2CosmNoteOnDraws Vc2CosmAdapters Vc2CosmAdapterWords Vc2CosmAdapterBlocks Vc2CosmTbAdapter
         Vc2CosmBufferedAdapters Vc2CosmDsp Vc2CosmDspCheck Vc2CosmDspKinds Vc2CosmDspSlots XtCosm XtOsc XtEnvelope)
VC2LIBM=(Vc2LibMath Vc2LibMathTables Vc2LibMathCheck)
VC2VOICES=(Vc2PartVoices Vc2PartVoicesTables Vc2PartVoicesCheck Vc2PartVoicesEvents Vc2PartVoicesFakes
           Vc2Assigner Vc2AssignCheck Vc2AssignEvents Vc2AssignRelease Vc2AssignNotes Vc2AssignReleaseCheck
           Vc2AssignReleaseEvents Vc2Keys Vc2KeysTables Vc2VoiceClass Vc2VoiceQueue Vc2VoiceClassCheck
           Vc2VoiceClassEvents)
VC2NOTES=(Vc2Notes Vc2NotesCheck Vc2NotesEvents Vc2NotesFakes)
VC2KEYS=(Vc2Keys Vc2KeysTables Vc2KeysCheck Vc2KeysEvents Vc2PartNodes Vc2PartNodesCheck)
VC2SETS=(Vc2Sets Vc2NoteMatrix Vc2MatrixSetup Vc2MatrixTables Vc2Matrix Vc2MatrixSources Vc2SetsCheck Vc2SetsDraws
         Vc2SetsEvents Vc2SetCommon Vc2SetCommonCheck Vc2SetEnvelope Vc2SetEnvelopeTables Vc2SetEnvelopeCheck
         Vc2LibMath Vc2LibMathTables Vc2SetChildHead Vc2SetLfo Vc2SetLfoCheck Vc2SetMix Vc2SetMixCheck
         Vc2SetTva Vc2SetTvaCheck)
VC2SETTINGS=(Vc2PartSettings Vc2PartSettingsCheck Vc2SetChildHead Vc2PartSettingsTables Vc2PartSettingsWords
    Vc2PartSettingsOsc Vc2PartSettingsOscCheck Vc2PartSettingsOscBlock Vc2PartSettingsVocoder
    Vc2PartSettingsVocoderCheck Vc2LibMath Vc2LibMathTables Vc2PartSettingsModes Vc2PartSettingsModesCheck
    Vc2PartSettingsDispatch Vc2PartSettingsDispatchCheck Vc2MatrixSetup Vc2MatrixTables Vc2Matrix Vc2MatrixSources
    Vc2PartSettingsCategory Vc2PartSettingsCategoryCheck Vc2StaticMatrix Vc2StaticMatrixCheck
    Vc2PartSettingsSystem Vc2PartSettingsSystemCheck Vc2PartSettingsMessages Vc2PartSettingsMessagesCheck
    Vc2PartNodes Vc2Keys Vc2KeysTables Vc2PartSettingsReceive Vc2PartSettingsReceiveCheck
    Vc2PartSettingsRoutingCheck Vc2PartReloads Vc2PartReloadsCheck)

# port|the planted file|its check's define|the kept objects' folder|the check's files
VC2PORTS=(
    "vc2params|Vc2Params.cpp|XT_VC2PARAMS_MAIN|vc2_obj|VC2PARAMS"
    "vc2in|Vc2Unvoiced.cpp|XT_VC2IN_MAIN|vc2in_obj|VC2IN"
    "vc2out|Vc2Consonants.cpp|XT_VC2IN_MAIN|vc2in_obj|VC2IN"
    "vc2legacy|Vc2LegacyAnalysis.cpp|XT_VC2LEGACY_MAIN|vc2legacy_obj|VC2LEG"
    "vc2tables|Vc2LegacyTables.cpp|XT_VC2LEGACY_MAIN|vc2legacy_obj|VC2LEG"
    "vc2object|Vc2Legacy.cpp|XT_VC2LEGACY_MAIN|vc2legacy_obj|VC2LEG"
    "vc2warp|Vc2HiresoWarp.cpp|XT_VC2HIRESO_MAIN|vc2hireso_obj|VC2HI"
    "vc2hitables|Vc2HiresoTables.cpp|XT_VC2HIRESO_MAIN|vc2hireso_obj|VC2HI"
    "vc2rate|Vc2HiresoRate.cpp|XT_VC2HIRESO_MAIN|vc2hireso_obj|VC2HI"
    "vc2hiana|Vc2HiresoAnalysis.cpp|XT_VC2HIRESO_MAIN|vc2hireso_obj|VC2HI"
    "vc2hiobj|Vc2Hireso.cpp|XT_VC2HIRESO_MAIN|vc2hireso_obj|VC2HI"
    "vc2hisyn|Vc2HiresoSynth.cpp|XT_VC2HIRESO_MAIN|vc2hireso_obj|VC2HI"
    "vc2voc|Vc2Vocoder.cpp|XT_VC2VOCODER_MAIN|vc2voc_obj|VC2VOC"
    "vc2voctables|Vc2VocoderTables.cpp|XT_VC2VOCODER_MAIN|vc2voc_obj|VC2VOC"
    "vc2car|Vc2Carrier.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2cartables|Vc2CarrierTables.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2vocosc|Vc2VocoderOsc.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2oscconv|Vc2OscConvert.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2oscconvtables|Vc2OscConvertTables.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2oscanaconv|Vc2OscAnalogConvert.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2tracker|Vc2Tracker.cpp|XT_VC2TRACKER_MAIN|vc2track_obj|VC2TRACK"
    "vc2tracktables|Vc2TrackerTables.cpp|XT_VC2TRACKER_MAIN|vc2track_obj|VC2TRACK"
    "vc2trackana|Vc2TrackerAnalysis.cpp|XT_VC2TRACKER_MAIN|vc2track_obj|VC2TRACK"
    "vc2trackest|Vc2TrackerEstimate.cpp|XT_VC2TRACKER_MAIN|vc2track_obj|VC2TRACK"
    "vc2math|Vc2Math.cpp|XT_VC2CARRIER_MAIN|vc2car_obj|VC2CAR"
    "vc2k6|Vc2Kind6.cpp|XT_VC2KIND6_MAIN|vc2k6_obj|VC2K6"
    "vc2k6core|Vc2Kind6Core.cpp|XT_VC2KIND6_MAIN|vc2k6_obj|VC2K6"
    "vc2k6tables|Vc2Kind6Tables.cpp|XT_VC2KIND6_MAIN|vc2k6_obj|VC2K6"
    "vc2k6math|Vc2Math.cpp|XT_VC2KIND6_MAIN|vc2k6_obj|VC2K6"
    "vc2an|Vc2AutoNote.cpp|XT_VC2AUTONOTE_MAIN|vc2an_obj|VC2AN"
    "vc2antables|Vc2AutoNoteTables.cpp|XT_VC2AUTONOTE_MAIN|vc2an_obj|VC2AN"
    "vc2mix|Vc2PartMixer.cpp|XT_VC2MIXER_MAIN|vc2mix_obj|VC2MIX"
    "vc2mixblock|Vc2MixerBlock.cpp|XT_VC2MIXER_MAIN|vc2mix_obj|VC2MIX"
    "vc2mixtables|Vc2PartMixerTables.cpp|XT_VC2MIXER_MAIN|vc2mix_obj|VC2MIX"
    "vc2mixmath|Vc2Math.cpp|XT_VC2MIXER_MAIN|vc2mix_obj|VC2MIX"
    "vc2tone|Vc2Tone.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonenoteon|Vc2ToneNoteOn.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2toneenv|Vc2ToneEnvelope.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonelfo|Vc2ToneLfo.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonemods|Vc2ToneModulators.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonemix|Vc2ToneMix.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonemixrender|Vc2ToneMixRender.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonemixtables|Vc2ToneMixTables.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2freelfos|Vc2FreeLfos.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonelists|Vc2ToneLists.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2tonemethods|Vc2ToneMethods.cpp|XT_VC2TONE_MAIN|vc2tone_obj|VC2TONE"
    "vc2osc|Vc2OscVoice.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscget|Vc2OscVoiceGet.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2osckinds|Vc2OscVoiceKinds.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2osctables|Vc2OscTables.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscnoteon|Vc2OscNoteOn.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2osctargets|Vc2OscTargets.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscplayers|Vc2OscPlayers.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2osclite|Vc2OscLite.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscanalog|Vc2OscAnalog.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscanalogla|Vc2OscAnalogLa.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscanalogsub|Vc2OscAnalogSub.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscanalogshape|Vc2OscAnalogShape.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscanalogupdate|Vc2OscAnalogUpdate.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscvarioff|Vc2OscVariOffRender.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2oscvibrato|Vc2OscVibrato.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2ramp|Vc2PitchRamp.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2ramptick|Vc2PitchRampTick.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2ramptables|Vc2RampTables.cpp|XT_VC2OSC_MAIN|vc2osc_obj|VC2OSC"
    "vc2tva|Vc2Tva.cpp|XT_VC2TVA_MAIN|vc2tva_obj|VC2TVA"
    "vc2tvatables|Vc2TvaTables.cpp|XT_VC2TVA_MAIN|vc2tva_obj|VC2TVA"
    "vc2growl|Vc2CosmGrowl.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmtables|Vc2CosmTables.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmmath|Vc2Math.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmconvert|Vc2CosmConvert.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmtvf|Vc2CosmTvf.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2matrix|Vc2Matrix.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2matrixtables|Vc2MatrixTables.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2matrixsources|Vc2MatrixSources.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2matrixsetup|Vc2MatrixSetup.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmobject|Vc2CosmObject.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmpools|Vc2CosmPools.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmnoteon|Vc2CosmNoteOn.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmadapters|Vc2CosmAdapters.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmwords|Vc2CosmAdapterWords.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmblocks|Vc2CosmAdapterBlocks.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmtb|Vc2CosmTbAdapter.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmbuffered|Vc2CosmBufferedAdapters.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmdsp|Vc2CosmDsp.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2xtcosm|XtCosm.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmfilters|Vc2CosmFilters.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmdynamics|Vc2CosmDynamics.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmtypes|Vc2CosmConverters.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmdrive|Vc2CosmDrive.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmsideband|Vc2CosmSideBand.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2cosmbalanced|Vc2CosmBalanced.cpp|XT_VC2COSM_MAIN|vc2cosm_obj|VC2COSM"
    "vc2libm|Vc2LibMath.cpp|XT_VC2LIBM_MAIN|vc2libm_obj|VC2LIBM"
    "vc2libmtables|Vc2LibMathTables.cpp|XT_VC2LIBM_MAIN|vc2libm_obj|VC2LIBM"
    "vc2voices|Vc2PartVoices.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2voicestables|Vc2PartVoicesTables.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2assign|Vc2Assigner.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2release|Vc2AssignRelease.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2nodenotes|Vc2AssignNotes.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2class|Vc2VoiceClass.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2queue|Vc2VoiceQueue.cpp|XT_VC2VOICES_MAIN|vc2voices_obj|VC2VOICES"
    "vc2notes|Vc2Notes.cpp|XT_VC2NOTES_MAIN|vc2notes_obj|VC2NOTES"
    "vc2keys|Vc2Keys.cpp|XT_VC2KEYS_MAIN|vc2keys_obj|VC2KEYS"
    "vc2nodes|Vc2PartNodes.cpp|XT_VC2KEYS_MAIN|vc2keys_obj|VC2KEYS"
    "vc2keystables|Vc2KeysTables.cpp|XT_VC2KEYS_MAIN|vc2keys_obj|VC2KEYS"
    "vc2sets|Vc2Sets.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2notematrix|Vc2NoteMatrix.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2setcommon|Vc2SetCommon.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2setenvelope|Vc2SetEnvelope.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2setenvtables|Vc2SetEnvelopeTables.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2setchildhead|Vc2SetChildHead.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2setlfo|Vc2SetLfo.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2setmix|Vc2SetMix.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2settva|Vc2SetTva.cpp|XT_VC2SETS_MAIN|vc2sets_obj|VC2SETS"
    "vc2settings|Vc2PartSettings.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingstables|Vc2PartSettingsTables.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingswords|Vc2PartSettingsWords.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsosc|Vc2PartSettingsOsc.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsblock|Vc2PartSettingsOscBlock.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsvocoder|Vc2PartSettingsVocoder.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsmodes|Vc2PartSettingsModes.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsdispatch|Vc2PartSettingsDispatch.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsmatrix|Vc2MatrixSetup.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingscategory|Vc2PartSettingsCategory.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2staticmatrix|Vc2StaticMatrix.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingssystem|Vc2PartSettingsSystem.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsmessages|Vc2PartSettingsMessages.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2settingsreceive|Vc2PartSettingsReceive.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
    "vc2partreloads|Vc2PartReloads.cpp|XT_VC2SETTINGS_MAIN|vc2settings_obj|VC2SETTINGS"
)

# vc2Port <port>: mutate.sh's IMAGE, ARGS and F for the port, and its check's other files kept (harnessParts); a port
# not in the table stops the run, saying so
vc2Port() {
    local row port file define objdir list
    for row in "${VC2PORTS[@]}"; do
        IFS='|' read -r port file define objdir list <<< "$row"
        [ "$port" = "$1" ] || continue
        IMAGE="$ROM4"
        ARGS=("${SEED:-2024}")
        F=$file
        local parts="$list[@]"
        harnessParts "$define" "$objdir" "${!parts}"
        return 0
    done
    echo "mutate: no port $1"
    exit 2
}
