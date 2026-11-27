// The WAVE page's events (UiWaveEdit.h, v481; the XT's SAMPLE Encode screen, the user's d-9): Current, the wave's
// events drawn, Current to the one before or after it, and an event added or deleted at Current, which the XT's own
// routines judge in the wave's encode (PluginWaveEdits::editEvent)
#include "UiWaveEdit.h"
#include "UiLook.h"
#include "UiParts.h"
#include <algorithm>

namespace {
constexpr float kMarkW = 8.0f, kMarkH = 7.0f;
}

void WaveEditView::setCurrent(long frame) {
    current_ = std::clamp(frame, 0L, std::max(0L, frames_ - 1));
    repaint();
}

// As the screen's 0x89516a00 and 0x89516a90 go: the first event past Current, else the last; the last before it, else
// the first
void WaveEditView::toEvent(bool after) {
    if (events_.empty()) return;
    long to = after ? events_.back() : events_.front();
    if (after) {
        const auto next = std::upper_bound(events_.begin(), events_.end(), current_);
        if (next != events_.end()) to = *next;
    } else {
        const auto at = std::lower_bound(events_.begin(), events_.end(), current_);
        if (at != events_.begin()) to = *(at - 1);
    }
    setCurrent(to);
}

void WaveEditView::addEvent() {
    PluginWaveEdits::editEvent(processor_, osc_, { true, (uint32_t)current_ });
}

void WaveEditView::deleteEvent() {
    PluginWaveEdits::editEvent(processor_, osc_, { false, (uint32_t)current_ });
}

// Each event a mark at the wave's top, as the XT's ▼; Current a thin line across the wave
void WaveEditView::paintEvents(juce::Graphics& g) const {
    const auto w = wave();
    for (const long e : events_) {
        const float x = xOf(e);
        if (x < w.getX() - kMarkW || x > w.getRight() + kMarkW) continue;
        juce::Path mark;
        mark.addTriangle(x - kMarkW / 2.0f, w.getY(), x + kMarkW / 2.0f, w.getY(), x, w.getY() + kMarkH);
        Ui::fillPath(g, mark, Look::text);
    }
    const float x = xOf(at(Point::current));
    Ui::fill(g, juce::Rectangle<float>(x, w.getY(), 1.0f, w.getHeight()).toNearestInt(), Look::label);
}
