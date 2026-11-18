#pragma once
// What xt_fx's parts do with the driver's words: compare two sides' and play the queue's onto a chip.  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxChip.h"
#include "XtFxControl.h"

namespace xtfxcheck {
// How many of the driver's 2048 words the two hold differently: `a` by anything with a word(i)
template <typename Words> int wordsApart(const Words& a, const XtFxChip& b) {
    int d = 0;
    for (unsigned i = 0; i < 2048; i++) d += (a.word(i) & 0x0fffffff) != (b.word(i) & 0x0fffffff);
    return d;
}

// one-off: XtFxUnit::settle's loop with a limit, as a check must end when the queue never empties.  The frames it played.
inline long playQueue(XtFxControl& c, XtFxChip& chip, long most) {
    long played = 0;
    for (; played < most && !c.drained(); played++) {
        c.pump();
        c.service(chip);
        chip.runFrame();
    }
    return played;
}
} // namespace xtfxcheck
#endif
