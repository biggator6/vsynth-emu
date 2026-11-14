# objects.sh, sourced: an object file kept between runs, built again only when it can be stale. mutate.sh's fx ports
# keep theirs (v197: they went by two headers alone, and a changed Sh4Interp.h left XtFxChip_main.o built on the old
# Sh4 class, which crashed their run with nothing planted). Needs $S (the sources' folder), $TC and $SDK (env.sh).

# True when object $1 is missing, or older than its source $2 or any header in $S
objectStale() {
    local h
    [ "$1" -nt "$2" ] || return 0
    for h in "$S"/*.h; do
        [ "$1" -nt "$h" ] || return 0
    done
    return 1
}

# Object $1 from source $2, with the compiler flags after them, when objectStale says so
keptObject() {
    local object=$1 source=$2
    shift 2
    objectStale "$object" "$source" || return 0
    "$TC/clang++" -isysroot "$SDK" -std=c++20 -O2 "$@" -c "$source" -o "$object.$$" && mv "$object.$$" "$object"
}
