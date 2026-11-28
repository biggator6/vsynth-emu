// A wave saved as the XT saves one from the WAVE page: PluginWaveEdit.h's PluginWaveEdits::save (v489, d-8's file).
#include "PluginWaveEdit.h"
#include "PluginParamIds.h"
#include "PluginProcessor.h"
#include "XtFwWaveFile.h"
#include <array>

namespace {
// Where the session keeps each OSC's file (PluginProcessor.cpp's loads)
const std::array<const char*, 2> kFileKeys { "loadedFilePath", "loadedFilePath2" };

// The loops the wave plays, in its frames: the forward loop the OSC's Loop Start and End (percents) as XtSynth reads
// them, the backward loop the session's or the wave's own (`bwd`), else the forward's
XtWaveLoops loopsOf(float startPercent, float endPercent, long frames, const PluginWaveEdit& bwd) {
    XtWaveLoops l { waveLoopFrame(startPercent, frames), waveLoopFrame(endPercent, frames), 0, 0 };
    l.bwdStart = bwd.backward ? bwd.bwdStart : l.loopStart;
    l.bwdEnd = bwd.backward ? bwd.bwdEnd : l.loopEnd;
    return l;
}
} // namespace

juce::File PluginWaveEdits::fileOf(const VSynthEmuProcessor& p, int osc) {
    return juce::File(p.apvts.state.getProperty(kFileKeys[(size_t)(osc != 0)]).toString());
}

void PluginWaveEdits::save(VSynthEmuProcessor& p, int osc, const juce::File& to) {
    const juce::String name = "OSC " + juce::String(osc + 1), who = name + ": Save Wave: ";
    const juce::File from = fileOf(p, osc);
    const auto view = p.waveView(osc);
    if (!p.hasFileLoaded(osc) || !from.existsAsFile() || !view) return p.setStatus(who + "no file to save from");
    const PluginWaveEdit kept = edit(p, osc);
    if (!kept.events.empty()) return p.setStatus(who + "its event edits aren't saved yet");
    XtWaveEdits edits;
    if (kept.cut) edits.cut = XtWaveCut { kept.keepFrom, kept.keepTo };
    const float start = p.raw(Params::oscId(osc, Params::kLoopStart));
    const float end = p.raw(Params::oscId(osc, Params::kLoopEnd));
    edits.loops = loopsOf(start, end, view->frames, backward(p, osc));
    juce::MemoryBlock bytes;
    if (!from.loadFileAsData(bytes)) return p.setStatus(who + "couldn't read " + from.getFileName());
    std::vector<uint8_t> wave((const uint8_t*)bytes.getData(), (const uint8_t*)bytes.getData() + bytes.getSize());
    p.addEncode([&p, wave = std::move(wave), edits, to, name, who](const std::atomic<bool>&) {
        std::vector<uint8_t> file;
        std::string why;
        if (!xtFwWaveFile(wave, file, &why, edits)) return p.setStatus(who + juce::String(why));
        if (!to.replaceWithData(file.data(), file.size()))
            return p.setStatus(who + "couldn't write " + to.getFileName());
        p.setStatus(name + ": Saved " + to.getFileName());
    });
}
