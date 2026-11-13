#pragma once
// The editor's tab bar: the row of tab names, the open one marked (moved from PluginEditor.cpp as it stood, v195, when the
// Zones tab came; VSynthEmuEditor::makeTabBar and showTab, PluginEditorTabs.cpp, make and turn it).
#include "PluginEditor.h"

class VSynthEmuEditor::TabBar : public juce::Component {
public:
    static constexpr int kMinPadding = 8, kMaxPadding = 28;   // around a tab's name
    juce::StringArray names;
    int current = 0;
    std::function<void(int)> onChange;

    // Each tab its name's width and an equal share of what's left, 28 at most, so every tab ends inside the bar: ten tabs
    // (v195) at 28 each ran Effects past the end at the smallest width
    juce::Rectangle<int> tabBounds(int i) const {
        const juce::Font font(13.0f, juce::Font::bold);
        int text = 0;
        for (const auto& n : names) text += font.getStringWidth(n);
        const int share = (getWidth() - 2 * Ui::pad - text) / juce::jmax(1, names.size());
        const int padding = juce::jlimit(kMinPadding, kMaxPadding, share);
        int x = Ui::pad;
        for (int k = 0; k < names.size(); ++k) {
            const int w = font.getStringWidth(names[k]) + padding;
            if (k == i) return { x, 0, w, getHeight() };
            x += w;
        }
        return {};
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(Ui::surfaceStrip);
        g.setColour(Ui::line);
        g.fillRect(0, getHeight() - 1, getWidth(), 1);
        for (int i = 0; i < names.size(); ++i) {
            const auto b = tabBounds(i);
            const bool on = i == current;
            const juce::Font font(13.0f, on ? juce::Font::bold : juce::Font::plain);
            Ui::drawText(g, names[i], b, { font, on ? Ui::text : Ui::label, juce::Justification::centred, false });
            if (on) Ui::fill(g, { b.getX(), getHeight() - 2, b.getWidth(), 2 }, Ui::accent);
        }
    }
    void mouseDown(const juce::MouseEvent& e) override {
        for (int i = 0; i < names.size(); ++i)
            if (tabBounds(i).contains(e.getPosition())) { if (onChange) onChange(i); return; }
    }
};
