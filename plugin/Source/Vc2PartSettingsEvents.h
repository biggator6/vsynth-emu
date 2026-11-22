#pragma once
// xt_vc2settings' events by what a routine is given (Vc2PartSettingsCheck.h's from v288 until v297): a part's
// reader on a drawn part, a kind's row, a row, a system's reader.  XT_VC2SETTINGS_MAIN only.
#include "Vc2PartSettingsCheck.h"

// A part's reader on a drawn part, the port's side `mine(part)` (v289)
template <uint32_t Routine, uint32_t TableAt, size_t Counted, class Mine>
std::string partEventWith(SettingsSides& s, std::mt19937& rng, const Mine& mine) {
    const int part = (int)(rng() % kSettingsParts);
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a part's reader", vc2R0 }, { TableAt, (uint32_t)part },
                          [&] { return (uint32_t)mine(part); });
}

template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string partEvent(SettingsSides& s, std::mt19937& rng) {
    const auto mine = [&](int part) { return (s.settings.*Member)(s.rows, part); };
    return partEventWith<Routine, TableAt, Counted>(s, rng, mine);
}

template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string kindEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts), kind = (int)(rng() % kSettingsKinds);
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a kind's row", vc2R0 }, { TableAt, (uint32_t)part, (uint32_t)kind },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part, kind); });
}

template <auto Member, uint32_t Routine, uint32_t TableAt, int Rows, size_t Counted>
std::string rowEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts), kind = (int)(rng() % kSettingsKinds), i = (int)(rng() % Rows);
    ++s.n.byEvent[Counted];
    const Vc2CheckRoutine r { Routine, "a row", vc2R0 };
    return settingsOnBoth(s, r, { TableAt, (uint32_t)part, (uint32_t)kind, (uint32_t)i },
                          [&] { return (uint32_t)(s.settings.*Member)(s.rows, part, kind, i); });
}

// A system's reader: rom.4's answers 1 (0x893a71d0 reads no table: the word it's given unread)
template <auto Member, uint32_t Routine, uint32_t TableAt, size_t Counted>
std::string systemEvent(SettingsSides& s, std::mt19937&) {
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a system's reader", vc2R0 }, { TableAt }, [&] {
        (s.settings.*Member)(s.rows);
        return 1u;
    });
}
