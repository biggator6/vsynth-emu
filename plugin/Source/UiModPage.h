#pragma once
// D's MODULATION page (research/UI_DESIGN.md, "The pages"; UI step 4): the Multi Step Modulator's card down the left,
// the Matrix's card and the controller assigns' card down the right.  A mode's (v453): the VC-2's has no Multi Step
// Modulator (rom.4 has none), its Matrix down the left by rom.4's IDs and its assigns with Beam Type at the right
// (research/UI_DESIGN_VC2.md, "MODULATION").
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiControlGrid.h"
#include "UiMatrix.h"
#include "UiMsm.h"
#include <memory>

class ModulationPage : public juce::Component {
public:
    // A mode's page: its Matrix's IDs and its assigns' rows; with no MSM, the Matrix takes the left column
    struct Mode {
        bool msm = true;
        Matrix::FieldIds matrix = Matrix::controlFieldId;
        ControlGrid::Rows assigns;
    };
    explicit ModulationPage(VSynthEmuProcessor& p);   // the V-Synth's
    ModulationPage(VSynthEmuProcessor& p, const Mode& mode);
    MsmView& msm() { return *msmView_; }
    bool hasMsm() const { return msmView_ != nullptr; }
    MatrixView& matrix() { return *matrixView_; }
    ControlGrid& assigns() { return *assignsGrid_; }
    void resized() override;

private:
    std::unique_ptr<Card> msm_, matrix_, assigns_;
    MsmView* msmView_ = nullptr;
    MatrixView* matrixView_ = nullptr;
    ControlGrid* assignsGrid_ = nullptr;
};
