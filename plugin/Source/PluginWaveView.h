#pragma once
// A loaded wave as the editor draws it (PluginWaveView.cpp, v200): kViewColumns columns, each its lowest and highest
// sample, the channels mixed; made as the wave goes to its encode.  The processor's WaveView (PluginProcessor.h), moved
// here as it stood (v455) when the processor's header reached the kit's 300 lines.
#include <vector>

struct PluginWaveView {
    static constexpr int kViewColumns = 512;
    struct Column { float low, high; };
    std::vector<Column> columns;
};
