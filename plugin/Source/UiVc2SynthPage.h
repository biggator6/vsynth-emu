#pragma once
// The VC-2 mode's SYNTH page (v453; research/UI_DESIGN_VC2.md, "SYNTH" and "A zone, a COSM and a face as the selected
// one"): D's path, rack and lower panel made from the VC-2's tables for the selected zone, which is the patch's Current
// Zone (0x140, d-3), chosen by the path's Z1 and Z2 chips.  A change of zone makes them again for the other zone's
// parameters (one set of controls, never two with one hidden), keeping the block selected and the lower panel's view.
#include "PluginProcessor.h"
#include "UiLowerPanel.h"
#include "UiPath.h"
#include "UiRack.h"
#include <functional>
#include <memory>

class Vc2SynthPage : public juce::Component {
public:
    explicit Vc2SynthPage(VSynthEmuProcessor& p);
    std::function<void(int effect)> onEffect;   // an effect's chip: 0 MFX, 1 Chorus, 2 Reverb
    std::function<void()> onVocoder;            // the VOCODER chip
    void select(int block);
    int selected() const { return selected_; }
    int zone() const { return zone_; }
    PathBar& path() { return *path_; }
    Rack& rack() { return *rack_; }
    LowerPanel& lower() { return *lower_; }
    void refresh();   // the editor's timer: the zone, then what the path, the rack and the lower panel show
    void resized() override;

private:
    VSynthEmuProcessor& processor_;
    std::unique_ptr<PathBar> path_;
    std::unique_ptr<Rack> rack_;
    std::unique_ptr<LowerPanel> lower_;
    int zone_ = -1, selected_;
    void make(int zone);
};
