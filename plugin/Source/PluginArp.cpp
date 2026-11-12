// The arpeggio's plugin side (v190): its ten settings as host parameters, appended after every other so a host's list keeps
// its places; read into XtSynth's controls each block and written back from a patch's; and its pattern, a patch's data,
// kept in the session's state ("arpPattern", which a session saved before v190 lacks: the XT's default then).
#include "PluginArp.h"
#include "PluginProcessor.h"
#include "XtArpeggioPatch.h"
#include <algorithm>
#include <cstring>
#include <span>

namespace {

enum class ArpKind { Toggle, Choice, Number };

// A Number's text and its reading back.  The firmware's text routine's (0x89461a70, research/tools/arptext.cpp), but End
// Point, which the routine gives as 0…31 and the manual as the steps of the pattern, 1…32 [MAN p. 66], shown as the step
struct ArpText {
    juce::String (*show)(int);
    int (*read)(const juce::String&);
};
juce::String plainText(int v) { return juce::String(v); }
int plainValue(const juce::String& t) { return t.getIntValue(); }
juce::String realText(int v) { return v == 0 ? juce::String("REAL") : juce::String(v); }
int realValue(const juce::String& t) { return t.trim().equalsIgnoreCase("REAL") ? 0 : t.getIntValue(); }
juce::String signedText(int v) { return v >= 0 ? "+" + juce::String(v) : juce::String(v); }   // "+0", as the XT
juce::String stepText(int v) { return juce::String(v + 1); }
int stepValue(const juce::String& t) { return t.getIntValue() - 1; }
constexpr ArpText kPlain { plainText, plainValue };

// The ten, by the XT's IDs: names [FW 0x89790338], range and default [FW 0x897a167c, its centre taken off], a Choice's
// values [FW 0x89461a70]
struct ArpParam {
    const char* id;          // the plugin's, never changed
    int fwId;
    const char* name;
    ArpKind kind;
    int lo, hi, def;
    const char* values;      // a Choice's, "|" between
    ArpText text;
};
constexpr ArpParam kArpParams[] = {
    { "arpSwitch", 0x42e, "Arpeggio Switch", ArpKind::Toggle, 0, 1, 0, nullptr, kPlain },
    { "arpHold", 0x42f, "Arpeggio Hold", ArpKind::Toggle, 0, 1, 0, nullptr, kPlain },
    { "arpMotif", 0x430, "Arpeggio Motif", ArpKind::Choice, 0, 7, 0,
      "UP|DOWN|UP&DOWN|RANDOM|NOTE-ORDER|RHYTHM|PHRASE|AUTO", kPlain },
    { "arpDuration", 0x431, "Arpeggio Duration", ArpKind::Number, 0, 100, 100, nullptr, kPlain },
    { "arpShuffleRate", 0x432, "Arpeggio Shuffle Rate", ArpKind::Number, 0, 100, 50, nullptr, kPlain },
    { "arpShuffleRes", 0x433, "Arpeggio Shuffle Resolution", ArpKind::Choice, 0, 1, 0, "16TH|8TH", kPlain },
    { "arpKeyVelocity", 0x434, "Arpeggio Keyboard Velocity", ArpKind::Number, 0, 127, 0, nullptr,
      { realText, realValue } },
    { "arpOctave", 0x435, "Arpeggio Octave Range", ArpKind::Number, -3, 3, 0, nullptr, { signedText, plainValue } },
    { "arpGrid", 0x436, "Arpeggio Grid", ArpKind::Choice, 0, 5, 3, "QUARTER|8TH|8TH-T|16TH|16TH-T|32TH", kPlain },
    { "arpEndPoint", 0x437, "Arpeggio End Point", ArpKind::Number, 0, 31, 31, nullptr, { stepText, stepValue } },
};

// Each one's ID as the audio thread asks for it, made once at load and not each block
const std::array<juce::String, std::size(kArpParams)> kArpIds = [] {
    std::array<juce::String, std::size(kArpParams)> ids;
    std::ranges::transform(kArpParams, ids.begin(), [](const ArpParam& r) { return juce::String(r.id); });
    return ids;
}();

const juce::String& idOf(int fwId) {
    const auto at = std::ranges::find(kArpParams, fwId, &ArpParam::fwId);
    return kArpIds[(size_t)(at - std::begin(kArpParams))];
}

std::unique_ptr<juce::RangedAudioParameter> paramOf(const ArpParam& r) {
    switch (r.kind) {
        case ArpKind::Toggle: return std::make_unique<juce::AudioParameterBool>(r.id, r.name, r.def != 0);
        case ArpKind::Choice: {
            const auto values = juce::StringArray::fromTokens(r.values, "|", "");
            return std::make_unique<juce::AudioParameterChoice>(r.id, r.name, values, r.def);
        }
        case ArpKind::Number: break;
    }
    const ArpText t = r.text;
    const auto text = juce::AudioParameterIntAttributes()
                          .withStringFromValueFunction([t](int v, int) { return t.show(v); })
                          .withValueFromStringFunction([t](const juce::String& s) { return t.read(s); });
    return std::make_unique<juce::AudioParameterInt>(r.id, r.name, r.lo, r.hi, r.def, text);
}

} // namespace

XtArpPattern xtArpDefaultPattern() {
    XtArpPattern p;
    std::fill(std::begin(p.line), std::end(p.line), uint8_t { 60 });   // NOTE, C4 [FW 0x897a167c: ID 0x44b's default]
    return p;
}

bool xtArpPatternSilent(const XtArpPattern& p) {
    const auto cells = std::span(&p.cell[0][0], sizeof p.cell);
    return std::ranges::all_of(cells, [](uint8_t c) { return c <= 1; });   // [FW 0x89302700]: a step plays cells past 1
}

ArpPatternWords::ArpPatternWords() { store(xtArpDefaultPattern()); }

XtArpPattern ArpPatternWords::load() const {
    std::array<uint32_t, kWords> w;
    const auto read = [](const std::atomic<uint32_t>& a) { return a.load(std::memory_order_relaxed); };
    std::ranges::transform(words_, w.begin(), read);
    XtArpPattern p;
    std::memcpy(&p, w.data(), sizeof p);
    return p;
}

void ArpPatternWords::store(const XtArpPattern& p) {
    std::array<uint32_t, kWords> w;
    std::memcpy(w.data(), &p, sizeof p);
    for (size_t i = 0; i < kWords; ++i) words_[i].store(w[i], std::memory_order_relaxed);
}

void VSynthEmuProcessor::addArpParams(ParamList& params) {
    for (const ArpParam& r : kArpParams) params.push_back(paramOf(r));
}

void VSynthEmuProcessor::readArpControls(XtSynth::Controls& c) const {
    c.arp.on = knob(idOf(kXtArpSwitchId)) != 0;
    for (const XtArpSettingId& s : kXtArpSettingIds) c.arp.*s.field = knob(idOf(s.id));
    c.arpPattern = arpWords_.load();
}

void VSynthEmuProcessor::writeArpControls(const XtSynth::Controls& c) {
    set(idOf(kXtArpSwitchId), c.arp.on ? 1.0f : 0.0f);
    for (const XtArpSettingId& s : kXtArpSettingIds) set(idOf(s.id), (float)(c.arp.*s.field));
    setArpPattern(c.arpPattern);
}

void VSynthEmuProcessor::setArpPattern(const XtArpPattern& p) {
    arpWords_.store(p);
    apvts.state.setProperty("arpPattern", juce::Base64::toBase64(&p, sizeof p), nullptr);
}

// A session's pattern, or the XT's default for one saved before there was one
void VSynthEmuProcessor::restoreArpPattern() {
    juce::MemoryOutputStream bytes;
    const juce::String kept = apvts.state.getProperty("arpPattern").toString();
    XtArpPattern p = xtArpDefaultPattern();
    if (kept.isNotEmpty() && juce::Base64::convertFromBase64(bytes, kept) && bytes.getDataSize() == sizeof p)
        std::memcpy(&p, bytes.getData(), sizeof p);
    arpWords_.store(p);
}
