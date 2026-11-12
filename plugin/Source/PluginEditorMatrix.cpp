// The Matrix tab: its page, one Control at a time and the controller assigns, and the attaching of one Control's or one
// assign's parameters to the rows.  Moved as they stood from PluginEditor.cpp (v193), when the MSM's tab came before it.
#include "PluginEditor.h"

using Ui::Page;

void VSynthEmuEditor::buildMatrixPage() {
    // ── Matrix: one Control at a time, and the controller assigns ──
    {
        Page& p = *pages_[7];
        Card m { "Matrix" };
        m.rows = { lr(matrixLabel_, matrixSelBox_), lr(*newLabel("Source"), matrixSourceBox_),
                   lr(*newLabel("Destination 1"), matrixDestBox_[0]), lr(matrixSensLabels_[0], matrixSens_[0]),
                   lr(*newLabel("Destination 2"), matrixDestBox_[1]), lr(matrixSensLabels_[1], matrixSens_[1]) };
        p.add(m);
        Card a { "Controller assign" };
        a.col = 1;
        a.rows = { lr(assignLabel_, assignSelBox_), lr(*newLabel("Function"), assignBox_) };
        p.add(a);
    }
}

// One Matrix Control's knobs on the row, as the XT's Control tabs do
void VSynthEmuEditor::attachMatrix(int m) {
    matrixSourceAtt_.reset();
    for (int j = 0; j < 2; ++j) { matrixDestAtt_[j].reset(); matrixSensAtt_[j].reset(); }
    matrixSourceAtt_ = std::make_unique<ComboAtt>(processor_.apvts, VSynthEmuProcessor::matrixParamId(m, "Source"), matrixSourceBox_);
    matrixDestAtt_[0] = std::make_unique<ComboAtt>(processor_.apvts, VSynthEmuProcessor::matrixParamId(m, "Dest1"), matrixDestBox_[0]);
    matrixDestAtt_[1] = std::make_unique<ComboAtt>(processor_.apvts, VSynthEmuProcessor::matrixParamId(m, "Dest2"), matrixDestBox_[1]);
    matrixSensAtt_[0] = std::make_unique<SliderAtt>(processor_.apvts, VSynthEmuProcessor::matrixParamId(m, "Sens1"), matrixSens_[0]);
    matrixSensAtt_[1] = std::make_unique<SliderAtt>(processor_.apvts, VSynthEmuProcessor::matrixParamId(m, "Sens2"), matrixSens_[1]);
}

void VSynthEmuEditor::attachAssign(int which) {
    assignAtt_.reset();
    assignAtt_ = std::make_unique<ComboAtt>(processor_.apvts, VSynthEmuProcessor::assignParamId(which), assignBox_);
}
