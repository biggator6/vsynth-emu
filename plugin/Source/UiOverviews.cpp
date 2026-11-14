// ENVELOPES and LFOs (UiOverviews.h; UI step 3)
#include "UiOverviews.h"
#include "UiEnvelope.h"
#include "UiLfo.h"
#include "UiLook.h"

namespace {
constexpr int kGap = 8, kNameH = 16, kMaxDrawingH = 110;
constexpr int kEnvelopesPerRow = 7, kLfosPerRow = 5;
}

void Overview::add(std::unique_ptr<juce::Component> drawing, std::function<void()>& clicked, const juce::String& name,
                   int block, int env) {
    clicked = [this, block, env] {
        if (onPick) onPick(block, env);
    };
    auto label = std::make_unique<juce::Label>();
    Look::fieldName(*label, name);
    addAndMakeVisible(*drawing);
    addAndMakeVisible(*label);
    tiles_.push_back({ std::move(drawing), std::move(label), block, env });
}

// The tiles across in rows of perRow, each its name over its drawing
void Overview::resized() {
    const auto r = getLocalBounds();
    const int rows = ((int)tiles_.size() + perRow_ - 1) / juce::jmax(1, perRow_);
    const int w = (r.getWidth() - (perRow_ - 1) * kGap) / perRow_;
    const int h = juce::jmin(kNameH + kMaxDrawingH, (r.getHeight() - (rows - 1) * kGap) / juce::jmax(1, rows));
    for (size_t i = 0; i < tiles_.size(); ++i) {
        const int col = (int)i % perRow_, row = (int)i / perRow_;
        auto tile = juce::Rectangle<int>(r.getX() + col * (w + kGap), r.getY() + row * (h + kGap), w, h);
        tiles_[i].name->setBounds(tile.removeFromTop(kNameH));
        tiles_[i].drawing->setBounds(tile);
    }
}

EnvelopesOverview::EnvelopesOverview(Controls::State& state)
    : Overview(kEnvelopesPerRow) {
    for (int block = 0; block < Blocks::kCount; ++block) {
        const Blocks::Block& b = Blocks::kPath[(size_t)block];
        int env = 0;
        for (const Blocks::EnvName& e : Blocks::kindOf(b).envs) {
            if (e.name == nullptr) continue;
            auto curve = std::make_unique<EnvCurve>(state, b, e, false);
            auto& clicked = curve->onClick;
            add(std::move(curve), clicked, Blocks::blockName(b) + " " + e.name, block, env++);
        }
    }
}

LfosOverview::LfosOverview(Controls::State& state)
    : Overview(kLfosPerRow) {
    for (int block = 0; block < Blocks::kCount; ++block) {
        const Blocks::Block& b = Blocks::kPath[(size_t)block];
        if (Blocks::kindOf(b).lfo == nullptr) continue;
        auto shape = std::make_unique<LfoShape>(state, b);
        auto& clicked = shape->onClick;
        add(std::move(shape), clicked, Blocks::lfoNameOf(b) + "  " + Blocks::blockName(b), block, 0);
    }
}
