#pragma once
// A harness's or research tool's arguments as numbers: one reader for the checks (XT_*_MAIN) and research/tools, which
// build with -Iplugin/Source.  Moved from research/tools/vc2run.cpp (v204), so fxmap and xt_fx read theirs the same way.
#include <charconv>
#include <cstring>
#include <optional>

// The whole of s as a number, or none when it doesn't read as one
inline std::optional<int> intArg(const char* s) {
    int v = 0;
    const char* end = s + std::strlen(s);
    const auto [at, why] = std::from_chars(s, end, v);
    if (why != std::errc() || at != end) return std::nullopt;
    return v;
}
