# vc2_tables.sh: the files of rom.4's tables that XtSynthRom.cpp's loadVc2 loads, which every program linking
# XtSynthRom.cpp links with it: build.sh's xt_synth, xt_tone and xt_vc2params, ab.sh's XtSynth CLI on both trees, and
# mutate.sh's kept objects for the tone ports.  Sourced by them.  Until v231 each had its own copy of this list, and
# mutate.sh's lacked v220's loaders on, so its tone ports didn't link once XtSynthRom.o was built again.  A VC-2 port's
# new loader goes here, as its call goes into loadVc2.

# A row a loader: its file, then the files it needs besides, without .cpp
VC2_TABLES=(
    "Vc2Params XtFxValueText"   # rom.4's parameters (v199), and the values' text they read with
    Vc2Unvoiced                 # the unvoiced detector's coefficients (v208)
    Vc2LegacyAnalysis           # LEGACY's analysis (v210)
    Vc2LegacyTables             # and its object's (v211)
    Vc2HiresoTables             # HIRESO's (v213)
    Vc2VocoderTables            # the vocoder object's (v220)
    Vc2CarrierTables            # the VOCODER OSC's carrier's (v221)
    Vc2TrackerTables            # the pitch tracker's (v223)
    "Vc2Kind6Tables Vc2Math"    # voice kind 6's (v225), and the math it shares
    Vc2AutoNoteTables           # Auto Note's (v226)
    Vc2PartMixerTables          # the part mixer's (v227)
    Vc2OscTables                # the OSC voice's (v230)
    Vc2TvaTables                # the TVA's (v233)
    Vc2CosmTables               # COSM's (v234)
    Vc2MatrixTables             # the Matrix's (v235)
)

# vc2Tables <source dir> [suffix]: the rows' files the tree has, one a line, each with the suffix (.cpp for a source).
# A row is there when its loader's file is: an older tree, as ab.sh's revision may be, has fewer.
vc2Tables() {
    local row
    local files=()
    for row in "${VC2_TABLES[@]}"; do
        read -r -a files <<< "$row"
        if [ -f "$1/${files[0]}.cpp" ]; then
            printf '%s\n' "${files[@]/%/${2:-}}"
        fi
    done
    return 0
}
