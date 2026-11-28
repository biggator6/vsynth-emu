#pragma once
// The PATCH Init window's callback as xt_files runs it on rom.2 (v473): what its EXECUTE asks for past the init (the
// patch's change for the part, the screen's refresh and its move to the edit screen) and its CANCEL's close, each
// hooked to answer 0 and be counted.  XT_FILES_MAIN builds only.
#include "Sh4Interp.h"
#include <cstdint>
#include <cstdio>

// An image's four, by address
struct XtInitCalls {
    uint32_t change, refresh, toScreen, close;
};

// What the callback asked for.  Hooked by `hook`, which keeps `this`: not to be moved after
struct XtInitAsked {
    int changes = 0, changedPart = -1, changeArg = -1, refreshes = 0, screen = -1, closes = 0;
    void hook(Sh4& c, const XtInitCalls& at) {
        c.hook(at.change, [this](Sh4& s) {
            ++changes;
            changedPart = s.rd8(s.r[4]);
            changeArg = static_cast<int>(s.r[5]);
            s.ret(0);
        });
        c.hook(at.refresh, [this](Sh4& s) { ++refreshes; s.ret(0); });
        c.hook(at.toScreen, [this](Sh4& s) { screen = static_cast<int>(s.r[4]); s.ret(0); });
        c.hook(at.close, [this](Sh4& s) { ++closes; s.ret(0); });
    }
    // EXECUTE's: the change asked once, for part 0 with 1, one refresh, the edit screen, no close
    bool executed(int editScreen) const {
        return changes == 1 && changedPart == 0 && changeArg == 1 && refreshes == 1 && screen == editScreen
               && closes == 0;
    }
    bool cancelled() const { return changes == 0 && closes == 1; }   // CANCEL's: no change, the window closed
    void say(const char* what) const {
        std::printf("%s: asked %d changes (part %d, %d), %d refreshes, screen %d, %d closes\n", what, changes,
                    changedPart, changeArg, refreshes, screen, closes);
    }
};
