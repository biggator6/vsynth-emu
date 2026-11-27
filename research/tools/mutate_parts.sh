# mutate_parts.sh: a harness's kept objects for mutate.sh, which sources it (moved out of it as they stood, v208).
# Each sets D, OBJ, EXTRA and L for mutate.sh's link, from its S, M, F, TC and SDK, and env.sh's HARNESS_SWITCHES.

# A port whose check is a harness of its own with the patches' reads (xt_arp, xt_msm): its files and those reads, all but
# the planted one built once into $M/<objdir> with the research switches, a changed header building them again
harnessParts() {   # harnessParts <define> <objdir> <the port's own files, without .cpp ...>
    D=$1
    OBJ="$M/$2"
    shift 2
    EXTRA=("${HARNESS_SWITCHES[@]}")
    mkdir -p "$OBJ"
    L=()
    local f H
    H=$(ls -t "$S"/*.h | head -1)
    for f in "$@" XtFiles XtWaveFile XtDiskImage XtPatchStore XtFirmware XtFwMachine XtMultiBandPlayer XtSoloVoice \
             XtSoloEncoder Sh4Interp Sh4Setup XtFwEncode XtFwEvents; do
        [ "$f.cpp" = "$F" ] && continue
        L+=("$OBJ/$f.o")
        [ "$OBJ/$f.o" -nt "$S/$f.cpp" ] && [ "$OBJ/$f.o" -nt "$H" ] && continue
        "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 -D"$D" -DXT_HARNESS_SWITCHES -c "$S/$f.cpp" -o "$OBJ/$f.$$.o" \
            && mv "$OBJ/$f.$$.o" "$OBJ/$f.o"
    done
}
# xt_fx's parts but the planted one: the ports named and the interpreter, each a kept object in $M/fx_obj; then its check,
# its own files from v204 (XtFxCheck*.cpp, moved out of XtFxChip.cpp), with the research switches.  The fx ports' plants
# are in the ports, never in the check.
fxParts() {   # fxParts <the ports kept, without .cpp ...>
    D=XT_FX_MAIN
    EXTRA=(-I"$REPO/research/third_party/gearmulator_esp" "${HARNESS_SWITCHES[@]}")
    OBJ="$M/fx_obj"
    mkdir -p "$OBJ"
    L=("$S/Sh4Setup.cpp")
    local f src o
    for f in Sh4Interp "$@"; do
        keptObject "$OBJ/$f.o" "$S/$f.cpp"
        L+=("$OBJ/$f.o")
    done
    for src in "$S"/XtFxCheck*.cpp; do
        o="$OBJ/$(basename "$src" .cpp).o"
        keptObject "$o" "$src" -DXT_FX_MAIN -DXT_HARNESS_SWITCHES -I"$REPO/research/third_party/gearmulator_esp" -I"$S"
        L+=("$o")
    done
}
