#pragma once
// What the editor's drawing shares (research/UI_DESIGN.md): drawn text, filled and stroked shapes, a key's name; and the
// Effects panel's page of titled cards and its joined buttons, drawn in the palette they are given (FxPanel, on D's FX
// page in Look::cards).  Today's editor's palette and sizes went with it (v196).
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace Ui {
    // A page of cards' sizes: its margin and the gap between cards, a card's title line, a row and the gap under it, the
    // space under a card's last row, and a row's name
    inline constexpr int pad = 12, gap = 12, cardHeadH = 28, rowH = 26, rowGap = 4, cardFoot = 10, labelW = 84;

    // How a piece of drawn text looks: its font and colour, where it sits, and whether it is cut with dots if it doesn't fit
    struct TextStyle {
        juce::Font font;
        juce::Colour colour;
        juce::Justification just = juce::Justification::centredLeft;
        bool dots = true;
    };
    // Text drawn in a style: the cards' titles, D's names and values on what it paints (v195, one place for it)
    inline void drawText(juce::Graphics& g, const juce::String& t, juce::Rectangle<int> r, const TextStyle& s) {
        g.setFont(s.font);
        g.setColour(s.colour);
        g.drawText(t, r, s.just, s.dots);
    }
    // A rectangle filled in a colour: D's marks and bars (v195)
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
    // A key's name, as Roland's: C4 is 60 (the Zones tab's, v195; D's ZONES page, UI step 4)
    inline juce::String keyName(int key) {
        static const char* const kNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return juce::String(kNames[key % 12]) + juce::String(key / 12 - 1);
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
    // The cards' titles: 11 pt bold, spaced out
    inline juce::Font titleFont() {
        juce::Font f(11.0f, juce::Font::bold);
        f.setExtraKerningFactor(0.08f);
        return f;
    }

    // The colours a page of cards is drawn in (D's are Look::cards, UiLook.h)
    struct Palette {
        juce::Colour ground, card, line, label, text, field, accent;
    };

    // ─── A page: cards of rows ────────────────────────────────────────────────────
    // Each card is a titled box in column 0 or 1 (or across both, col -1), its
    // rows a list of items: a component with a fixed width, or 0 for a share of
    // what the fixed ones leave. A card may take another's place (sameAs) and
    // show only while shown() holds.
    class Page : public juce::Component {
    public:
        explicit Page(const Palette& c) : colours(&c) {}
        const Palette* colours;                             // what its cards are drawn in
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
                g.setColour(colours->card);
                g.fillRect(c.bounds);
                g.setColour(colours->line);
                g.drawRect(c.bounds, 1);
                const auto head = c.bounds.reduced(Ui::pad, 0).withHeight(Ui::cardHeadH);
                drawText(g, (c.titleFn ? c.titleFn() : c.title).toUpperCase(), head, { titleFont(), colours->label });
            }
        }
    };

    // ─── Joined toggle buttons, in a palette's accent and text ───────────────────
    class Segmented : public juce::Component {
    public:
        std::function<void(int)> onChange;

        Segmented(const juce::StringArray& names, int group, const Palette& colours) {
            for (int i = 0; i < names.size(); ++i) {
                auto* b = buttons_.add(new juce::TextButton(names[i]));
                b->setClickingTogglesState(true);
                b->setRadioGroupId(group);
                b->setConnectedEdges((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < names.size() - 1 ? juce::Button::ConnectedOnRight : 0));
                b->setColour(juce::TextButton::buttonOnColourId, colours.accent);
                b->setColour(juce::TextButton::textColourOnId, juce::Colours::black);
                b->setColour(juce::TextButton::textColourOffId, colours.text);
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
