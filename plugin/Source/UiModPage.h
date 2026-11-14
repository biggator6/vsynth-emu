#pragma once
// D's MODULATION page (research/UI_DESIGN.md, "The pages"; UI step 4): the Multi Step Modulator's card down the left,
// the Matrix's card and the controller assigns' card down the right.
#include "PluginProcessor.h"
#include "UiCard.h"
#include "UiControlGrid.h"
#include "UiMatrix.h"
#include "UiMsm.h"
#include <memory>

class ModulationPage : public juce::Component {
public:
    explicit ModulationPage(VSynthEmuProcessor& p);
    MsmView& msm() { return *msmView_; }
    MatrixView& matrix() { return *matrixView_; }
    ControlGrid& assigns() { return *assignsGrid_; }
    void resized() override;

private:
    std::unique_ptr<Card> msm_, matrix_, assigns_;
    MsmView* msmView_ = nullptr;
    MatrixView* matrixView_ = nullptr;
    ControlGrid* assignsGrid_ = nullptr;
};
