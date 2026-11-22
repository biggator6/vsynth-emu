// D's MODULATION page (UiModPage.h; UI step 4; a mode's, v453)
#include "UiModPage.h"
#include "UiLook.h"
#include "UiPages.h"

namespace {
constexpr int kMsmW = 640;   // the Multi Step Modulator's column (the VC-2's Matrix's); the Matrix's takes the rest

ModulationPage::Mode vsynthModulation() {
    ModulationPage::Mode m;
    for (int i = 0; i < VSynthEmuProcessor::kAssigns; ++i)
        m.assigns.push_back({ VSynthEmuProcessor::assignParamId(i), VSynthEmuProcessor::assignName(i) });
    return m;
}
}

ModulationPage::ModulationPage(VSynthEmuProcessor& p) : ModulationPage(p, vsynthModulation()) {}

ModulationPage::ModulationPage(VSynthEmuProcessor& p, const Mode& mode) {
    if (mode.msm) {
        msm_ = std::make_unique<Card>("MULTI STEP MODULATOR");
        msmView_ = &msm_->setBody(std::make_unique<MsmView>(p.apvts));
        addAndMakeVisible(*msm_);
    }
    matrix_ = std::make_unique<Card>("MATRIX");
    matrixView_ = &matrix_->setBody(std::make_unique<MatrixView>(p.apvts, mode.matrix));
    assigns_ = Pages::gridCard(p.apvts, "CONTROLLER ASSIGNS", mode.assigns);
    assignsGrid_ = dynamic_cast<ControlGrid*>(assigns_->body());
    addAndMakeVisible(*matrix_);
    addAndMakeVisible(*assigns_);
}

// The MSM down the left, the Matrix and the assigns down the right; with no MSM the Matrix down the left, as high as its
// rows, and the assigns at the right
void ModulationPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    auto left = r.removeFromLeft(kMsmW);
    r.removeFromLeft(Look::gap);
    if (msm_) {
        msm_->setBounds(left);
        matrix_->setBounds(r.removeFromTop(matrix_->heightFor(MatrixView::height())));
        r.removeFromTop(Look::gap);
    } else {
        matrix_->setBounds(left.removeFromTop(matrix_->heightFor(MatrixView::height())));
    }
    Pages::stack({ assigns_.get() }, r, false);
}
