#pragma once
// A loaded wave as the editor draws it (PluginWaveView.cpp, v200): kViewColumns columns, each its lowest and highest
// sample, the channels mixed; made as the wave goes to its encode.  The processor's WaveView (PluginProcessor.h), moved
// here as it stood (v455) when the processor's header reached the kit's 300 lines.  v479: the WAVE page draws any
// window of it, so it keeps its frames, mixed, and counts the wave as the XT holds it.
#include <vector>

struct PluginWaveView {
    static constexpr int kViewColumns = 512;
    struct Column { float low, high; };
    std::vector<Column> columns;   // the whole wave in kViewColumns, as the OSC panel draws it
    // Its frames as loaded, the channels mixed, at their file's rate; and the wave as the XT holds it, at 44.1 kHz, the
    // frames every point counts: its encode's once that has run, before it the file's at its rate
    std::vector<float> mono;
    double rate = 44100.0;
    long frames = 0;
    // n columns of `span` of the XT's frames from `first`, each the lowest and highest of the file's frames under it
    std::vector<Column> window(long first, long span, int n) const;
};
