# mutate_vc2ports.sh: rom.4's ports for mutate.sh, which sources it (moved out of it as they stood, v220, as a table).
# Each port plants its file under its check, on rom.4, [cases] the check's cases and SEED any's: rom.4's parameter
# tables (vc2params, v199) under xt_vc2params, [cases] its values an ID; the unvoiced detector (vc2in, v208) and its
# consonants (vc2out, v209) under xt_vc2in; LEGACY's analysis (vc2legacy, v210), tables (vc2tables) and object
# (vc2object, v211) under xt_vc2legacy; HIRESO's warp (vc2warp, v213), tables (vc2hitables), rate filters (vc2rate,
# v214), analysis (vc2hiana, v215), object (vc2hiobj, v216) and synthesis (vc2hisyn, v217) under xt_vc2hireso; the
# vocoder object (vc2voc, v220) and its tables (vc2voctables) under xt_vc2vocoder; the VOCODER OSC's carrier (vc2car,
# v221) and its tables (vc2cartables) under xt_vc2carrier.

# Each check's files, without .cpp
VC2PARAMS=(Vc2Params Vc2ParamsCheck XtFxValueText)
VC2IN=(Vc2Unvoiced Vc2Consonants Vc2UnvoicedCheck Vc2CheckWords)
VC2LEG=(Vc2LegacyAnalysis Vc2LegacyTables Vc2Legacy Vc2LegacyCheck Vc2LegacySynthCheck Vc2CheckWords)
VC2HIPORTS=(Vc2HiresoWarp Vc2HiresoTables Vc2HiresoRate Vc2HiresoAnalysis Vc2Hireso Vc2HiresoSynth)
VC2HI=("${VC2HIPORTS[@]}" Vc2Unvoiced Vc2HiresoCheck Vc2HiresoRateCheck Vc2HiresoAnalysisCheck Vc2HiresoObjectCheck
       Vc2CheckWords)
VC2VOC=(Vc2Vocoder Vc2VocoderTables Vc2Unvoiced Vc2Consonants Vc2LegacyAnalysis Vc2Legacy Vc2LegacyTables
        "${VC2HIPORTS[@]}" Vc2VocoderCheck Vc2CheckWords)
VC2CAR=(Vc2Carrier Vc2CarrierTables Vc2CarrierCheck)

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
