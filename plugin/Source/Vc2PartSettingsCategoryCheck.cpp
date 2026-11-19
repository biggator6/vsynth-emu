// xt_vc2settings' eighth part (v294): the Category and budget 0x89392c60 against rom.4's, on the seed seven after the
// run's, every table, the held note's word and the nodes noise; given the node of source 1 and a drawn part, as the
// part's handler gives it.  Its callees hooked to log among the reads (the budget's setter 0x89391240, the controller
// 0x89392f90, the voices' truncation 0x89390fd0, all notes off 0x89391e30; Auto Note's two setters by v290's hooks),
// the port's calls out the same through SettingsPortOuts; Patch Category drawn 0…13 half the time; the stack word
// Auto Note's switch's second read object leaves, at its depth under this routine, given v291's handle of its own.
// XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
constexpr uint32_t kCategoryAt = 0x89392c60;   // [FW]
// [FW] Auto Note's switch's second read object's word, under this routine: its 16 bytes pushed and 28 more, then the
// switch's 32 and the word at its +0xc (on the XT the saved r14 of 0x89390fd0 lies there: the getter's address)
constexpr uint32_t kLeftoverWordAt = kVc2Sp - 64;
constexpr uint32_t kNodeBytes = 16;
constexpr SettingsNarrow kCategoryNarrow[] = { { 0x12c, 14, kHalfTheTime } };   // [FW] Patch Category, past 11 too

// [FW] the callees and their calls as the log has them
SettingsCall budget(Sh4& c) { return { kOutBudget, 0, 0, 0, (int32_t)c.r[4] }; }
SettingsCall control(Sh4& c) { return { kOutController, (int32_t)c.r[4], (int32_t)c.r[5], (int32_t)c.r[6],
                                        (int32_t)c.r[7] }; }
SettingsCall truncated(Sh4& c) { return { kOutTruncate, 0, 0, 0, (int32_t)c.r[4] }; }
SettingsCall notesOff(Sh4&) { return { kOutNotesOff, 0, 0, 0, 0 }; }
constexpr SettingsCallee kCategoryCallees[] = { { 0x89391240, budget }, { 0x89392f90, control },
                                                { 0x89390fd0, truncated }, { 0x89391e30, notesOff } };

void hookCategory(SettingsSides& s) {
    s.rom.store.narrow = kCategoryNarrow;
    settingsHookCallees(s, kCategoryCallees);
}

std::string categoryEvent(SettingsSides& s, std::mt19937& rng) {
    const int part = (int)(rng() % kSettingsParts);
    s.fw.cpu.wr32(kLeftoverWordAt, kLeftoverHandle);
    ++s.n.byEvent[36];
    const std::string what = settingsOnBoth(s, { kCategoryAt, "the Category and budget", nullptr },
                                            { kNodesAt + kNodeBytes * (uint32_t)part }, [&] {
        s.settings.category(s.rows, s.nodes, part, s.outs);
        return 0u;
    });
    s.n.heldNotes += s.rows.heldNote;
    for (const SettingsCall& c : s.mine.log) s.n.notesOff += c.block == kOutNotesOff;
    return what;
}

const SettingsEvent kCategoryEvents[] = { categoryEvent };

void sayCategoryPart(const SettingsCounts& n) {
    std::printf("Category: %ld events, %ld holding a note, %ld all notes off (%ld calls)\n", n.categoryEvents,
                n.heldNotes, n.notesOff, n.categoryCalls);
}
} // namespace

const SettingsPartRow kCategoryPartRow { 7, &SettingsCounts::categoryEvents, &SettingsCounts::categoryCalls,
                                         settingsNodesCaseOf<kCategoryEvents, kV295Rows>, sayCategoryPart,
                                         hookCategory };
#endif
