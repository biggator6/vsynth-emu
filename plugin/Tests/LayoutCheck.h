#pragma once
// TEST_UI_LAYOUT's editor and its checks (LayoutTest.cpp): what one state of the editor must be, and every visible
// control moved to see which parameter it sets.  Moved from PluginTest.cpp's main (v193), where they were its lambdas.
#include "../Source/FxPanel.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <functional>
#include <memory>
#include <set>
#include <vector>

inline bool isControl(juce::Component* c) {
    return dynamic_cast<juce::Label*>(c) || dynamic_cast<juce::Slider*>(c) || dynamic_cast<juce::ComboBox*>(c)
        || dynamic_cast<juce::Button*>(c);
}

// A control as a failure says it
inline juce::String controlName(juce::Component* c) {
    if (auto* l = dynamic_cast<juce::Label*>(c)) return "label \"" + l->getText() + "\"";
    if (auto* b = dynamic_cast<juce::Button*>(c)) return "button \"" + b->getButtonText() + "\"";
    if (auto* x = dynamic_cast<juce::ComboBox*>(c)) return "box \"" + x->getText() + "\"";
    if (dynamic_cast<juce::Slider*>(c)) return "slider at " + c->getBounds().toString();
    return "component at " + c->getBounds().toString();
}

// JUCE squeezes a text to its minimum scale, then cuts it
inline bool cutShort(juce::Label& l) {
    const float scale = l.getMinimumHorizontalScale() > 0 ? l.getMinimumHorizontalScale() : 0.7f;
    return juce::GlyphArrangement::getStringWidth(l.getFont(), l.getText()) * scale > (float)(l.getWidth() - l.getBorderSize().getLeftAndRight());
}

inline juce::Label* boxLabelOf(juce::Component& c) {
    juce::Label* box = nullptr;
    for (juce::Component* ch : c.getChildren())
        if (auto* l = dynamic_cast<juce::Label*>(ch)) box = l;
    return box;
}

// The editor under test, what it has found and counted, and the checks and moves; the sweeps are LayoutTest.cpp's
struct LayoutCheck {
    VSynthEmuProcessor& p;
    std::unique_ptr<juce::AudioProcessorEditor> ed;
    VSynthEmuEditor* editor = nullptr;
    FxPanel* panel = nullptr;
    juce::Viewport* view = nullptr;
    int errors = 0, states = 0, checked = 0, cosmTypes = 0;
    std::vector<juce::RangedAudioParameter*> params;
    std::set<juce::String> reached;

    explicit LayoutCheck(VSynthEmuProcessor& proc) : p(proc), ed(proc.createEditor()) {
        editor = dynamic_cast<VSynthEmuEditor*>(ed.get());
        for (juce::Component* ch : ed->getChildren()) if (auto* fp = dynamic_cast<FxPanel*>(ch)) panel = fp;
        if (panel) for (juce::Component* ch : panel->getChildren()) if (auto* v = dynamic_cast<juce::Viewport*>(ch)) view = v;
        for (auto* prm : p.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) params.push_back(r);
        cosmTypes = p.apvts.getParameter("cosmType")->getNumSteps();
    }

    void fail(const juce::String& m) {
        if (++errors <= 40) std::fprintf(stderr, "layout: %s\n", m.toRawUTF8());
    }

    // Each visible parent under c: not into a control's own parts, nor the effects' rows, which TEST_FX_PANEL goes through
    void walk(juce::Component& c, const std::function<void(juce::Component&)>& visit) {
        visit(c);
        for (juce::Component* ch : c.getChildren())
            if (ch->isVisible() && !isControl(ch) && !dynamic_cast<juce::Viewport*>(ch)) walk(*ch, visit);
    }

    juce::Component* find(juce::Component& c, const std::function<bool(juce::Component*)>& is) {
        for (juce::Component* ch : c.getChildren()) {
            if (is(ch)) return ch;
            if (juce::Component* in = find(*ch, is)) return in;
        }
        return nullptr;
    }

    // The first T anywhere under the editor that `is`
    template <class T>
    T* firstOf(const std::function<bool(T&)>& is) {
        const auto match = [&](juce::Component* c) {
            auto* t = dynamic_cast<T*>(c);
            return t && is(*t);
        };
        return dynamic_cast<T*>(find(*ed, match));
    }
    juce::ComboBox* boxOf(const juce::String& item) {
        return firstOf<juce::ComboBox>([&](juce::ComboBox& x) { return x.getItemText(0) == item; });
    }
    juce::Button* buttonOf(const juce::String& text) {
        return firstOf<juce::Button>([&](juce::Button& b) { return b.getButtonText() == text; });
    }

    // The effects' list: its names and values at their defaults, on each type
    void checkList(const juce::String& where) {
        for (juce::Component* ch : view->getViewedComponent()->getChildren()) {
            if (!ch->isVisible()) continue;
            if (auto* l = dynamic_cast<juce::Label*>(ch)) {
                if (cutShort(*l)) fail(where + ": " + controlName(ch) + " is cut short");
            } else if (juce::Label* box = boxLabelOf(*ch); box && cutShort(*box)) {
                fail(where + ": the value " + controlName(box) + " is cut short");
            }
        }
    }

    // a card that isn't shown takes its controls with it, the title line's too, unless a card that is holds them
    void checkCards(juce::Component& c, const juce::String& where) {
        auto* pg = dynamic_cast<Ui::Page*>(&c);
        if (!pg) return;
        std::set<juce::Component*> held, hidden;
        for (const auto& card : pg->cards) {
            auto& into = card.shown && !card.shown() ? hidden : held;
            for (const auto& h : card.head) if (h.c) into.insert(h.c);
            for (const auto& r : card.rows) for (const auto& it : r.items) if (it.c) into.insert(it.c);
            for (juce::Component* x : card.extra) if (x) into.insert(x);
        }
        for (juce::Component* k : hidden)
            if (!held.count(k) && k->isVisible())
                fail(where + ": " + controlName(k) + " shows on a card that isn't shown");
    }

    // Whether a control's text is cut short: a label's, a box's or a slider's value (v190), a toggle's as LookAndFeel_V4
    // draws it, the tick then the text
    static bool textCut(juce::Component* k) {
        if (auto* l = dynamic_cast<juce::Label*>(k)) return cutShort(*l);
        if (dynamic_cast<juce::ComboBox*>(k) || dynamic_cast<juce::Slider*>(k)) {
            juce::Label* box = boxLabelOf(*k);
            return box && cutShort(*box);
        }
        auto* b = dynamic_cast<juce::ToggleButton*>(k);
        if (!b) return false;
        const float size = juce::jmin(15.0f, (float)b->getHeight() * 0.75f);
        const juce::Font font { juce::FontOptions(size) };
        const float text = juce::GlyphArrangement::getStringWidth(font, b->getButtonText());
        return text * 0.7f > (float)(b->getWidth() - juce::roundToInt(size * 1.1f) - 12);
    }

    void checkKids(juce::Component& c, const juce::String& where) {
        std::vector<juce::Component*> kids;
        for (juce::Component* ch : c.getChildren()) if (ch->isVisible() && !dynamic_cast<juce::ResizableCornerComponent*>(ch)) kids.push_back(ch);
        for (size_t i = 0; i < kids.size(); ++i) {
            juce::Component* k = kids[i];
            const juce::String at = where + ": " + controlName(k);
            ++checked;
            if (isControl(k) && k->getBounds().isEmpty()) fail(at + " has no size");
            if (!c.getLocalBounds().contains(k->getBounds())) fail(at + " is outside its parent");
            for (size_t j = i + 1; j < kids.size(); ++j)
                if (k->getBounds().intersects(kids[j]->getBounds())) fail(at + " is over " + controlName(kids[j]));
            if (textCut(k)) fail(at + " is cut short");
        }
    }

    void check(const juce::String& where) {
        ++states;
        walk(*ed, [&](juce::Component& c) {
            checkCards(c, where);
            checkKids(c, where);
        });
    }

    // Every control moved, and the parameters it changes put back
    juce::RangedAudioParameter* changedBy(const std::function<void()>& move) {
        std::vector<float> before;
        for (auto* r : params) before.push_back(r->getValue());
        move();
        juce::RangedAudioParameter* changed = nullptr;
        for (size_t i = 0; i < params.size(); ++i)
            if (params[i]->getValue() != before[i]) { changed = params[i]; reached.insert(params[i]->paramID); params[i]->setValueNotifyingHost(before[i]); }
        return changed;
    }

    // How a control is moved: none for what chooses what the others show, or isn't a parameter
    static std::function<void()> moverOf(juce::Component* k) {
        if (auto* s = dynamic_cast<juce::Slider*>(k)) {
            const double to = s->getValue() == s->getMaximum() ? s->getMinimum() : s->getMaximum();
            return [s, to] { s->setValue(to, juce::sendNotificationSync); };
        }
        if (auto* x = dynamic_cast<juce::ComboBox*>(k)) {
            const juce::String first = x->getItemText(0);
            if (first == "OSC 1" || first == "COSM 1" || first == "Control 1" || first == "Track 1"
                || first == VSynthEmuProcessor::assignName(0) || first == "Passthrough (baseline)") return {};
            return [x] { x->setSelectedItemIndex(x->getSelectedItemIndex() == 0 ? 1 : 0, juce::sendNotificationSync); };
        }
        auto* b = dynamic_cast<juce::Button*>(k);
        if (!b || (dynamic_cast<juce::TextButton*>(b) && !b->getClickingTogglesState())) return {};   // Load, ROM
        const juce::String text = b->getButtonText();
        if (text.startsWith("OSC ") || text.startsWith("COSM ")) return {};   // as the boxes above
        if (text == "MFX" || text == "Chorus" || text == "Reverb" || text == "System") return {};
        if (b->getRadioGroupId() != 0 && b->getToggleState()) return {};   // one of a pair: the other moves it
        return [b] { b->setToggleState(!b->getToggleState(), juce::sendNotificationSync); };
    }

    void moveAll(const juce::String& where) {
        std::vector<juce::Component*> controls;
        walk(*ed, [&](juce::Component& c) { for (juce::Component* ch : c.getChildren()) if (ch->isVisible() && isControl(ch) && !dynamic_cast<juce::Label*>(ch)) controls.push_back(ch); });
        for (juce::Component* k : controls) {
            const std::function<void()> move = moverOf(k);
            if (move && !changedBy(move)) fail(where + ": " + controlName(k) + " moves no parameter");
        }
    }

    // LayoutTest.cpp: the sweeps
    void msmTracks();
    void moveEverywhere();
    int coverage(const char* ui);
    void chrome();
    void effectsAt(const juce::String& at);
    void atSizes();
    void tabClicks();
    void joined();
};
