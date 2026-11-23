// The VC-2's controls by rom.4's value strings (UiVc2Text.h; v453)
#include "UiVc2Text.h"
#include "PluginVc2Params.h"
#include "UiParts.h"
#include "Vc2Params.h"
#include "XtFxValueText.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace Vc2Text {

namespace {

constexpr char kNames = 1, kRange = 2, kUnit = 3;   // [FW] the string's marks (XtFxValueText.h)
constexpr int kMostBoxed = 8;                       // d-8: a range beside a name in this many values or fewer, a box
constexpr int kCosmType = 0x463;
// [FW] the patch EQ's Freq and Q, both bands, and rom.2's EQ Mid 1 Freq and Q: the mid bands rom.4's driver reads the
// patch EQ into (VC2_VOCAL_DESIGNER.md, "Program A's routing"), whose routine formats them where rom.4's has no format
constexpr int kEqFreq[] = { 0x1af, 0x1b2 }, kEqQ[] = { 0x1b0, 0x1b3 };
constexpr int kVSynthEqFreq = 0x67, kVSynthEqQ = 0x68;
// COSM Type's values by the manual's short names, rom.4's order: the V-Synth's types with GROWL in COMB's place (d-9)
const char* const kCosmTypes[] = { "THRU",  "OD/DS", "W-SHAPE", "AMP",     "SPEAKER", "RESONATOR",
                                   "SBF1",  "SBF2",  "GROWL",   "DUAL",    "TVF",     "DYN-TVF",
                                   "COMP",  "LIMITER", "F-SHIFT", "LO-FI", "TB-FILTER" };

// A piece of a string: a name, or a range's two ends
struct Piece {
    juce::String a, b;
    bool range = false;
};
// A string as its pieces in order and its unit (its brackets gone)
struct Pieces {
    std::vector<Piece> pieces;
    juce::String unit;
    int ranges = 0, names = 0;
};

Pieces piecesOf(int id) {
    Pieces s;
    const juce::String all(vc2ValueString(id));
    const int u = all.indexOfChar(kUnit);
    s.unit = u < 0 ? juce::String() : all.substring(u + 1).removeCharacters("[]").trim();
    juce::StringArray parts;
    parts.addTokens(u < 0 ? all : all.substring(0, u), juce::String::charToString(kNames), {});
    for (const juce::String& part : parts) {
        const int at = part.indexOfChar(kRange);
        if (at >= 0) {
            s.pieces.push_back({ part.substring(0, at).trim(), part.substring(at + 1).trim(), true });
            ++s.ranges;
        } else if (part.trim().isNotEmpty()) {
            s.pieces.push_back({ part.trim(), {}, false });
            ++s.names;
        }
    }
    return s;
}

// A key's number by its name as the strings write it ("C-1" 0, "G9" 127), -1 if it isn't one
int keyNamed(const juce::String& name) {
    for (int key = 0; key < 128; ++key)
        if (Ui::keyName(key) == name) return key;
    return -1;
}

// An end of a range: the letters before its number, the number, its decimals and digits, and whether it is signed
struct End {
    juce::String prefix;
    double number = 0.0;
    int decimals = 0, digits = 0;
    bool plus = false, ok = false;
};
End rangeEnd(const juce::String& t) {
    End e;
    const int first = t.indexOfAnyOf("+-0123456789");
    if (first < 0) return e;
    e.prefix = t.substring(0, first);
    const juce::String number = t.substring(first);
    if (!number.containsOnly("+-.0123456789")) return e;
    e.number = number.getDoubleValue();
    e.plus = number.startsWithChar('+');
    const int dot = number.indexOfChar('.');
    e.decimals = dot < 0 ? 0 : number.length() - dot - 1;
    e.digits = number.retainCharacters("0123456789").length();
    e.ok = true;
    return e;
}

// How many values a range makes: the names leave the one range the rest; of several, each a step of one
int valuesOfRange(const Pieces& s, const Piece& p, int total) {
    if (s.ranges == 1) return total - s.names;
    const int ka = keyNamed(p.a), kb = keyNamed(p.b);
    if (ka >= 0 && kb >= 0) return kb - ka + 1;
    return juce::roundToInt(std::abs(rangeEnd(p.b).number - rangeEnd(p.a).number)) + 1;
}

// The value `i` of a range of `count`, or false if its ends don't make that many: notes a key a step, numbers in even
// steps of their decimals
bool rangeText(const Piece& p, int count, int i, const juce::String& unit, juce::String& out) {
    const int ka = keyNamed(p.a), kb = keyNamed(p.b);
    if (ka >= 0 && kb >= 0) {
        out = Ui::keyName(ka + i);
        return kb - ka + 1 == count;
    }
    const End a = rangeEnd(p.a), b = rangeEnd(p.b);
    if (!a.ok || !b.ok || a.prefix != b.prefix || count < 2) return false;
    const int decimals = juce::jmax(a.decimals, b.decimals);
    const double unitStep = std::pow(10.0, -decimals);
    const double step = (b.number - a.number) / (count - 1);
    if (step == 0.0 || std::abs(step / unitStep - std::round(step / unitStep)) > 1e-6) return false;
    const double v = a.number + step * i;
    juce::String t = decimals > 0 ? juce::String(v, decimals) : juce::String(juce::roundToInt(v));
    if (a.prefix.isNotEmpty()) t = a.prefix + t.paddedLeft('0', a.digits);
    else if ((a.plus || b.plus) && v > 0.0) t = "+" + t;
    out = unit.isNotEmpty() ? t + " " + unit : t;
    return true;
}

// Value `index` of `total` on the string's own scale, or false if the string doesn't make `total` values: a name a
// value, the ranges what valuesOfRange gives them
bool onScale(const Pieces& s, int index, int total, juce::String& out) {
    int at = 0;
    bool found = false;
    for (const Piece& p : s.pieces) {
        const int count = p.range ? valuesOfRange(s, p, total) : 1;
        if (count < 1) return false;
        if (!found && index < at + count) {
            juce::String t;
            if (p.range && !rangeText(p, count, index - at, s.unit, t)) return false;
            out = p.range ? t : p.a;
            found = true;
        }
        at += count;
    }
    return found && at == total;
}

template <size_t N>
bool listedIn(const int (&ids)[N], int id) {
    return std::find(std::begin(ids), std::end(ids), id) != std::end(ids);
}

}

Kind controlKind(int id, int lo, int hi) {
    if (vc2ParamDesc(0) == nullptr) return Kind::knob;
    const Pieces s = piecesOf(id);
    if (s.ranges == 0 && s.names == 0) return Kind::knob;
    if (s.ranges == 0) {
        const bool onOff = s.names == 2 && hi - lo == 1 && (s.pieces[0].a == "OFF" || s.pieces[0].a == "BYPASS") &&
                           s.pieces[1].a == "ON";
        return onOff ? Kind::toggle : Kind::box;
    }
    if (s.names == 0) return Kind::knob;
    return hi - lo + 1 <= kMostBoxed || s.ranges > 1 || s.names > 1 ? Kind::box : Kind::knob;
}

juce::String editorText(int id, int value, int lo, int hi, XtFxValueText* vsynth) {
    if (id == kCosmType && juce::isPositiveAndBelow(value, (int)std::size(kCosmTypes))) return kCosmTypes[value];
    const juce::String routine(vc2ValueText(id, value));
    if (listedIn(kEqFreq, id) || listedIn(kEqQ, id)) {
        if (vsynth == nullptr) return routine;
        return juce::String(vsynth->text(listedIn(kEqFreq, id) ? kVSynthEqFreq : kVSynthEqQ, value));
    }
    const Pieces s = piecesOf(id);
    const int total = hi - lo + 1;
    juce::String first, last, mine;
    if (!onScale(s, 0, total, first) || !onScale(s, total - 1, total, last) || !onScale(s, value - lo, total, mine))
        return routine;
    const bool formats = juce::String(vc2ValueText(id, lo)) == first && juce::String(vc2ValueText(id, hi)) == last;
    return formats ? routine : mine;
}

int romIdOf(const juce::String& paramId) {
    if (!paramId.startsWith(kVc2ParamPrefix)) return -1;
    return paramId.fromLastOccurrenceOf("_", false, false).getHexValue32();
}

int controllerOf(const juce::String& text) {
    return text.startsWith("CC") && text.substring(2).containsOnly("0123456789") ? text.substring(2).getIntValue() : -1;
}

}
