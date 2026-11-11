// arptext <rom.2>: the front panel's text for the arpeggio's values, by the firmware's own routine (0x89461a70, through
// XtFxValueText): Motif, Duration, Shuffle Resolution, Keyboard Velocity, Octave Range, Grid, End Point and a step's
// cell (v188, HARDWARE_FIRMWARE.md, "The arpeggiator").  Research only; not in build.sh.  Build from plugin/Source:
//   $TC/clang++ -isysroot $SDK -std=c++20 -O1 -I. -I$REPO/research/third_party/gearmulator_esp \
//       $REPO/research/tools/arptext.cpp XtFxControl.cpp XtFxChip.cpp Sh4Interp.cpp -o $WORK/bin/arptext
#include "XtFxControl.h"
#include <cstdio>
#include <initializer_list>

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    XtFxValueText t(argv[1]);
    if (!t.ok()) return 2;
    struct Shown {
        int id;
        std::initializer_list<int> values;
    };
    const Shown shown[] = {
        { 0x430, { 0, 1, 2, 3, 4, 5, 6, 7 } }, { 0x431, { 0, 100 } }, { 0x433, { 0, 1 } },
        { 0x434, { 0, 1, 127 } },               { 0x435, { 1, 4, 7 } }, { 0x436, { 0, 1, 2, 3, 4, 5 } },
        { 0x437, { 0, 31 } },                   { 0x44d, { 0, 1, 2, 3, 100, 101, 127, 128 } },
    };
    for (const Shown& s : shown)
        for (const int v : s.values) std::printf("%03x %3d: %s\n", s.id, v, t.text(s.id, v).c_str());
    return 0;
}
