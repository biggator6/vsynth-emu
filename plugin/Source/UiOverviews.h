#pragma once
// ENVELOPES and LFOs (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): every envelope of every block
// on the path, and every LFO, at once, each a tile of its name over its curve or its shape; a click on one opens its
// block in SELECTED, at that envelope.  Both are an Overview, made from the kinds' table, block by block.
#include "UiBlockKinds.h"
#include <functional>
#include <memory>
#include <vector>

class Overview : public juce::Component {
public:
    std::function<void(int block, int env)> onPick;   // a block's index in Blocks::kPath, and its envelope's
    int tiles() const { return (int)tiles_.size(); }
    juce::Component& drawing(int tile) { return *tiles_[(size_t)tile].drawing; }
    int blockOf(int tile) const { return tiles_[(size_t)tile].block; }
    int envOf(int tile) const { return tiles_[(size_t)tile].env; }
    void resized() override;

protected:
    explicit Overview(int perRow) : perRow_(perRow) {}
    // A tile: its drawing, which calls back when clicked, and its name
    void add(std::unique_ptr<juce::Component> drawing, std::function<void()>& clicked, const juce::String& name,
             int block, int env);

private:
    struct Tile {
        std::unique_ptr<juce::Component> drawing;
        std::unique_ptr<juce::Label> name;
        int block, env;
    };
    std::vector<Tile> tiles_;
    int perRow_;
};

class EnvelopesOverview : public Overview {
public:
    explicit EnvelopesOverview(Controls::State& state);
};

class LfosOverview : public Overview {
public:
    explicit LfosOverview(Controls::State& state);
};
