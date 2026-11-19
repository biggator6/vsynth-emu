// The SH-4's side of the V-Synth XT's effects: see XtFxControl.h.
#include "XtFxControl.h"
#include "Sh4Interp.h"
#include "XtFxChip.h"
#include <algorithm>
#include <cctype>
#include <climits>

namespace {
constexpr uint32_t kSp = 0x8C0E0000, kMatrix = 0x8C0F0000;
constexpr uint64_t kMaxSteps = 2000000000ull;                                     // far more than the longest thing the driver does; the interpreter stops rather than hang
constexpr uint32_t kSampleRate = 44100;
}

XtFxControl::XtFxControl(const char* romPath, Bus* bus, const XtFxImage& image)
    : image_(image), bus_(bus), open_(std::make_unique<Batch>()) {
    sh4_ = std::make_unique<Sh4>();
    if (!sh4_->loadRom(romPath)) { error_ = "cannot load the firmware image"; return; }
    sh4_->mapRam(0x8C000000, 0x00100000);
    // Some MFX types (3, 27) compare a byte behind two pointers that only 0x894cce60 sets, and nothing calls that.  On the XT they read the bottom of its memory through null, both the same byte.
    sh4_->mapRam(0x00000000, 0x00001000);
    Sh4& c = *sh4_;
    // The store, at the firmware's defaults: its read, 0x89481860, gives the stored value less the centre, and the default when the stored value's out of range.
    store_.assign((size_t)image_.ids, 0); low_.assign((size_t)image_.ids, 0); high_.assign((size_t)image_.ids, 0);
    for (int id = 0; id < image_.ids; id++) {
        int def, lo, hi;
        if (id < image_.firstHigh) {
            const uint32_t d = image_.descLow + 12 * (uint32_t)id;
            const int centre = (uint16_t)c.rd16(d + 4);
            def = (uint16_t)c.rd16(d + 6) - centre;
            lo = (uint16_t)c.rd16(d + 8) - centre;
            hi = (uint16_t)c.rd16(d + 10) - centre;
        } else {
            const uint32_t d = image_.descHigh + 8 * (uint32_t)(id - image_.firstHigh);
            def = (int16_t)c.rd16(d + 2);
            lo = (int16_t)c.rd16(d + 4);
            hi = (int16_t)c.rd16(d + 6);
        }
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

// What the driver needs from outside itself: the chip's bus at its four primitives (each sends what the routine sends,
// byte for byte, read off rom.2's 0x894af400, 0x894af510, 0x894af610, 0x894af6a0, and rom.4's the same), its sleep and
// its reads of the parameters; and the routines the image answers with nothing.
void XtFxControl::hooks() {
    Sh4& c = *sh4_;
    for (const uint32_t fn : image_.answered)
        if (fn) c.hook(fn, [](Sh4& m) { m.ret(0); });
    c.hook(image_.sleep, [this](Sh4& m) { sleep(m.r[4] & 0xffff); m.ret(0); });
    c.hook(image_.putWord, [this](Sh4& m) { put(0x34, (uint16_t)m.r[4], m.r[5]); m.ret(0); });
    c.hook(image_.getWord, [this](Sh4& m) { m.ret(take((uint16_t)m.r[4])); });
    c.hook(image_.putCoefficient, [this](Sh4& m) {
        const uint32_t v = (uint16_t)m.r[5], s = m.r[6] & 0xff;
        const uint32_t high = (v << 1 & 0xfc) | (s & 3), top = (v >> 7 & 1) | (s << 2 & 12);
        put(0x35, (uint16_t)m.r[4], (v >> 8 & 0xff) | high << 8 | top << 16);
        m.ret(0);
    });
    c.hook(image_.putOffset, [this](Sh4& m) {
        const uint16_t i = (uint16_t)m.r[4];
        const uint32_t v = m.r[5], a = take(i), b = take((uint16_t)(i + 4));
        const uint32_t top = (v >> 16 & 3) | (b >> 24 & 0xc0);
        put(0x36, i, (a >> 20 & 0xf8) | (v & 0xff) << 8 | (v >> 8 & 0xff) << 16 | top << 24);
        m.ret(0);
    });
    // The parameter store, at the firmware's two reads.
    const auto read = [this](Sh4& m) {
        const int id = (int16_t)m.r[5];
        if (readObserver_) readObserver_(readContext_, id);
        m.ret((uint32_t)(int32_t)(id >= 0 && id < image_.ids ? store_[(size_t)id] : 0));
    };
    for (const uint32_t fn : image_.reads) c.hook(fn, read);
}

// The XT's start, from rom.2's 0x894abd84 and the task's own, 0x894ad560: the driver's state and the chip's first
// programs.  Then, as messages, what the system says and the first patch.
void XtFxControl::boot() {
    call(image_.staticInit);   // the C++ static constructor of the object that clears the MFX's memory
    call(image_.start, { image_.self });
    call(image_.startTask, { image_.self });
    send(BlockSystem, true);
    routeInput();
}

// ------------------------------------------------------------------ the store
unsigned XtFxControl::blockOf(int id) {
    const auto inRow = [id](const XtFxImage::IdRow& r) { return id >= r.first && id <= r.last; };
    if (const auto row = std::ranges::find_if(image_.idRows, inRow); row != image_.idRows.end()) return row->block;
    Sh4& c = *sh4_;
    const auto within = [&](uint32_t table, int types) {
        const int first = (int16_t)c.rd16(table), last = (int16_t)c.rd16(table + 4 * (uint32_t)(types - 1) + 2);
        return id >= first && id <= last;
    };
    if (within(image_.mfxIds, image_.mfxTypes)) return BlockMfx;
    if (within(image_.choIds, image_.choTypes)) return BlockChorus;
    if (within(image_.revIds, image_.revTypes)) return BlockReverb;
    return 0;
}

void XtFxControl::set(int id, int value) {
    if (id < 0 || id >= image_.ids) return;
    const int v = std::clamp(value, (int)low_[(size_t)id], (int)high_[(size_t)id]);
    if (store_[(size_t)id] == v) return;
    store_[(size_t)id] = (int16_t)v;
    dirty_ |= blockOf(id);
}

int XtFxControl::get(int id) const { return id >= 0 && id < image_.ids ? store_[(size_t)id] : 0; }
int XtFxControl::lowest(int id) const { return id >= 0 && id < image_.ids ? low_[(size_t)id] : 0; }
int XtFxControl::highest(int id) const { return id >= 0 && id < image_.ids ? high_[(size_t)id] : 0; }

// ------------------------------------------------------------------ the driver's two kinds of change
// As the rest of the firmware asks for them: a message at the image's flags (rom.2's 0x89ced200), flags in its second
// and fourth bytes, and the task's reader (rom.2's 0x894ac970) takes it.  So which readers run, in what order and with what
// arguments is the firmware's: a live edit runs those of the blocks that changed; a patch mutes Reverb, Chorus and MFX, clears the new Reverb, and forces a reload of each with the switches last.
void XtFxControl::send(unsigned blocks, bool patch) {
    const unsigned second = (blocks & BlockSystem ? 0x20 : 0) | (blocks & BlockAttenuation ? 0x10 : 0);
    const unsigned fourth = (patch ? 0x80 : 0) | (blocks & BlockSwitches ? 0x40 : 0) | (blocks & BlockMfx ? 0x20 : 0)
                          | (blocks & BlockChorus ? 0x10 : 0) | (blocks & BlockReverb ? 0x08 : 0);
    sh4_->wr8(image_.message + 1, (uint8_t)second);
    sh4_->wr8(image_.message + 3, (uint8_t)fourth);
    call(image_.takeMessage, { image_.self });
    flush(0);
    if (blocks & BlockInput) routeInput();
}

void XtFxControl::update() {
    const unsigned d = dirty_; dirty_ = 0;
    if (d) send(d, false);
}

// What isn't a patch's (the system's Master, Output Gain and EQ, and Attenuation) goes as a live edit does.
void XtFxControl::loadPatch() {
    const unsigned d = dirty_ & (BlockSystem | BlockAttenuation | BlockInput); dirty_ = 0;
    send(d, true);
}

void XtFxControl::tick() {
    if (!drained()) return;
    if (patchWanted_) { patchWanted_ = false; loadPatch(); }
    else update();
}

// rom.2's 0x894abe50, whose event then runs 0x894ad4f0: the values go to the MFX object, and those a type assigns move
// its parameters.  An image with no routine mapped for them does nothing here.
void XtFxControl::setMatrix(int prm1, int prm2, int prm3) {
    if (!error_.empty() || !image_.matrixTake) return;
    const int v[3] = { prm1, prm2, prm3 };
    for (uint32_t i = 0; i < 3; i++) sh4_->wr16(kMatrix + 2 * i, (uint16_t)v[i]);
    uint32_t changed = 0;
    if (call(image_.matrixTake, { image_.self, kMatrix }, &changed) && changed)
        call(image_.matrixApply, { image_.self });
    flush(0);
}

// The clock's object: +0x40 its source, +0x48 the ms over six clocks.  The task runs this on its event 0x20, and it does nothing unless the system's reader has seen Clock Source.
void XtFxControl::outsideClock(int sixClocksMs) {
    if (!error_.empty() || sixClocksMs <= 0) return;
    sh4_->wr8(image_.clock + 0x40, 1);
    sh4_->wr16(image_.clock + 0x48, (uint16_t)sixClocksMs);
    call(image_.clockPoll, { image_.self });
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

// ------------------------------------------------------------------ the catalog
XtFxCatalog xtFxCatalog(const char* romPath) {
    XtFxCatalog cat;
    const XtFxControl c(romPath);                                                       // its store holds the descriptors' ranges, and the defaults till something's set
    if (!c.ok()) return cat;
    Sh4& rom = *c.sh4_;
    constexpr uint32_t kNames = 0x89790338, kMfxNames = 0x89788c98;
    const auto text = [&](uint32_t a, int most) { return xtFirmwareName(rom, a, most); };
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
    const XtFxImage& im = xtVSynthFx();                                                 // the catalog is rom.2's
    types(im.mfxIds, im.mfxTypes, cat.mfx);
    types(im.choIds, im.choTypes, cat.chorus);
    types(im.revIds, im.revTypes, cat.reverb);
    for (int t = 0; t < im.mfxTypes; ++t) cat.mfxTypes.push_back(text(kMfxNames + 24 * (uint32_t)t, 20));
    cat.chorusTypes = { "Off", "Chorus 1", "Chorus 2", "Chorus 3", "Chorus 4", "Feedback Chorus", "Flanger", "Short Delay", "Fbk Short Delay" };
    cat.reverbTypes = { "Off", "Room 1", "Room 2", "Room 3", "Hall 1", "Hall 2", "Hall 3", "Garage", "Plate", "Non-Linear", "Delay" };
    cat.eq.push_back(param(0x70));
    for (int id = 0x65; id <= 0x6f; ++id) cat.eq.push_back(param(id));
    cat.values = std::make_shared<XtFxValueText>(romPath);
    cat.ok = c.ok() && cat.values->ok();
    return cat;
}
