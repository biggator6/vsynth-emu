#pragma once
// A check's two logs of the calls a routine made, rom.4's and the port's, compared (v228: the part mixer's and the tone's
// checks; moved out of Vc2PartMixerFakes.cpp).  Check builds only.
#include <algorithm>
#include <string>
#include <vector>

// How many of a log's calls are of `kind` (v229: the mixer's and the tone's checks' counts)
template <class Call>
long vc2CountCalls(const std::vector<Call>& log, int32_t kind) {
    return (long)std::ranges::count(log, kind, &Call::kind);
}

// The first call the logs differ at, each side's said by `said(call)` ("none" past a log's end); empty when they don't
template <class Call, class Said>
std::string vc2CallsDifference(const std::vector<Call>& rom, const std::vector<Call>& port, Said said) {
    const auto [a, b] = std::ranges::mismatch(rom, port);
    if (a == rom.end() && b == port.end()) return "";
    const auto text = [&](const auto& at, const std::vector<Call>& log) {
        return at == log.end() ? std::string("none") : said(*at);
    };
    return "the calls, call " + std::to_string(a - rom.begin()) + ": " + text(b, port) + ", against " + text(a, rom);
}
