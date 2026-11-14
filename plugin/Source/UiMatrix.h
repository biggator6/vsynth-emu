#pragma once
// The Matrix on the MODULATION page (research/UI_DESIGN.md, "The pages"; UI step 4): its eight Controls as rows under the
// columns' names, a row made for each Control by index (UiMatrixIds.h): its number, its Source, and its two destinations
// each with its Sens.
#include "UiFields.h"
#include "UiMatrixIds.h"
#include <array>
#include <memory>
#include <vector>

class MatrixRow : public juce::Component {
public:
    MatrixRow(Controls::State& state, int control);
    int made() const { return made_; }   // 5 with every ID found
    static constexpr int kH = 26;
    void resized() override;

private:
    Controls::Attached attached_;
    juce::Label number_;
    juce::ComboBox source_;
    std::array<juce::ComboBox, Matrix::kDests> dests_;
    std::array<std::unique_ptr<Bar>, Matrix::kDests> sens_;
    int made_ = 0;
};

class MatrixView : public juce::Component {
public:
    explicit MatrixView(Controls::State& state);
    MatrixRow& row(int control) { return *rows_[(size_t)control]; }
    static int height();   // the columns' names and the eight rows
    void resized() override;

private:
    std::vector<std::unique_ptr<juce::Label>> heads_;
    std::vector<std::unique_ptr<MatrixRow>> rows_;
};
