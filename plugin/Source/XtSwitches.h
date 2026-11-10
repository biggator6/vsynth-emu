#pragma once
// The research switches the harnesses take from their environment (notes.txt says what each does): XT_ROUND, the
// SOLO encoder's XT_TRACE, XT_DUMPDEC and XT_DUMPFIR, and the earlier engine's VSE_*.  A build that defines
// XT_HARNESS_SWITCHES reads them, through research/tools/harness_switches.cpp, as build.sh's and ab.sh's programs do.
// The plugin's targets don't: there every switch is unset and its branch compiled away, so a host started from a shell
// that sets one plays as the XT does (R8 of .claude/claude-packs-review.md, v174).  A new switch is read here, never
// by getenv.  VSE_APP_DIR and VSE_XT_ROM are the plugin's own, where its tests point it, and aren't switches.
#include <charconv>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <system_error>

#ifdef XT_HARNESS_SWITCHES
const char* xtSwitchFromEnvironment(const char* name);   // research/tools/harness_switches.cpp
#endif

// Its text; nullptr when it isn't set, and in the plugin's builds always
inline const char* xtSwitch([[maybe_unused]] const char* name) {
#ifdef XT_HARNESS_SWITCHES
    return xtSwitchFromEnvironment(name);
#else
    return nullptr;
#endif
}

// Whether it's set to this text
inline bool xtSwitchIs(const char* name, const char* text) {
    const char* v = xtSwitch(name);
    return v != nullptr && std::strcmp(v, text) == 0;
}

// Its number; `otherwise` when it isn't set or doesn't read as one
inline int xtSwitchInt(const char* name, int otherwise) {
    const char* v = xtSwitch(name);
    int x = 0;
    if (v == nullptr || std::from_chars(v, v + std::strlen(v), x).ec != std::errc()) return otherwise;
    return x;
}
inline double xtSwitchReal(const char* name, double otherwise) {
    const char* v = xtSwitch(name);
    if (v == nullptr) return otherwise;
    char* end = nullptr;
    const double x = std::strtod(v, &end);
    return end != v ? x : otherwise;
}

// The bytes to the file it names, if it's set
inline void xtSwitchDump(const char* name, const void* bytes, std::size_t n) {
    const char* path = xtSwitch(name);
    if (path == nullptr) return;
    FILE* f = std::fopen(path, "wb");
    if (f == nullptr) {
        std::fprintf(stderr, "%s: can't write %s\n", name, path);
        return;
    }
    if (std::fwrite(bytes, 1, n, f) != n) std::fprintf(stderr, "%s: %s is short\n", name, path);
    std::fclose(f);
}

// XT_ROUND=nearest: the P-code emulator's round to nearest, not the SH-4's round to zero; read once
inline bool xtSwitchNearest() {
    static const bool nearest = xtSwitchIs("XT_ROUND", "nearest");
    return nearest;
}
