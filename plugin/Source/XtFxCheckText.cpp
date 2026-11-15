// xt_fx control's value text: every value of every parameter against the firmware's routine and driver (XtFxCheck.h;
// moved from XtFxChip.cpp as it stood, v204).  XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheck.h"

namespace xtfxcheck {
// The panel's text.  Every value of every parameter against the firmware's routine itself, without the unit and with it.  What the plugin does to the text is small: the note values' names, the spaces, and a
// space and no brackets for the unit, which goes only on a number.  What's left over is the three of Non-Linear whose strings are a row out.  Then the note names against what the driver does at a tempo,
// and each value's text typed back in.
void valueText(const char* rom) {
    const XtFxCatalog cat = xtFxCatalog(rom);
    Sh4 fw; fw.loadRom(rom); fw.mapRam(0x8C000000, 0x00100000);
    int tried = 0, different = 0;
    const bool first = xtSwitch("XT_FX_FIRST") != nullptr;
    const auto check = [&](bool good, const char* what, int id = 0, int v = 0, const std::string& got = {}) { tried++; if (!good) { different++; if (first) std::printf("  value text: %s, ID %x value %d: \"%s\"\n", what, id, v, got.c_str()); } };
    const auto routine = [&](uint32_t fn, int id, int v) {
        uint32_t at = 0; std::string t;
        if (!fw.call(fn, { (uint32_t)id, (uint32_t)v }, 0x8C0E0000, &at, 2000000)) return std::string("a fault");
        for (uint8_t ch = fw.rd8(at); ch; ch = fw.rd8(++at)) t += (char)ch;
        return t;
    };
    const auto isNumber = [](const std::string& t) {
        size_t i = t[0] == '-' || t[0] == '+' ? 1 : 0, digits = 0, dots = 0;
        for (; i < t.size(); i++) { if (t[i] == '.') dots++; else if (t[i] >= '0' && t[i] <= '9') digits++; else return false; }
        return digits > 0 && dots <= 1;
    };
    static const std::map<std::string, std::string> notes = { { "\x11", "1/16" }, { "\x12", "1/8" }, { "\x18", "1/4" }, { "\x13", "1/2" }, { "\x14", "1/1" }, { "\x15", "2/1" } };
    // The four the routine has no format for go by an ID of the same range that has one, and keep their own units.
    static const std::map<int, int> borrowed = { { 0x864, 0x82b }, { 0x875, 0x83c }, { 0x887, 0x83c }, { 0x898, 0x84d } };
    // The Reverb Times read as the driver's table has them, which is the routine's format but between 6 and 21.  Its two tables are one where they meet.
    static const std::set<int> reverbTimes = { 0x814, 0x815, 0x828, 0x839, 0x84b, 0x84c, 0x861, 0x872, 0x884, 0x896, 0x897 };
    const auto tableSeconds = [&](int v) { const int hundredths = (int)((fw.rd32(0x897ac574 + 4 * (uint32_t)v) * 100 + 22050) / 44100); char b[16]; std::snprintf(b, sizeof b, "%d.%02d", hundredths / 100, hundredths % 100); return std::string(b); };
    for (uint32_t v = 0; v < 64; v++) check(fw.rd32(0x897ac474 + 4 * v) == fw.rd32(0x897ac574 + 4 * v), "the driver's two tables of times", 0, (int)v);
    for (int v = 0; v < 80; v++) {
        const std::string format = routine(0x89462120, 0x84b, v);
        if (first && v >= 5 && v <= 22) std::printf("  Reverb Time %2d: the routine says %s, the driver's table %s\n", v, format.c_str(), tableSeconds(v).c_str());
        check((format == tableSeconds(v)) == (v < 6 || v > 21), "where the routine's format and the driver's table part", 0x84b, v, format);
    }
    int kinds[4] = {}, values = 0, outOfRow = 0, timesShown = 0;
    const auto param = [&](const XtFxParam& p) {
        const auto lent = borrowed.find(p.id);
        if (reverbTimes.count(p.id)) {
            timesShown++;
            for (int v = p.low; v <= p.high; v++) { values++; const std::string shown = cat.values->text(p.id, v); check(shown == tableSeconds(v) + " sec", "a Reverb Time's text", p.id, v, shown); check(cat.values->text(p.id, cat.values->value(p.id, p.low, p.high, shown, p.low == v ? p.high : p.low)) == shown, "the text typed back", p.id, v, shown); }
            return;
        }
        for (int v = p.low; v <= p.high; v++) {
            values++;
            const std::string own = routine(0x89462120, p.id, v), bare = lent != borrowed.end() ? routine(0x89462120, lent->second, v) : own, with = routine(0x89462130, p.id, v), shown = cat.values->text(p.id, v);
            check(with.compare(0, own.size(), own) == 0 && with.size() >= own.size(), "the routine with a unit doesn't start as it does without", p.id, v, with);
            std::string unit = with.substr(std::min(own.size(), with.size()));
            if (p.id >= 0x8b6 && p.id <= 0x8b8) { unit = p.id == 0x8b6 ? "[ms]" : p.id == 0x8b7 ? "[%]" : "[Hz]"; outOfRow++; }
            const auto note = notes.find(bare.substr(0, 1));
            std::string want;
            if (note != notes.end()) { want = note->second + (bare.substr(1) == "\x17" ? "T" : bare.substr(1) == "." ? "." : ""); kinds[0]++; }
            else {
                const size_t a = bare.find_first_not_of(' '), b = bare.find_last_not_of(' ');
                want = a == std::string::npos ? "" : bare.substr(a, b - a + 1);
                if (isNumber(want) && unit.size() > 2) { want += " " + unit.substr(1, unit.size() - 2); kinds[1]++; } else kinds[isNumber(want) ? 2 : 3]++;
            }
            check(!want.empty() && shown == want, "the text", p.id, v, shown);
            check(cat.values->text(p.id, cat.values->value(p.id, p.low, p.high, shown, p.low == v ? p.high : p.low)) == shown, "the text typed back", p.id, v, shown);
        }
    };
    for (const auto* list : { &cat.mfx, &cat.chorus, &cat.reverb }) for (const auto& type : *list) for (const XtFxParam& q : type) param(q);
    for (const XtFxParam& q : cat.eq) param(q);
    if (first) std::printf("  value text: %d values: %d notes, %d numbers with units, %d without, %d names; %d out of row\n", values, kinds[0], kinds[1], kinds[2], kinds[3], outOfRow);
    check(values == 53128, "the values"); check(timesShown == 11, "the Reverb Times");

    // Typing: what the box takes besides a value's own text.
    const auto typed = [&](int id, int low, int high, const char* text, const char* want) { const std::string got = cat.values->text(id, cat.values->value(id, low, high, text, low)); check(got == want, text, id, 0, got); };
    typed(0x55e, 0, 19, "200", "200 Hz"); typed(0x55e, 0, 19, "200hz", "200 Hz"); typed(0x55e, 0, 19, "210", "200 Hz");         // Parametric EQ's Low Freq: a number, the nearest, in any case
    typed(0x55f, 0, 30, "+6", "+6 dB"); typed(0x55f, 0, 30, "-6 dB", "-6 dB"); typed(0x55f, 0, 30, "6", "+6 dB"); typed(0x55f, 0, 30, "-6", "-6 dB");    // its Low Gain
    typed(0x57a, 0, 127, "1/16.", "1/16."); typed(0x57a, 0, 127, "1/8t", "1/8T"); typed(0x57a, 0, 127, "2.03", "2.05 Hz");       // Sweep Rate
    typed(0x5de, 0, 121, "2", "0 ms");                                                                                           // Delay Time: 2/1 isn't the number 2
    typed(0x578, 0, 5, "mid+h", "MID+HIGH");                                                                                     // Band Mode: the start of a name
    check(cat.values->value(0x578, 0, 5, "nothing", 3) == 3 && cat.values->value(0x578, 0, 5, " ", 3) == 3, "what it doesn't know stays where it was");
    typed(0x5f7, 0, 126, "l20", "L20"); typed(0x5f7, 0, 126, " L 20 ", "L20"); typed(0x5f7, 0, 126, "20r", "20R");               // a Pan
    typed(0x7c2, 0, 16, "THRU", "THRU"); typed(0x7c2, 0, 16, "2100", "2000Hz");                                                  // Chorus 1's Pre Low Freq, with the firmware's own Hz
    typed(0x8ae, 0, 110, "95.4%", "95 %");                                                                                      // Env Time Ratio

    // The note names, against the driver: at a tempo, a Rate's coefficient goes as its Hz, and a note's Hz is the beats per second over the note's beats.  Sweep Rate has all sixteen.
    struct Coef : XtFxControl::Bus {
        std::map<int, uint32_t> last;
        void write(uint8_t command, uint16_t index, uint32_t word) override { if (command == 0x35) last[index] = word; }
        uint32_t read(uint16_t) override { return 0; }
        void wait(uint32_t) override {}
    };
    const auto number = [](uint32_t word) { return (double)((word & 0xff) << 8 | (word >> 16 & 1) << 7 | (word >> 9 & 0x7e)); };   // the bus's encoding of a coefficient (XtFxControl::hooks)
    const auto beatsOf = [](const std::string& n) {
        const size_t s = n.find('/');
        double b = 4.0 * realOf(n.c_str()) / realOf(n.c_str() + s + 1);
        if (n.back() == 'T') b *= 2.0 / 3;
        if (n.back() == '.') b *= 1.5;
        return b;
    };
    int measured = 0;
    for (const int tempo : { 1200, 900 }) {
        Coef bus; XtFxControl a(rom, &bus); a.set(0x11d, tempo); a.set(0x120, 1); a.set(0x177, 3); a.loadPatch();
        const auto rate = [&](int v) { a.set(0x57a, v); a.update(); return number(bus.last[0x5ca]); };
        const double perHz = rate(19) / 1.0;                                                                                      // value 19 is 1.00 Hz
        check(cat.values->text(0x57a, 19) == "1.00 Hz", "Sweep Rate's 1.00 Hz", 0x57a, 19, cat.values->text(0x57a, 19));
        for (int v = 112; v <= 127; v++) {
            const std::string n = cat.values->text(0x57a, v);
            const double want = tempo / 600.0 / beatsOf(n), got = rate(v) / perHz;
            if (first) std::printf("  note %3d %-6s at %4.1f BPM: %6.3f Hz, %6.3f by its name\n", v, n.c_str(), tempo / 10.0, got, want);
            check(n.find('/') != std::string::npos && std::abs(got / want - 1) < 0.01, "a note's name against the driver", 0x57a, v, n);
            measured++;
        }
    }
    // The four: each has its lender's range, its ends are as its own string says, and the driver treats it alike.  Plate's Xover Freq writes what Hall 1's does, value by value.  The three Reverb Times the routine
    // has no format for: their ends as their own strings say.
    {
        Coef idle; XtFxControl e(rom, &idle);
        std::map<int, int> ends = borrowed;
        for (const int id : { 0x861, 0x872, 0x884 }) ends[id] = id;
        for (const auto& [id, from] : ends) {
            check(e.lowest(id) == e.lowest(from) && e.highest(id) == e.highest(from), "a lender's range", id);
            std::string own; for (uint32_t a = fw.rd32(0x89794114 + 4 * (uint32_t)(id - 0x55d)); fw.rd8(a) > 3; a++) own += (char)fw.rd8(a);     // up to the first mark
            uint32_t a = fw.rd32(0x89794114 + 4 * (uint32_t)(id - 0x55d)) + (uint32_t)own.size() + 1;
            std::string last; for (; fw.rd8(a) > 3; a++) last += (char)fw.rd8(a);
            int top = e.highest(id); while (top > e.lowest(id) && !std::isdigit((unsigned char)cat.values->text(id, top)[0])) top--;
            check(std::abs(realOf(cat.values->text(id, e.lowest(id)).c_str()) - realOf(own.c_str())) < 0.001,
                  "a lent scale's low end against the ID's own string", id, e.lowest(id),
                  cat.values->text(id, e.lowest(id)));
            check(std::abs(realOf(cat.values->text(id, top).c_str()) - realOf(last.c_str())) < 0.001,
                  "a lent scale's high end against the ID's own string", id, top, cat.values->text(id, top));
        }
        const auto words = [&](int id, int v, Coef& bus, XtFxControl& a) { bus.last.clear(); a.set(id, v); a.update(); std::vector<double> c; for (const auto& w : bus.last) c.push_back(number(w.second)); std::sort(c.begin(), c.end()); return c; };
        Coef p8, p4; XtFxControl plate(rom, &p8), hall(rom, &p4);
        plate.set(0x122, 1); plate.set(0x230, 8); plate.loadPatch(); hall.set(0x122, 1); hall.set(0x230, 4); hall.loadPatch();
        for (int v = 21; v >= 0; v--) check(words(0x898, v, p8, plate) == words(0x84d, v, p4, hall), "Plate's Xover Freq against Hall 1's", 0x898, v);
    }
    // Reverb Time: a comb's feedback gain goes as 10^(-3 delay / time), so its log times the time is a constant, on what the text says, at every value.  With two bands, Low and Hi go together.  A coefficient that
    // grows with the time is the gain itself, and one that falls is -gain.
    {
        struct Time { int type, low, hi; uint32_t comb; bool negative; };
        for (const Time& t : { Time{ 1, 0x814, 0x815, 0x4b0, true }, Time{ 2, 0x828, 0, 0x53b, true }, Time{ 3, 0x839, 0, 0x4e7, true }, Time{ 4, 0x84b, 0x84c, 0x47e, false }, Time{ 5, 0x861, 0, 0x53b, true },
                               Time{ 6, 0x872, 0, 0x4e7, true }, Time{ 7, 0x884, 0, 0x4e7, true }, Time{ 8, 0x896, 0x897, 0x4bc, true } }) {
            Coef bus; XtFxControl a(rom, &bus); a.set(0x122, 1); a.set(0x230, t.type); a.loadPatch();
            const int high = a.highest(t.low);
            std::vector<double> product, steady;
            const auto seconds = [&](int v) { return realOf(cat.values->text(t.low, v).c_str()); };
            for (int v = 0; v <= high; v++) {
                a.set(t.low, v); if (t.hi) a.set(t.hi, v); a.update();
                const double c = number(bus.last[(int)t.comb]), gain = (t.negative ? 65536.0 - c : c) / 32768.0;
                product.push_back(gain > 0.01 && gain < 0.9995 ? -std::log10(gain) * seconds(v) : -1);                              // past those the coefficient can't tell
                if (product.back() >= 0 && v >= 30) steady.push_back(product.back());
            }
            std::sort(steady.begin(), steady.end());
            const double k = steady.empty() ? 1 : steady[steady.size() / 2];
            int measuredHere = 0;
            for (int v = 0; v <= high; v++) if (product[(size_t)v] >= 0) { measuredHere++; if (first && v <= 24) std::printf("  type %d Reverb Time %2d: says %.2f, the driver's gain %.2f\n", t.type, v, seconds(v), seconds(v) * k / product[(size_t)v]); check(std::abs(product[(size_t)v] / k - 1) < 0.03, "a Reverb Time against the driver's gain", t.low, v, cat.values->text(t.low, v)); }
            check(measuredHere > high * 7 / 10, "a Reverb Time measured at too few values", t.low);
        }
    }
    // And a delay's coefficient goes as its time: Stereo Delay's Delay Time, those that fit under its 650 ms at 120 BPM.
    {
        Coef bus; XtFxControl a(rom, &bus); a.set(0x11d, 1200); a.set(0x120, 1); a.set(0x177, 11); a.loadPatch();
        const auto time = [&](int v) { a.set(0x5de, v); a.update(); return number(bus.last[0x5c8]); };
        const double perMs = time(20) / 100.0;                                                                                    // value 20 is 100 ms
        check(cat.values->text(0x5de, 20) == "100 ms", "Delay Time's 100 ms", 0x5de, 20, cat.values->text(0x5de, 20));
        for (int v = 106; v <= 112; v++) {
            const std::string n = cat.values->text(0x5de, v);
            const double want = 500.0 * beatsOf(n), got = time(v) / perMs;
            if (first) std::printf("  note %3d %-6s at 120 BPM: %6.1f ms, %6.1f by its name\n", v, n.c_str(), got, want);
            check(n.find('/') != std::string::npos && std::abs(got / want - 1) < 0.01, "a note's name against the delay", 0x5de, v, n);
            measured++;
        }
    }
    check(measured == 39, "the notes measured");
    std::printf("control's value text against the firmware's routine and driver: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck
#endif
