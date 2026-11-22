# mutate_cap.sh, sourced by mutate.sh (v312): capped <program> <args...> runs a mutant as timeout(1) would, its output
# and status its own, but stopped past MUT_TIMEOUT seconds (default 1500) with timeout's status 124, and killed past
# MUT_MEM_MB of resident memory (default 2048) with SIGKILL's 137: a run that died either way, so its plant reads as
# caught.  Polled every 0.1 s, so a run may pass the cap by what it grows in that time before it dies (an allocator at
# 1.3 GB a second passed a 512 MB cap by 190 MB at 0.2 s).  The memory cap: two plant runs of v312 whose mutants' walks
# never ended took the machine's memory (four held 63 to 152 GB at the panic); the clean checks peak under 300 MB
# (xt_tone's 278 at 2000 cases), and mutlist's six jobs at the cap leave a 24 GB machine room.  mutate_cap_test.sh
# proves it.  From v314 the watch runs with the shell's own stderr to /dev/null: the shell says how a background job
# ended ("Bus error: 10") when it notices, and mutlist reads " error" in a run's output as a plant that never ran, so a
# mutant that crashed read ERROR where timeout(1), which said nothing, had it caught.  The mutant's own stderr is
# its own, given before the watch.
capped() {
    "$@" &
    capWatch "$!" 2> /dev/null
}

capWatch() {   # capWatch <pid>: the run's status, or 124 past the time, the run killed past the memory
    local pid=$1
    local kb=$(( ${MUT_MEM_MB:-2048} * 1024 ))
    local polls=$(( ${MUT_TIMEOUT:-1500} * 10 ))
    local n=0 rss
    while kill -0 "$pid"; do
        rss=$(ps -o rss= -p "$pid" | tr -d ' ')
        if [ "${rss:-0}" -gt "$kb" ]; then
            kill -9 "$pid"
            break
        fi
        n=$(( n + 1 ))
        if [ "$n" -gt "$polls" ]; then
            kill -TERM "$pid"
            wait "$pid"
            return 124
        fi
        sleep 0.1
    done
    wait "$pid"
}
