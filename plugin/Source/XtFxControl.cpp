// The SH-4's side of the V-Synth XT's effects: see XtFxControl.h.
#include "XtFxControl.h"
#include "Sh4Interp.h"
#include "XtFxChip.h"
#include <algorithm>

namespace {
constexpr uint32_t kThis = 0x89ced1b4, kMessage = 0x89ced200, kSp = 0x8C0E0000, kMatrix = 0x8C0F0000;
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

// 0x894abe50, whose event then runs 0x894ad4f0: the values go to the MFX object, and those a type assigns move its parameters.
void XtFxControl::setMatrix(int prm1, int prm2, int prm3) {
    if (!error_.empty()) return;
    const int v[3] = { prm1, prm2, prm3 };
    for (uint32_t i = 0; i < 3; i++) sh4_->wr16(kMatrix + 2 * i, (uint16_t)v[i]);
    uint32_t changed = 0;
    if (call(0x894ad4e0, { kThis, kMatrix }, &changed) && changed) call(0x894ad4f0, { kThis });
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
