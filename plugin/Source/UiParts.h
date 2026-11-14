#pragma once
// The editor's design (research/UI_DESIGN.md): its palette and sizes, a page of titled cards, joined buttons. PluginEditor and FxPanel share them.
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace Ui {
    inline const juce::Colour surfaceApp      { 0xFF1A1A2E };   // window
    inline const juce::Colour surfaceTitlebar { 0xFF0D0D20 };   // header
    inline const juce::Colour surfaceStrip    { 0xFF111128 };   // source bar, tab bar
    inline const juce::Colour surfaceXt       { 0xFF15152E };   // cards, the perform column
    inline const juce::Colour surfaceField    { 0xFF0D0D1A };   // value boxes, envelope views
    inline const juce::Colour line            { 0xFF333355 };   // dividers, card borders, knob outline
    inline const juce::Colour accent          { 0xFF00BFA5 };   // the one interactive colour
    inline const juce::Colour title           { 0xFF00FF88 };   // the wordmark
    inline const juce::Colour text            { 0xFFD3D3D3 };   // values (lightgrey)
    inline const juce::Colour label           { 0xFFADD8E6 };   // names (lightblue)

    inline constexpr int headerH = 40, sourceH = 56, performW = 248, tabH = 38;
    inline constexpr int pad = 12, gap = 12, cardHeadH = 28, rowH = 26, rowGap = 4, cardFoot = 10, labelW = 84;

    // A value slider on a card: its value box `boxW` wide, in the palette (v190: the editor's constructor's, and the
    // Arpeggio card's)
    inline void styleValueSlider(juce::Slider& s, int boxW) {
        s.setColour(juce::Slider::textBoxTextColourId, text);
        s.setColour(juce::Slider::textBoxBackgroundColourId, surfaceField);
        s.setColour(juce::Slider::trackColourId, accent);
        s.setTextBoxStyle(s.getTextBoxPosition(), false, boxW, 20);
    }

    // How a piece of drawn text looks: its font and colour, where it sits, and whether it is cut with dots if it doesn't fit
    struct TextStyle {
        juce::Font font;
        juce::Colour colour;
        juce::Justification just = juce::Justification::centredLeft;
        bool dots = true;
    };
    // Text drawn in a style: the set-up the cards' and the Perform column's titles, the tabs and the Zones tab's map share
    // (v195, one place for it)
    inline void drawText(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, const TextStyle& s) {
        g.setFont(s.font);
        g.setColour(s.colour);
        g.drawText(t, r, s.just, s.dots);
    }
    // A rectangle filled in a colour: the tab bar's mark, the Zones map's keys (v195)
    inline void fill(juce::Graphics& g, juce::Rectangle<int> r, juce::Colour c) {
        g.setColour(c);
        g.fillRect(r);
    }
    // A rounded rectangle filled in a colour: D's boxes, chips, switches and keys (UI step 1)
    inline void fillRounded(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float radius) {
        g.setColour(c);
        g.fillRoundedRectangle(r, radius);
    }
    // A path stroked in a colour: D's knobs' arcs, a box's chevron, a waveform (UI step 2)
    inline void stroke(juce::Graphics& g, const juce::Path& p, juce::Colour c, const juce::PathStrokeType& type) {
        g.setColour(c);
        g.strokePath(p, type);
    }
    // A round dot of a width at a centre, in a colour: D's knob caps, switches, points to drag (UI step 3)
    inline void dot(juce::Graphics& g, juce::Point<float> centre, float width, juce::Colour c) {
        g.setColour(c);
        g.fillEllipse(juce::Rectangle<float>(width, width).withCentre(centre));
    }
    // Straight lines through points in their order: D's envelope curves (UI step 3)
    inline juce::Path polyline(const std::vector<juce::Point<float>>& points) {
        juce::Path p;
        for (size_t i = 0; i < points.size(); ++i) {
            if (i == 0) p.startNewSubPath(points[i]);
            else p.lineTo(points[i]);
        }
        return p;
    }
    // The cards' and the Perform column's titles: 11 pt bold, spaced out
    inline juce::Font titleFont() {
        juce::Font f(11.0f, juce::Font::bold);
        f.setExtraKerningFactor(0.08f);
        return f;
    }

    // ─── A tab's page: cards of rows ──────────────────────────────────────────────
    // Each card is a titled box in column 0 or 1 (or across both, col -1), its
    // rows a list of items: a component with a fixed width, or 0 for a share of
    // what the fixed ones leave. A card may take another's place (sameAs) and
    // show only while shown() holds (the analog oscillator's in the PCM ones').
    class Page : public juce::Component {
    public:
        struct Item { juce::Component* c; int w; };
        struct Row  { std::vector<Item> items; };
        struct Card {
            Card() = default;
            explicit Card(juce::String t) : title(std::move(t)) {}
            // A card whole: its title, the title line's controls and the rows, in column `column` (v193)
            Card(juce::String t, std::vector<Item> h, std::vector<Row> r, int column = 0)
                : title(std::move(t)), head(std::move(h)), rows(std::move(r)), col(column) {}
            juce::String title;
            std::function<juce::String()> titleFn;          // a title that follows the selectors
            std::vector<Item> head;                         // controls on the title line, right-aligned
            std::vector<Row> rows;
            int col = 0;                                    // 0, 1, or -1 across both
            int sameAs = -1;                                // the index of the card whose place this takes
            bool bare = false;                              // rows only: no box, no title
            bool fill = false;                              // down to the page's bottom, for a custom() that scrolls
            int customH = 0;                                // a body laid out by custom() instead of rows
            std::function<void(juce::Rectangle<int>)> custom;
            std::vector<juce::Component*> extra;            // what custom() places
            std::function<bool()> shown;
            juce::Rectangle<int> bounds;
        };
        std::vector<Card> cards;
        int leftW = 0;                                      // column 0's width; 0: half

        Card& add(Card c) { cards.push_back(std::move(c)); return cards.back(); }

        // Take each card's components from wherever they are, visibility kept
        void adopt() {
            for (auto& c : cards) {
                for (auto& i : c.head) if (i.c) addChildComponent(i.c);
                for (auto& r : c.rows) for (auto& i : r.items) if (i.c) addChildComponent(i.c);
                for (auto* e : c.extra) addChildComponent(e);
            }
        }

        static int heightOf(const Card& c) {
            const int body = c.custom ? c.customH : (int)c.rows.size() * (Ui::rowH + Ui::rowGap) - Ui::rowGap;
            return c.bare ? body : Ui::cardHeadH + body + Ui::cardFoot;
        }

        static void layoutRow(juce::Rectangle<int> r, const std::vector<Item>& items) {
            int fixed = 0, shares = 0;
            for (auto& i : items) { if (i.w > 0) fixed += i.w; else ++shares; }
            const int gaps = 8 * juce::jmax(0, (int)items.size() - 1);
            const int share = shares ? juce::jmax(0, (r.getWidth() - fixed - gaps) / shares) : 0;
            for (auto& i : items) {
                auto cell = r.removeFromLeft(i.w > 0 ? i.w : share);
                r.removeFromLeft(8);
                if (i.c) i.c->setBounds(cell);
            }
        }

        void resized() override {
            auto area = getLocalBounds().reduced(Ui::pad);
            const int colW = leftW > 0 ? leftW : (area.getWidth() - Ui::gap) / 2;
            int y[2] = { area.getY(), area.getY() };
            for (auto& c : cards) {
                const int h = c.fill ? juce::jmax(0, area.getBottom() - y[juce::jmax(0, c.col)]) : heightOf(c);
                juce::Rectangle<int> b;
                if (c.sameAs >= 0) {
                    const auto& o = cards[(size_t)c.sameAs].bounds;
                    b = { o.getX(), o.getY(), o.getWidth(), h };
                    if (c.col >= 0) y[c.col] = juce::jmax(y[c.col], b.getBottom() + Ui::gap);
                } else if (c.col < 0) {
                    const int top = juce::jmax(y[0], y[1]);
                    b = { area.getX(), top, area.getWidth(), h };
                    y[0] = y[1] = b.getBottom() + Ui::gap;
                } else {
                    b = { area.getX() + c.col * (colW + Ui::gap), y[c.col], c.col ? area.getRight() - (area.getX() + colW + Ui::gap) : colW, h };
                    y[c.col] = b.getBottom() + Ui::gap;
                }
                c.bounds = b;
                auto inner = c.bare ? b : b.reduced(Ui::pad, 0);
                if (!c.bare) {
                    auto head = inner.removeFromTop(Ui::cardHeadH).withTrimmedTop(3).withTrimmedBottom(1);
                    for (auto it = c.head.rbegin(); it != c.head.rend(); ++it) {
                        if (it->c) it->c->setBounds(head.removeFromRight(it->w));
                        head.removeFromRight(8);
                    }
                }
                if (c.custom) { c.custom(c.fill ? inner.withTrimmedBottom(Ui::cardFoot) : inner.removeFromTop(c.customH)); continue; }
                for (auto& r : c.rows) {
                    layoutRow(inner.removeFromTop(Ui::rowH), r.items);
                    inner.removeFromTop(Ui::rowGap);
                }
            }
        }

        void paint(juce::Graphics& g) override {
            for (auto& c : cards) {
                if (c.bare || (c.shown && !c.shown())) continue;
                g.setColour(Ui::surfaceXt);
                g.fillRect(c.bounds);
                g.setColour(Ui::line);
                g.drawRect(c.bounds, 1);
                const auto head = c.bounds.reduced(Ui::pad, 0).withHeight(Ui::cardHeadH);
                drawText(g, (c.titleFn ? c.titleFn() : c.title).toUpperCase(), head, { titleFont(), Ui::label });
            }
        }
    };

    // ─── Joined toggle buttons ───────────────────────────────────────────────────
    class Segmented : public juce::Component {
    public:
        std::function<void(int)> onChange;

        Segmented(const juce::StringArray& names, int group) {
            for (int i = 0; i < names.size(); ++i) {
                auto* b = buttons_.add(new juce::TextButton(names[i]));
                b->setClickingTogglesState(true);
                b->setRadioGroupId(group);
                b->setConnectedEdges((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < names.size() - 1 ? juce::Button::ConnectedOnRight : 0));
                b->setColour(juce::TextButton::buttonOnColourId, Ui::accent);
                b->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
                b->setColour(juce::TextButton::textColourOffId, Ui::text);
                b->onClick = [this, i] { if (buttons_[i]->getToggleState() && onChange) onChange(i); };
                addAndMakeVisible(b);
            }
            setIndex(0);
        }
        void setIndex(int i) {
            if (juce::isPositiveAndBelow(i, buttons_.size()))
                buttons_[i]->setToggleState(true, juce::dontSendNotification);
        }
        void resized() override {
            auto r = getLocalBounds();
            const int w = r.getWidth() / juce::jmax(1, buttons_.size());
            for (int i = 0; i < buttons_.size(); ++i)
                buttons_[i]->setBounds(i == buttons_.size() - 1 ? r : r.removeFromLeft(w));
        }
    private:
        juce::OwnedArray<juce::TextButton> buttons_;
    };
}
