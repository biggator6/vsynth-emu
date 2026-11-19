// xt_vc2keys <rom.4> [cases] [seed] (v276): the VC-2 part's keys, Vc2Keys, against rom.4's own routines on its own key
// tables and portamento keys in .bss (0x89982720…0x899888ac), the OS clock hooked to answer the ms an event draws.
// Each case starts from noise (every table's curve one of the eight, every glide aimed at a table, its held flags 0
// or 1 and its clock below 2^30), then 400 events (Vc2KeysEvents.cpp): a table's init, reset, note-on, note-off,
// pedals, clear and velocity factor, the held keys' queries on a table's held or latched keys, a glide's constructor,
// reset, note-on and note-off, and the three initializers.  Before each, a table's counts for a key, its held keys,
// Sostenuto or curve drawn now and then, or a glide's words.  After each, the span word for word and the answer.
// Not drawn: keys and note-on velocities past 127, a range below 0 or past 127 (rom.4 reads and writes beyond the
// tables there), held flags other than 0 and 1 (no routine writes another), and clocks 2^30 apart (XtKeyState's
// difference is an int's).  XT_VC2KEYS_FIRST=1 says where the first case to differ does, XT_VC2KEYS_STOP=1 stops
// there.  XT_VC2KEYS_MAIN only.
#ifdef XT_VC2KEYS_MAIN
#include "Vc2KeysCheck.h"
#include "Vc2KeysTables.h"
#include <cstring>

namespace {
using K = Vc2Keys;
constexpr int kSteps = 400;
constexpr uint32_t kSpanBytes = sizeof(Vc2Keys), kTableBytes = sizeof(Vc2KeyTable), kGlideBytes = sizeof(Vc2KeyGlide);
static_assert(K::kTablesAt + kSpanBytes == K::kOwnGlideAt + kGlideBytes, "the port's keys are rom.4's span");

// A word of the span by what holds it
std::string keysWordName(uint32_t at) {
    char t[64];
    const uint32_t glides = K::kGlidesAt + kGlideBytes * K::kGlides;
    if (at < K::kGlidesAt)
        std::snprintf(t, sizeof t, "table %u's +0x%03x", (at - K::kTablesAt) / kTableBytes,
                      (at - K::kTablesAt) % kTableBytes);
    else if (at < glides)
        std::snprintf(t, sizeof t, "glide %u's +0x%02x", (at - K::kGlidesAt) / kGlideBytes,
                      (at - K::kGlidesAt) % kGlideBytes);
    else
        std::snprintf(t, sizeof t, "the part's own words' +0x%03x", at - glides);
    return t;
}

std::string keysDifference(KeysSides& s) {
    std::vector<uint32_t> ours(kSpanBytes / 4), theirs(kSpanBytes / 4);
    std::memcpy(ours.data(), &s.port, kSpanBytes);
    s.fw.cpu.read(K::kTablesAt, theirs.data(), kSpanBytes);
    const auto [a, b] = std::ranges::mismatch(theirs, ours);
    if (a == theirs.end()) return "";
    const uint32_t at = K::kTablesAt + 4 * (uint32_t)(a - theirs.begin());
    char t[160];
    std::snprintf(t, sizeof t, "%s (0x%08x): %08x, against %08x", keysWordName(at).c_str(), at, *b, *a);
    return t;
}

// A glide's words as the routines meet them: aimed at a table, its flags 0 or 1, its clock below 2^30
void drawGlide(Vc2KeyGlide& g, std::mt19937& rng) {
    g.table = keysAnyTable(rng);
    g.lastMs = (int32_t)(rng() % kKeysClockMost);
    const uint8_t windows[6] = { 0, 1, 31, 32, 33, (uint8_t)rng() };
    g.window = windows[rng() % 6];
    for (uint8_t* flag : { &g.prevPolyHeld, &g.lastPolyHeld, &g.prevMonoHeld, &g.lastMonoHeld })
        *flag = (uint8_t)(rng() % 2);
    for (int8_t* key : { &g.key, &g.prevPoly, &g.lastPoly, &g.prevMono, &g.lastMono })
        *key = (int8_t)(rng() % 3 ? rng() % 128 : rng());
}

// A key's two counts (none, a few, as many, one short, at the most) and its held bit, usually set
void drawCounts(Vc2KeyTable& t, uint32_t key, std::mt19937& rng) {
    const uint8_t counts[6] = { 0, 1, 2, 254, 255, (uint8_t)rng() };
    t.ons[key] = counts[rng() % 6];
    t.offs[key] = rng() % 3 == 0 ? t.ons[key] : rng() % 2 ? (uint8_t)(t.ons[key] - 1) : counts[rng() % 6];
    const uint16_t bit = (uint16_t)(1u << (key & 15));
    t.held[key >> 4] = rng() % 4 ? (uint16_t)(t.held[key >> 4] | bit) : (uint16_t)(t.held[key >> 4] & ~bit);
}

void steerKeys(KeysSides& s, std::mt19937& rng) {
    s.drawnTable = keysAnyTable(rng);
    s.drawnKey = rng() % 128;
    Vc2KeyTable& t = s.port.table(s.drawnTable);
    drawCounts(t, s.drawnKey, rng);
    if (rng() % 4 == 0) t.held[rng() % 8] = (uint16_t)rng();
    const uint8_t pedals[6] = { 0, 0x3f, 0x40, 0x7f, 0xbf, (uint8_t)rng() };
    if (rng() % 8 == 0) t.sostenuto = pedals[rng() % 6];
    if (rng() % 8 == 0) t.curve = (uint8_t)(rng() % Vc2KeysTables::kCurves);
    if (rng() % 6 == 0) drawGlide(s.port.glide(keysAnyGlide(rng)), rng);
}

// A case of the check: the port's words noise, every table's curve and every glide's words as the routines meet them,
// then the events
std::string keysCase(KeysSides& s, std::mt19937& rng) {
    std::vector<uint32_t> noise(kSpanBytes / 4);
    for (uint32_t& w : noise) w = (uint32_t)rng();
    std::memcpy(&s.port, noise.data(), kSpanBytes);
    for (Vc2KeyTable* t : { &s.port.spare, &s.port.own }) t->curve = (uint8_t)(rng() % Vc2KeysTables::kCurves);
    for (Vc2KeyTable& t : s.port.tables) t.curve = (uint8_t)(rng() % Vc2KeysTables::kCurves);
    for (Vc2KeyGlide& g : s.port.glides) drawGlide(g, rng);
    drawGlide(s.port.ownGlide, rng);
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        steerKeys(s, rng);
        const std::span<const KeysEvent> events = rng() % 50 == 0 ? keysInits() : keysEvents();
        what = events[rng() % events.size()](s, rng);
    }
    return what;
}

void sayKeyCounts(const Vc2Run& run, const KeysCounts& n, long different) {
    const auto& e = n.byEvent;
    std::printf("keys: %d cases, %ld events: %ld inits, %ld resets, %ld note-ons (%ld at the most), %ld note-offs "
                "(%ld releasing), %ld Holds, %ld Sostenutos (%ld latching or clearing), %ld clears, %ld velocity "
                "factors\n", run.cases, n.events, e[0], e[1], e[2], n.saturated, e[3], n.released, e[4], e[5],
                n.latched, e[6], e[7]);
    std::printf("queries: %ld highest in a range (%ld found), %ld highest, %ld sets, %ld fills, %ld tests; glides: %ld "
                "constructors, %ld resets, %ld note-ons, %ld note-offs (%ld found); initializers: %ld, %ld and %ld\n",
                e[8], n.found, e[9], e[10], e[11], e[12], e[13], e[14], e[15], e[16], n.glidesFound, e[17], e[18],
                e[19]);
    std::printf("xt_vc2keys: %ld different\n", different);
}
} // namespace

KeysSides::KeysSides(Vc2Fw& rig) : fw(rig) {
    fw.cpu.hook(kKeysClockAt, [this](Sh4& c) { c.ret((uint32_t)ms); });
}

std::string keysOnBoth(KeysSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                       const std::function<uint32_t()>& mine) {
    s.fw.cpu.write(K::kTablesAt, &s.port, kSpanBytes);
    return vc2BothAnswer(s.fw, r, args, mine, [&] { return keysDifference(s); });
}

uint32_t keysAnyTable(std::mt19937& rng) {
    const uint32_t n = rng() % (K::kTables + 2);
    const uint32_t owned[2] = { K::kSpareAt, K::kOwnAt };
    return n < K::kTables ? K::kTablesAt + kTableBytes * n : owned[n - K::kTables];
}

uint32_t keysAnyGlide(std::mt19937& rng) {
    const uint32_t n = rng() % (K::kGlides + 1);
    return n < K::kGlides ? K::kGlidesAt + kGlideBytes * n : K::kOwnGlideAt;
}

int main(int argc, char** argv) {
    const Vc2CheckStart start { "xt_vc2keys", vc2LoadKeys, { "XT_VC2KEYS_FIRST", "XT_VC2KEYS_STOP" } };
    return vc2CheckMain<KeysSides>(argc, argv, start, keysCase, sayKeyCounts);
}
#endif
