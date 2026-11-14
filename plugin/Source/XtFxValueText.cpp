// A parameter's value as the XT's panel shows it: see XtFxValueText.h (moved from XtFxControl.cpp as it stood, v199;
// then rom.2's constants and quirks made its description, kVSynthText).
#include "XtFxValueText.h"
#include "Sh4Interp.h"
#include <algorithm>
#include <cctype>
#include <iterator>

// ------------------------------------------------------------------ the values as text
namespace {
constexpr uint32_t kSp = 0x8C0E0000;
constexpr int kHighIds = 0x55d;
constexpr uint32_t kValueText = 0x89461a70;                                             // (id, value, 1 to add the unit): the GUI's text for a parameter's value goes here, through 0x89370ca0 and 0x89462120, with 0
constexpr uint32_t kValueStrings = 0x89792ba0, kValueStringsHigh = 0x89794114;          // each ID's string, which the routine reads too: the values' names or their range, and after a 3 the unit in brackets
constexpr uint32_t kReverbTime = 0x894b2bf0;                                            // (value): the time the driver takes for a Reverb Time, in samples at 44.1 kHz
constexpr uint64_t kTextSteps = 2000000;                                                // far more than the longest; the routine doesn't loop on anything but its own lists

// A note value comes back in the LCD font's own codes: the note, then a triplet's 0x17, a dot or a space.  Which note each is, xt_fx control measures against what the driver does with it at a tempo.
const char* noteOf(uint8_t glyph) {
    switch (glyph) {
        case 0x11: return "1/16";
        case 0x12: return "1/8";
        case 0x18: return "1/4";
        case 0x13: return "1/2";
        case 0x14: return "1/1";
        case 0x15: return "2/1";
        default:   return nullptr;
    }
}

// The routine has no format for four IDs, so it gives a bare number where the ID's own string and the manual have a scale.  The user's decision (2026-11-04): they read on that scale, so they run as an ID
// of the same range that has it.  xt_fx control measures that the driver treats them alike.
int formatOf(int id) {
    switch (id) {
        case 0x864: return 0x82b;                                                       // Hall 2's Room Size: Room 2's
        case 0x875: case 0x887: return 0x83c;                                           // Hall 3's and Garage's Room Size: Room 3's
        case 0x898: return 0x84d;                                                       // Plate's Xover Freq: Hall 1's
        default:    return id;
    }
}

// The Reverb Times read as the driver has them, not as the routine does.  Its format is out with the driver between values 6 and 21, 0.31...0.46 where the sound is 0.40...1.90, and it has none at all for
// Hall 2's, Hall 3's and Garage's.  The user's decision (2026-11-04).  xt_fx control measures the driver's times against what the combs' gains do.
bool isReverbTime(int id) {
    switch (id) {
        case 0x814: case 0x815:                                                         // Room 1's Low and Hi
        case 0x828:                                                                     // Room 2's
        case 0x839:                                                                     // Room 3's
        case 0x84b: case 0x84c:                                                         // Hall 1's Low and Hi
        case 0x861:                                                                     // Hall 2's
        case 0x872:                                                                     // Hall 3's
        case 0x884:                                                                     // Garage's
        case 0x896: case 0x897:                                                         // Plate's Low and Hi
            return true;
        default:
            return false;
    }
}

// The number a text starts with, with no regard to the locale.  `rest` is where it stops.
bool leadingNumber(const std::string& t, double& number, size_t& rest) {
    size_t i = 0;
    const bool negative = i < t.size() && t[i] == '-';
    if (i < t.size() && (t[i] == '-' || t[i] == '+')) i++;
    double v = 0, scale = 1;
    bool digits = false;
    for (; i < t.size() && std::isdigit((unsigned char)t[i]); i++) { v = v * 10 + (t[i] - '0'); digits = true; }
    if (i < t.size() && t[i] == '.') {
        size_t j = i + 1;
        for (; j < t.size() && std::isdigit((unsigned char)t[j]); j++) { scale /= 10; v += (t[j] - '0') * scale; digits = true; }
        if (j > i + 1) i = j;
    }
    if (!digits) return false;
    number = negative ? -v : v;
    rest = i;
    return true;
}

// Is it a number, alone or with a unit or a letter after it?  So "500Hz" and "63R" are, and "1/16", "1.5:1" and "2-STACK" aren't.
bool numberIn(const std::string& t, double& number) {
    size_t rest = 0;
    if (!leadingNumber(t, number, rest)) return false;
    while (rest < t.size() && t[rest] == ' ') rest++;
    return rest == t.size() || std::isalpha((unsigned char)t[rest]) || t[rest] == '%';
}

std::string squeezed(const std::string& t) {
    std::string out;
    for (const char ch : t) if (ch != ' ') out += (char)std::tolower((unsigned char)ch);
    return out;
}

// The strings lost a row at the Reverb's Non-Linear type: Feedback Time has what belongs to Feedback Level, that has Post HC Freq's, and Post HC Freq has an empty one, all the way through the routine's
// own unit.  The type's screen has its labels right, so these three take the right row.  Feedback Time's is gone, but Envelope T4 has the same values.
struct RowMove {
    int id, row;
};
constexpr RowMove kRowMoves[] = { { 0x8b6, 0x8b2 }, { 0x8b7, 0x8b6 }, { 0x8b8, 0x8b7 } };
int unitRowOf(int id) {
    const auto moved = std::ranges::find(kRowMoves, id, &RowMove::id);
    return moved == std::end(kRowMoves) ? id : moved->row;
}

constexpr XtTextImage kVSynthText { kValueText, kValueStrings, kValueStringsHigh, kHighIds, kReverbTime, formatOf,
                                    unitRowOf, isReverbTime };
}

const XtTextImage& xtVSynthText() { return kVSynthText; }

std::string xtNoteText(const std::string& t) {
    const char* note = t.empty() ? nullptr : noteOf((uint8_t)t[0]);
    if (!note) return {};
    return std::string(note) + (t.size() > 1 && t[1] == 0x17 ? "T" : t.size() > 1 && t[1] == '.' ? "." : "");
}

std::string xtFirmwareName(Sh4& image, uint32_t at, int most) {
    std::string t;
    for (int i = 0; i < most; ++i) {
        const char ch = (char)image.rd8(at + (uint32_t)i);
        if (!ch) break;
        t += ch;
    }
    const size_t star = t.find(" *");                                               // a mark for the manual's footnotes
    if (star != std::string::npos) t.erase(star);
    const size_t from = t.find_first_not_of(' '), to = t.find_last_not_of(' ');
    return from == std::string::npos ? std::string() : t.substr(from, to - from + 1);
}

XtFxValueText::XtFxValueText(const char* romPath, const XtTextImage& image)
    : image_(image), sh4_(std::make_unique<Sh4>()) {
    ok_ = sh4_->loadRom(romPath);
    if (ok_) sh4_->mapRam(0x8C000000, 0x00100000);
}

XtFxValueText::~XtFxValueText() = default;

const std::string& XtFxValueText::text(int id, int value) {
    const uint32_t key = (uint32_t)id << 16 | (uint16_t)value;
    const auto found = shown_.find(key);
    return found != shown_.end() ? found->second : shown_.emplace(key, make(id, value)).first->second;
}

// The unit from the ID's string, on its image's row (unitRowOf: rom.2's)
std::string XtFxValueText::unit(int id) {
    const int row = image_.unitRow(id);
    const int high = image_.firstHigh;
    Sh4& c = *sh4_;
    uint32_t a = row < high ? c.rd32(image_.strings + 4 * (uint32_t)row)
                            : c.rd32(image_.stringsHigh + 4 * (uint32_t)(row - high));
    for (uint8_t ch = c.rd8(a); ch != 3; ch = c.rd8(++a)) if (!ch) return {};
    std::string out;
    for (uint8_t ch = c.rd8(++a); ch; ch = c.rd8(++a)) if (ch != '[' && ch != ']') out += (char)ch;
    return out;
}

std::string XtFxValueText::make(int id, int value) {
    uint32_t at = 0;
    if (ok_ && image_.isTime(id)) {
        if (!sh4_->call(image_.time, { (uint32_t)value }, kSp, &at, kTextSteps)) return std::to_string(value);
        const uint32_t hundredths = (at * 100 + 22050) / 44100;
        return std::to_string(hundredths / 100) + "." + (hundredths % 100 < 10 ? "0" : "") + std::to_string(hundredths % 100) + " " + unit(id);
    }
    const uint32_t format = (uint32_t)image_.formatOf(id);
    if (!ok_ || !sh4_->call(image_.routine, { format, (uint32_t)value, 0 }, kSp, &at, kTextSteps))
        return std::to_string(value);
    std::string t;
    for (uint8_t ch = sh4_->rd8(at); ch; ch = sh4_->rd8(++at)) t += (char)ch;
    if (const std::string note = xtNoteText(t); !note.empty()) return note;
    const size_t from = t.find_first_not_of(' '), to = t.find_last_not_of(' ');
    t = from == std::string::npos ? std::string() : t.substr(from, to - from + 1);
    // The XT has the unit in a label beside the box, so it isn't saying THRU or LFO is in Hz or dB, and a 500Hz has its own.
    double number = 0; size_t rest = 0;
    if (leadingNumber(t, number, rest) && rest == t.size()) { const std::string u = unit(id); if (!u.empty()) t += " " + u; }
    return t;
}

int XtFxValueText::value(int id, int low, int high, const std::string& typed, int current) {
    const std::string wanted = squeezed(typed);
    if (wanted.empty()) return current;
    for (int v = low; v <= high; v++) if (squeezed(text(id, v)) == wanted) return v;
    double number = 0;
    if (numberIn(wanted, number)) {
        int nearest = low - 1; double best = 0;
        for (int v = low; v <= high; v++) {
            double there = 0;
            if (!numberIn(text(id, v), there)) continue;
            const double away = there > number ? there - number : number - there;
            if (nearest < low || away < best) { best = away; nearest = v; }
        }
        if (nearest >= low) return nearest;
    }
    for (int v = low; v <= high; v++) if (squeezed(text(id, v)).compare(0, wanted.size(), wanted) == 0) return v;
    return current;
}
