// D's MODULATION page (UiModPage.h; UI step 4)
#include "UiModPage.h"
#include "UiLook.h"
#include "UiPages.h"

namespace {
constexpr int kMsmW = 640;   // the Multi Step Modulator's column; the Matrix's takes the rest
}

ModulationPage::ModulationPage(VSynthEmuProcessor& p) {
    msm_ = std::make_unique<Card>("MULTI STEP MODULATOR");
    msmView_ = &msm_->setBody(std::make_unique<MsmView>(p.apvts));
    matrix_ = std::make_unique<Card>("MATRIX");
    matrixView_ = &matrix_->setBody(std::make_unique<MatrixView>(p.apvts));
    ControlGrid::Rows rows;
    for (int i = 0; i < VSynthEmuProcessor::kAssigns; ++i)
        rows.push_back({ VSynthEmuProcessor::assignParamId(i), VSynthEmuProcessor::assignName(i) });
    assigns_ = Pages::gridCard(p.apvts, "CONTROLLER ASSIGNS", rows);
    assignsGrid_ = dynamic_cast<ControlGrid*>(assigns_->body());
    addAndMakeVisible(*msm_);
    addAndMakeVisible(*matrix_);
    addAndMakeVisible(*assigns_);
}

void ModulationPage::resized() {
    auto r = getLocalBounds().reduced(Look::inset);
    msm_->setBounds(r.removeFromLeft(kMsmW));
    r.removeFromLeft(Look::gap);
    matrix_->setBounds(r.removeFromTop(matrix_->heightFor(MatrixView::height())));
    r.removeFromTop(Look::gap);
    Pages::stack({ assigns_.get() }, r, false);
}
