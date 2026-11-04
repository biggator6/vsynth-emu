#pragma once
// The SH-4's side of the V-Synth XT's effects: the firmware's own driver from the user's rom.2, in the interpreter, off the audio thread.  It reads each parameter by the
// firmware's ID from a store here, and what it would have written to the effect chip reaches XtFxChip through a queue the audio thread empties between samples.  So the mutes,
// the ramps, the order of a type change and every converter from a parameter to a coefficient are the firmware's.  HARDWARE_FIRMWARE.md, "XtFxControl".
//
// The driver's waits don't block anything here.  It runs straight through, and a wait becomes a count of samples in the stream, so the chip plays on for that long before it gets
// what the driver wrote next.  The words between two waits reach the chip together, between two samples: the XT's bus takes longer over them, at a speed nothing here knows.
//
// Two threads: one control thread, which makes every call marked so (one at a time), and the audio thread, which calls service().  service() takes no lock and allocates nothing.
#include <atomic>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

class Sh4;
class XtFxChip;

class XtFxControl {
public:
    // Where the driver's bus goes.  By default the queue, with reads answered by a copy of the chip's words kept on the control thread; the check puts the reference chip here.
    struct Bus {
        virtual ~Bus() = default;
        virtual void write(uint8_t command, uint16_t index, uint32_t word) = 0;         // 0x2003, then a word's four bytes at index × 4
        virtual uint32_t read(uint16_t index) = 0;                                      // the word, as the firmware's 0x894af510 returns it
        virtual void wait(uint32_t samples) = 0;                                        // the driver sleeps
    };

    // Control thread.  Loads rom.2 and starts the driver as the XT does, with the store at the firmware's defaults.  bus: nullptr for the queue.
    explicit XtFxControl(const char* romPath, Bus* bus = nullptr);
    ~XtFxControl();
    XtFxControl(const XtFxControl&) = delete;
    XtFxControl& operator=(const XtFxControl&) = delete;

    bool ok() const { return error_.empty(); }
    const std::string& error() const { return error_; }                                 // the interpreter's fault, or a ROM that won't load.  After one, nothing more goes to the chip

    // Control thread.  A parameter by the firmware's ID, in what its read returns: the stored value less the descriptor's centre.  set() clamps it to the descriptor's range.
    void set(int id, int value);
    int get(int id) const;
    int lowest(int id) const;
    int highest(int id) const;

    // Control thread.  Sends the firmware the message for what's been set since the last call.  update(): a live edit, of each block that changed.  loadPatch(): a patch change, all of
    // them muted and loaded again.
    void update();
    void loadPatch();
    bool pending() const { return dirty_ != 0; }                                        // has anything been set since the last of those?

    // Control thread.  The Matrix's MFX-PRM1…3, as it sums them (its slots 9…0xb), which the firmware hands the MFX on each pass (0x894e7aa0).  Nothing happens unless a value has
    // changed and the type assigns it, and a new type starts from nothing, so it wants calling as often as the firmware does.
    void setMatrix(int prm1, int prm2, int prm3);

    // Control thread, when nothing else has called anything for a while: frees what the chip has taken and passes on what the queue had no room for.  Without audio running the
    // driver's writes wait here, without limit.
    void pump();

    // Audio thread, before each runFrame().  The chip starts new, or reset, with this object, and isn't reset after: the driver keeps a copy of its words.
    void service(XtFxChip& chip);

    // For the check: called on the audio thread with each word as it goes to the chip.  Set before the audio thread starts.
    using Observer = void (*)(void* context, uint8_t command, uint16_t index, uint32_t word);
    void observe(Observer observer, void* context) { observer_ = observer; observerContext_ = context; }

    // Control thread: has the chip got everything?  And the driver's copy of a word.
    bool drained() const { return (!ok() || (backlog_.empty() && open_->words.empty())) && read_.load(std::memory_order_acquire) == write_.load(std::memory_order_relaxed); }
    uint32_t word(unsigned index) const { return shadow_[index & 2047]; }

private:
    struct Word { uint32_t value; uint16_t index; uint8_t command; };
    struct Batch { std::vector<Word> words; uint32_t waitAfter = 0; };                  // reaches the chip whole; then the chip plays on for waitAfter samples before the next
    static constexpr uint32_t kSlots = 1024;
    enum Block : uint8_t { BlockSwitches = 1, BlockMfx = 2, BlockChorus = 4, BlockReverb = 8, BlockSystem = 16, BlockAttenuation = 32 };

    void boot();
    bool call(uint32_t fn, std::initializer_list<uint32_t> args = {}, uint32_t* result = nullptr);
    void send(unsigned blocks, bool patch);
    void hooks();
    unsigned blockOf(int id);
    void put(uint8_t command, uint16_t index, uint32_t word);
    uint32_t take(uint16_t index);
    void sleep(uint32_t ms);
    void flush(uint32_t waitAfter);

    std::unique_ptr<Sh4> sh4_;
    Bus* bus_;
    std::string error_;
    std::vector<int16_t> store_, low_, high_;
    unsigned dirty_ = 0;

    // The control thread's side of the queue.
    uint32_t shadow_[2048] = {};
    std::unique_ptr<Batch> open_;
    std::deque<std::unique_ptr<Batch>> backlog_;
    uint32_t freed_ = 0;

    // Between the threads.  A batch is the control thread's until write_ passes it, then the audio thread's to read until read_ does, then the control thread's to free.
    Batch* slots_[kSlots] = {};
    alignas(64) std::atomic<uint32_t> write_{0};
    alignas(64) std::atomic<uint32_t> read_{0};
    alignas(64) uint32_t waiting_ = 0;                                                  // the audio thread's: samples left to play before the next batch
    Observer observer_ = nullptr;
    void* observerContext_ = nullptr;
};
