#pragma once
// xt_vc2settings' further parts (Vc2PartSettingsCheck.h's until v304, when it reached the 300-line limit): a part's
// row (its seed's offset, its counts, a case of its events, its line, what it does first), a count in a line said by
// its counts alone, the runner, and each part's row, defined in its own file.  Vc2PartSettingsCheck.h includes it at
// its end.  XT_VC2SETTINGS_MAIN only.
#include <span>
#include <string>

// A count in a part's line said by its counts alone (v298): its event's index in byEvent, what it counts
struct SettingsSaid {
    int event;
    const char* what;
};
// A further part (v289): its seed's offset from the run's, its counts, a case of its events, its line, and what it
// does first; from v298, a line of its events alone by its name and counts, with no `say`
struct SettingsPartRow {
    uint32_t offset;
    long SettingsCounts::*events;
    long SettingsCounts::*calls;
    std::string (*oneCase)(SettingsSides&, std::mt19937&, long&, long&);
    void (*say)(const SettingsCounts&);
    void (*start)(SettingsSides&) = nullptr;
    const char* name = nullptr;
    std::span<const SettingsSaid> said {};
};
// Its cases, its line, how many differ
long settingsRunPart(SettingsSides& s, const Vc2Run& run, const SettingsPartRow& p);
// The third part (v288): the OSC table's readers, the block reader hooked; the fourth (v289): rom.4's own
// (Vc2PartSettingsOscCheck.cpp); the fifth (v290): the vocoder reader (Vc2PartSettingsVocoderCheck.cpp)
extern const SettingsPartRow kOscPartRows[2];
extern const SettingsPartRow kVocoderPartRow;
// The sixth (v291): the mode row and Auto Note's switch (Vc2PartSettingsModesCheck.cpp)
extern const SettingsPartRow kModePartRow;
// The seventh (v292): the dispatchers and G's channel word (Vc2PartSettingsDispatchCheck.cpp)
extern const SettingsPartRow kDispatchPartRow;
// The eighth (v294): the Category and budget (Vc2PartSettingsCategoryCheck.cpp)
extern const SettingsPartRow kCategoryPartRow;
// The ninth (v295): the static M's initializers (Vc2StaticMatrixCheck.cpp)
extern const SettingsPartRow kStaticPartRow;
// The tenth (v296): the system's readers and the budget (Vc2PartSettingsSystemCheck.cpp)
extern const SettingsPartRow kSystemPartRow;
// The eleventh (v297): the parameters' messages (Vc2PartSettingsMessagesCheck.cpp)
extern const SettingsPartRow kMessagePartRow;
// The twelfth (v298): the receive channel (Vc2PartSettingsReceiveCheck.cpp)
extern const SettingsPartRow kReceivePartRow;
// The thirteenth (v299): the routing's reset (Vc2PartSettingsRoutingCheck.cpp)
extern const SettingsPartRow kRoutingPartRow;
// The fourteenth (v300): the reload objects (Vc2PartReloadsCheck.cpp)
extern const SettingsPartRow kReloadPartRow;
// The fifteenth (v301): the controllers (Vc2PartControllersCheck.cpp)
extern const SettingsPartRow kControllerPartRow;
// The sixteenth (v302): the controller numbers and the part's init (Vc2PartNumbersCheck.cpp)
extern const SettingsPartRow kNumberPartRow;
// The seventeenth (v303): the number classes' handlers (Vc2PartHandlersCheck.cpp)
extern const SettingsPartRow kHandlerPartRow;
// The eighteenth (v304): the part's start-up (Vc2PartStartUpCheck.cpp)
extern const SettingsPartRow kStartPartRow;
// The nineteenth (v306): the Time Trip (Vc2TimeTripCheck.cpp)
extern const SettingsPartRow kTripPartRow;
