// The SH-4's side of the V-Synth XT's effects: see XtFxControl.h.
#include "XtFxControl.h"
#include "Sh4Interp.h"
#include "XtFxChip.h"
#include <algorithm>
#include <cctype>
#include <climits>

namespace {
constexpr uint32_t kThis = 0x89ced1b4, kMessage = 0x89ced200, kClock = 0x89979bfc, kSp = 0x8C0E0000, kMatrix = 0x8C0F0000;
constexpr uint32_t kDescLow = 0x897a167c, kDescHigh = 0x897a56d8;                 // parameter descriptors: IDs below kHighIds, 12 bytes each; the rest 8 bytes each
constexpr uint32_t kMfxIds = 0x897a8218, kChoIds = 0x897a82e4, kRevIds = 0x897a8310;  // for each type, its first and last parameter's ID
constexpr int kHighIds = 0x55d, kIds = 0x8d0, kMfxTypes = 42, kChoTypes = 9, kRevTypes = 11;
constexpr uint64_t kMaxSteps = 2000000000ull;                                     // far more than the longest thing the driver does; the interpreter stops rather than hang
constexpr uint32_t kSampleRate = 44100;
}

XtFxControl::XtFxControl(const char* romPath, Bus* bus) : bus_(bus), open_(std::make_unique<Batch>()) {
    sh4_ = std::make_unique<Sh4>();
    if (!sh4_->loadRom(romPath)) { error_ = "cannot load rom.2"; return; }
    sh4_->mapRam(0x8C000000, 0x00100000);
    // Some MFX types (3, 27) compare a byte behind two pointers that only 0x894cce60 sets, and nothing calls that.  On the XT they read the bottom of its memory through null, both the same byte.
    sh4_->mapRam(0x00000000, 0x00001000);
    Sh4& c = *sh4_;
    // The store, at the firmware's defaults: its read, 0x89481860, gives the stored value less the centre, and the default when the stored value's out of range.
    store_.assign(kIds, 0); low_.assign(kIds, 0); high_.assign(kIds, 0);
    for (int id = 0; id < kIds; id++) {
        int def, lo, hi;
        if (id < kHighIds) { const uint32_t d = kDescLow + 12 * (uint32_t)id; const int centre = (uint16_t)c.rd16(d + 4); def = (uint16_t)c.rd16(d + 6) - centre; lo = (uint16_t)c.rd16(d + 8) - centre; hi = (uint16_t)c.rd16(d + 10) - centre; }
        else { const uint32_t d = kDescHigh + 8 * (uint32_t)(id - kHighIds); def = (int16_t)c.rd16(d + 2); lo = (int16_t)c.rd16(d + 4); hi = (int16_t)c.rd16(d + 6); }
        store_[(size_t)id] = (int16_t)def; low_[(size_t)id] = (int16_t)lo; high_[(size_t)id] = (int16_t)hi;
    }
    hooks();
    boot();
}

XtFxControl::~XtFxControl() {
    for (uint32_t i = freed_; i != write_.load(std::memory_order_relaxed); i++) delete slots_[i & (kSlots - 1)];
}

bool XtFxControl::call(uint32_t fn, std::initializer_list<uint32_t> args, uint32_t* result) {
    if (!error_.empty()) return false;
    if (sh4_->call(fn, args, kSp, result, kMaxSteps)) return true;
    error_ = sh4_->error();
    return false;
}

// What the driver needs from outside itself: the chip's bus at its four primitives (each sends what the routine sends, byte for byte, read off 0x894af400, 0x894af510, 0x894af610,
// 0x894af6a0), the two spins between bus cycles, its sleep and its reads of the parameters.
void XtFxControl::hooks() {
    Sh4& c = *sh4_;
    c.hook(0x894af3a0, [](Sh4& m) { m.ret(0); });
    c.hook(0x894af3c0, [](Sh4& m) { m.ret(0); });
    c.hook(0x894b26a0, [this](Sh4& m) { sleep(m.r[4] & 0xffff); m.ret(0); });
    c.hook(0x894af400, [this](Sh4& m) { put(0x34, (uint16_t)m.r[4], m.r[5]); m.ret(0); });
    c.hook(0x894af510, [this](Sh4& m) { m.ret(take((uint16_t)m.r[4])); });
    c.hook(0x894af610, [this](Sh4& m) { const uint32_t v = (uint16_t)m.r[5], s = m.r[6] & 0xff;
        put(0x35, (uint16_t)m.r[4], (v >> 8 & 0xff) | ((v << 1 & 0xfc) | (s & 3)) << 8 | ((v >> 7 & 1) | (s << 2 & 12)) << 16); m.ret(0); });
    c.hook(0x894af6a0, [this](Sh4& m) { const uint16_t i = (uint16_t)m.r[4]; const uint32_t v = m.r[5], a = take(i), b = take((uint16_t)(i + 4));
        put(0x36, i, (a >> 20 & 0xf8) | (v & 0xff) << 8 | (v >> 8 & 0xff) << 16 | ((v >> 16 & 3) | (b >> 24 & 0xc0)) << 24); m.ret(0); });
    // The parameter store, at the firmware's two reads.
    const auto read = [this](Sh4& m) { const int id = (int16_t)m.r[5]; m.ret((uint32_t)(int32_t)(id >= 0 && id < kIds ? store_[(size_t)id] : 0)); };
    c.hook(0x8947d310, read); c.hook(0x8947d330, read);
    // Its start also takes something off the tone generator's lists, which aren't set up here and have nothing to do with the chip.
    for (const uint32_t fn : { 0x894d9230u, 0x894d90c0u, 0x894d9020u }) c.hook(fn, [](Sh4& m) { m.ret(0); });
    c.hook(0x89301680, [](Sh4& m) { m.ret(0); });                                 // the end of the system's reader: ID 0x77, to the kernel
}

// The XT's start, from 0x894abd84 and the task's own, 0x894ad560: the driver's state and the chip's first programs.  Then, as messages, what the system says and the first patch.
void XtFxControl::boot() {
    call(0x894b4a60);                                                             // the C++ static constructor of the object that clears the MFX's memory
    call(0x894ac6a0, { kThis });
    call(0x894ac780, { kThis });
    send(BlockSystem, true);
}

// ------------------------------------------------------------------ the store
unsigned XtFxControl::blockOf(int id) {
    if (id == 0x11d || (id >= 0x120 && id <= 0x122)) return BlockSwitches;
    if (id == 0x51e) return BlockAttenuation;
    if (id == 0x3e || id == 0x41 || id == 0x76 || (id >= 0x65 && id <= 0x70)) return BlockSystem;
    if ((id >= 0x177 && id <= 0x17a)) return BlockMfx;
    if (id >= 0x1fc && id <= 0x1fe) return BlockChorus;
    if (id == 0x230 || id == 0x231) return BlockReverb;
    Sh4& c = *sh4_;
    const auto within = [&](uint32_t table, int types) { const int first = (int16_t)c.rd16(table), last = (int16_t)c.rd16(table + 4 * (uint32_t)(types - 1) + 2); return id >= first && id <= last; };
    if (within(kMfxIds, kMfxTypes)) return BlockMfx;
    if (within(kChoIds, kChoTypes)) return BlockChorus;
    if (within(kRevIds, kRevTypes)) return BlockReverb;
    return 0;
}

void XtFxControl::set(int id, int value) {
    if (id < 0 || id >= kIds) return;
    const int v = std::clamp(value, (int)low_[(size_t)id], (int)high_[(size_t)id]);
    if (store_[(size_t)id] == v) return;
    store_[(size_t)id] = (int16_t)v;
    dirty_ |= blockOf(id);
}

int XtFxControl::get(int id) const { return id >= 0 && id < kIds ? store_[(size_t)id] : 0; }
int XtFxControl::lowest(int id) const { return id >= 0 && id < kIds ? low_[(size_t)id] : 0; }
int XtFxControl::highest(int id) const { return id >= 0 && id < kIds ? high_[(size_t)id] : 0; }

// ------------------------------------------------------------------ the driver's two kinds of change
// As the rest of the firmware asks for them: a message at 0x89ced200, flags in its second and fourth bytes, and the task's 0x894ac970 takes it.  So which readers run, in what order and with what
// arguments is the firmware's: a live edit runs those of the blocks that changed; a patch mutes Reverb, Chorus and MFX, clears the new Reverb, and forces a reload of each with the switches last.
void XtFxControl::send(unsigned blocks, bool patch) {
    sh4_->wr8(kMessage + 1, (uint8_t)((blocks & BlockSystem ? 0x20 : 0) | (blocks & BlockAttenuation ? 0x10 : 0)));
    sh4_->wr8(kMessage + 3, (uint8_t)((patch ? 0x80 : 0) | (blocks & BlockSwitches ? 0x40 : 0) | (blocks & BlockMfx ? 0x20 : 0) | (blocks & BlockChorus ? 0x10 : 0) | (blocks & BlockReverb ? 0x08 : 0)));
    call(0x894ac970, { kThis });
    flush(0);
}

void XtFxControl::update() {
    const unsigned d = dirty_; dirty_ = 0;
    if (d) send(d, false);
}

// What isn't a patch's (the system's Master, Output Gain and EQ, and Attenuation) goes as a live edit does.
void XtFxControl::loadPatch() {
    const unsigned d = dirty_ & (BlockSystem | BlockAttenuation); dirty_ = 0;
    send(d, true);
}

void XtFxControl::tick() {
    if (!drained()) return;
    if (patchWanted_) { patchWanted_ = false; loadPatch(); }
    else update();
}

// 0x894abe50, whose event then runs 0x894ad4f0: the values go to the MFX object, and those a type assigns move its parameters.
void XtFxControl::setMatrix(int prm1, int prm2, int prm3) {
    if (!error_.empty()) return;
    const int v[3] = { prm1, prm2, prm3 };
    for (uint32_t i = 0; i < 3; i++) sh4_->wr16(kMatrix + 2 * i, (uint16_t)v[i]);
    uint32_t changed = 0;
    if (call(0x894ad4e0, { kThis, kMatrix }, &changed) && changed) call(0x894ad4f0, { kThis });
    flush(0);
}

// The clock's object: +0x40 its source, +0x48 the ms over six clocks.  The task runs this on its event 0x20, and it does nothing unless the system's reader has seen Clock Source.
void XtFxControl::outsideClock(int sixClocksMs) {
    if (!error_.empty() || sixClocksMs <= 0) return;
    sh4_->wr8(kClock + 0x40, 1);
    sh4_->wr16(kClock + 0x48, (uint16_t)sixClocksMs);
    call(0x894acb10, { kThis });
    flush(0);
}

// ------------------------------------------------------------------ the driver's bus
void XtFxControl::put(uint8_t command, uint16_t index, uint32_t word) {
    if (bus_) { bus_->write(command, index, word); return; }
    const uint8_t mode = command & 3;
    if (mode != 3) {
        const unsigned n = XtFxChip::wordsOf(mode);
        if ((unsigned)index + n <= 2048) { uint32_t w[5]; for (unsigned i = 0; i < n; i++) w[i] = shadow_[index + i]; const uint8_t b[4] = { (uint8_t)word, (uint8_t)(word >> 8), (uint8_t)(word >> 16), (uint8_t)(word >> 24) }; XtFxChip::merge(mode, b, w); for (unsigned i = 0; i < n; i++) shadow_[index + i] = w[i]; }
    }
    open_->words.push_back({ word, index, command });
}

// The firmware reads a word through the mailbox.  The chip here is the audio thread's, so the answer comes from the copy, and the read still goes down the queue: it touches the chip.
uint32_t XtFxControl::take(uint16_t index) {
    if (bus_) return bus_->read(index);
    open_->words.push_back({ 0, index, 0x37 });
    return shadow_[index & 2047] & 0x0fffffff;
}

void XtFxControl::sleep(uint32_t ms) {
    const uint32_t samples = (ms * kSampleRate + 999) / 1000;
    if (bus_) { bus_->wait(samples); return; }
    flush(samples);
}

void XtFxControl::flush(uint32_t waitAfter) {
    if (bus_ || !error_.empty()) return;
    if (!open_->words.empty() || waitAfter) { open_->waitAfter = waitAfter; backlog_.push_back(std::move(open_)); open_ = std::make_unique<Batch>(); }
    pump();
}

void XtFxControl::pump() {
    const uint32_t r = read_.load(std::memory_order_acquire);
    for (; freed_ != r; freed_++) delete slots_[freed_ & (kSlots - 1)];
    uint32_t w = write_.load(std::memory_order_relaxed);
    while (!backlog_.empty() && w - r < kSlots) { slots_[w & (kSlots - 1)] = backlog_.front().release(); backlog_.pop_front(); w++; }
    write_.store(w, std::memory_order_release);
}

// ------------------------------------------------------------------ the audio thread
void XtFxControl::service(XtFxChip& chip) {
    if (waiting_ && --waiting_) return;
    uint32_t r = read_.load(std::memory_order_relaxed);
    const uint32_t w = write_.load(std::memory_order_acquire);
    while (r != w && !waiting_) {
        const Batch& b = *slots_[r & (kSlots - 1)];
        for (const Word& x : b.words) { chip.hostWord(x.command, x.index, x.value); if (observer_) observer_(observerContext_, x.command, x.index, x.value); }
        waiting_ = b.waitAfter;
        read_.store(++r, std::memory_order_release);
    }
}

// ------------------------------------------------------------------ the unit
XtFxUnit::XtFxUnit(const char* romPath) : chip_(std::make_unique<XtFxChip>()), control_(romPath) {}
XtFxUnit::~XtFxUnit() = default;

void XtFxUnit::settle() {
    XtFxChip& chip = *chip_;
    while (ok() && !control_.drained()) { control_.pump(); control_.service(chip); chip.runFrame(); }
}

int32_t XtFxUnit::toChip(float x) {
    // FTRC: toward zero, and to the limits past them.  This is on the XT's numbers: the tone's Out Gain, a power of two, moves no bits.  (The limits, spelled out, as a cast past them
    // is undefined in C++.  An ARM host's cast happens to do the same, so no check run here can show them missing; an Intel host's doesn't.)
    const float v = x * 0.0625f;
    const int32_t i = v >= 2147483648.0f ? INT32_MAX : v <= -2147483648.0f ? INT32_MIN : (int32_t)v;
    return i >> 8;
}

float XtFxUnit::fromChip(int32_t word) { return (float)word * 0x1p-23f; }

void XtFxUnit::process(const float* frame, float* lr, int frames) {
    XtFxChip& chip = *chip_;
    for (int i = 0; i < frames; ++i) {
        control_.service(chip);
        const float* f = frame + 8 * i;
        for (unsigned w = 0; w < 8; ++w) chip.setGram(0x20 + 2 * w, toChip(f[w]));
        chip.runFrame();
        // what a program wrote at a slot reads one on, once the chip has rotated
        lr[2 * i] = fromChip(chip.gram(0xa1)) + fromChip(chip.gram(0xa5));
        lr[2 * i + 1] = fromChip(chip.gram(0xa3)) + fromChip(chip.gram(0xa7));
    }
}

// ------------------------------------------------------------------ the catalog
XtFxCatalog xtFxCatalog(const char* romPath) {
    XtFxCatalog cat;
    const XtFxControl c(romPath);                                                       // its store holds the descriptors' ranges, and the defaults till something's set
    if (!c.ok()) return cat;
    Sh4& rom = *c.sh4_;
    constexpr uint32_t kNames = 0x89790338, kMfxNames = 0x89788c98;
    const auto text = [&](uint32_t a, int most) {
        std::string t;
        for (int i = 0; i < most; ++i) { const char ch = (char)rom.rd8(a + (uint32_t)i); if (!ch) break; t += ch; }
        const size_t star = t.find(" *");                                               // a mark for the manual's footnotes
        if (star != std::string::npos) t.erase(star);
        const size_t from = t.find_first_not_of(' '), to = t.find_last_not_of(' ');
        return from == std::string::npos ? std::string() : t.substr(from, to - from + 1);
    };
    const auto param = [&](int id) {
        XtFxParam p;
        p.id = id; p.low = c.lowest(id); p.high = c.highest(id); p.def = c.get(id);
        p.name = text(rom.rd32(kNames + 4 * (uint32_t)id), 40);
        return p;
    };
    const auto types = [&](uint32_t table, int count, std::vector<std::vector<XtFxParam>>& out) {
        out.resize((size_t)count);
        for (int t = 0; t < count; ++t) {
            const int first = (int16_t)rom.rd16(table + 4 * (uint32_t)t), last = (int16_t)rom.rd16(table + 4 * (uint32_t)t + 2);
            for (int id = first; id <= last; ++id) {
                XtFxParam p = param(id);
                if (p.high > p.low) out[(size_t)t].push_back(std::move(p));             // Thru's, and the Off types': one with no range and no name
            }
        }
    };
    types(kMfxIds, kMfxTypes, cat.mfx);
    types(kChoIds, kChoTypes, cat.chorus);
    types(kRevIds, kRevTypes, cat.reverb);
    for (int t = 0; t < kMfxTypes; ++t) cat.mfxTypes.push_back(text(kMfxNames + 24 * (uint32_t)t, 20));
    cat.chorusTypes = { "Off", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Feedback Chorus", "Flanger", "Short Delay", "Fbk Short Delay" };
    cat.reverbTypes = { "Off", "Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Hall 3", "Garage", "Plate", "Non-Linear", "Delay" };
    cat.eq.push_back(param(0x70));
    for (int id = 0x65; id <= 0x6f; ++id) cat.eq.push_back(param(id));
    cat.values = std::make_shared<XtFxValueText>(romPath);
    cat.ok = c.ok() && cat.values->ok();
    return cat;
}

// ------------------------------------------------------------------ the values as text
namespace {
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
}

XtFxValueText::XtFxValueText(const char* romPath) : sh4_(std::make_unique<Sh4>()) {
    ok_ = sh4_->loadRom(romPath);
    if (ok_) sh4_->mapRam(0x8C000000, 0x00100000);
}

XtFxValueText::~XtFxValueText() = default;

const std::string& XtFxValueText::text(int id, int value) {
    const uint32_t key = (uint32_t)id << 16 | (uint16_t)value;
    const auto found = shown_.find(key);
    return found != shown_.end() ? found->second : shown_.emplace(key, make(id, value)).first->second;
}

// The unit from the ID's string.  The strings lost a row at the Reverb's Non-Linear type: Feedback Time has what belongs to Feedback Level, that has Post HC Freq's, and Post HC Freq has an empty one, all
// the way through the routine's own unit.  The type's screen has its labels right, so these three take the right row.  Feedback Time's is gone, but Envelope T4 has the same values.
std::string XtFxValueText::unit(int id) {
    const int row = id == 0x8b6 ? 0x8b2 : id == 0x8b7 ? 0x8b6 : id == 0x8b8 ? 0x8b7 : id;
    Sh4& c = *sh4_;
    uint32_t a = row < kHighIds ? c.rd32(kValueStrings + 4 * (uint32_t)row) : c.rd32(kValueStringsHigh + 4 * (uint32_t)(row - kHighIds));
    for (uint8_t ch = c.rd8(a); ch != 3; ch = c.rd8(++a)) if (!ch) return {};
    std::string out;
    for (uint8_t ch = c.rd8(++a); ch; ch = c.rd8(++a)) if (ch != '[' && ch != ']') out += (char)ch;
    return out;
}

std::string XtFxValueText::make(int id, int value) {
    uint32_t at = 0;
    if (ok_ && isReverbTime(id)) {
        if (!sh4_->call(kReverbTime, { (uint32_t)value }, kSp, &at, kTextSteps)) return std::to_string(value);
        const uint32_t hundredths = (at * 100 + 22050) / 44100;
        return std::to_string(hundredths / 100) + "." + (hundredths % 100 < 10 ? "0" : "") + std::to_string(hundredths % 100) + " " + unit(id);
    }
    if (!ok_ || !sh4_->call(kValueText, { (uint32_t)formatOf(id), (uint32_t)value, 0 }, kSp, &at, kTextSteps)) return std::to_string(value);
    std::string t;
    for (uint8_t ch = sh4_->rd8(at); ch; ch = sh4_->rd8(++at)) t += (char)ch;
    if (!t.empty()) if (const char* note = noteOf((uint8_t)t[0])) return std::string(note) + (t.size() > 1 && t[1] == 0x17 ? "T" : t.size() > 1 && t[1] == '.' ? "." : "");
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
