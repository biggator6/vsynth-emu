// The part on its OS model (Vc2SynthOsTasks.h, v355).
#include "Vc2SynthOsTasks.h"
#include "Vc2Synth.h"
#include "Vc2SynthMidi.h"

namespace {
using Os = Vc2SynthOs;
// [FW] the update task's start (0x893952c0…0x89395304): its cyclic handler, the kernel's record 10 (rom.4's
// 0x8992e24c's +0x28), its handler 0x893952a0 on and every 10 ticks; its alarm 10 ticks; the update queue's lock, the
// kernel's semaphore 6 (0x8992e208's +0x18, rom.1's .data)
constexpr int kUpdateRecord = 10, kUpdateHandler = 0, kSensingHandler = 1;
constexpr uint16_t kUpdateOn = 1, kUpdateTicks = 10;
constexpr uint32_t kUpdateLock = 0x880f3dc0u;
constexpr uint32_t kUpdateEvent = 2;   // [FW] the cyclic handler's (0x893952a0)
constexpr uint32_t kInputEvent = 4, kOutputEvent = 2;   // [FW] the DMA's interrupts' (0x89395d30, 0x89395d96)
// [FW] the audio task's object's words: the blocks counted (+0xc), the block busy (+0x28), the blocks missed (+0x2c)
constexpr size_t kBlocksWord = 0xc / 4, kBusyWord = 0x28 / 4, kMissedWord = 0x2c / 4;
constexpr size_t kOutput = 0, kInput = 1;   // the DMA buffers' indices' order at 0x89989cf0
constexpr uint32_t kBlockFrames = 32, kFramesPerMs = 44100, kMsPerSecond = 1000;
// [FW] the receive interrupts' sources (MIDI IN, USB), the channel statuses, and the classes of two bytes (0x8950cff8)
constexpr uint8_t kMidiIn = 1, kUsb = 2, kChannelFirst = 0x80, kSystemFirst = 0xf0, kClassBits = 0xf0;
constexpr uint8_t kProgram = 0xc0, kPressure = 0xd0;
} // namespace

const Vc2SynthOsTasks::Ways Vc2SynthOsTasks::kWays[Vc2SynthOs::kIdle] = {
    { &Vc2SynthOsTasks::audioStart, &Vc2SynthOsTasks::audioPass },
    { &Vc2SynthOsTasks::clockStart, &Vc2SynthOsTasks::clockPass },
    { &Vc2SynthOsTasks::notesStart, &Vc2SynthOsTasks::notesPass },
    { &Vc2SynthOsTasks::updateStart, &Vc2SynthOsTasks::updatePass },
    { &Vc2SynthOsTasks::inputStart, &Vc2SynthOsTasks::inputPass },
    { &Vc2SynthOsTasks::osMidiStart, &Vc2SynthOsTasks::osMidiPass },
    { &Vc2SynthOsTasks::frontStart, &Vc2SynthOsTasks::frontPass },
};

void Vc2SynthOsTasks::start() {
    os.start();
    midi.midiOn();
}

void Vc2SynthOsTasks::prologue(int task) { (this->*kWays[task].start)(); }

uint16_t Vc2SynthOsTasks::pass(int task, uint32_t events) { return (this->*kWays[task].pass)(events); }

void Vc2SynthOsTasks::audioStart() { s_.audioStart(); }

// The notes' task's start (its listener on the hub, 0x8938fc40), its alarm off, then the part's
void Vc2SynthOsTasks::notesStart() {
    midi.notesStart();
    os.kernel.setInterval(0);
    s_.taskStart();
}

void Vc2SynthOsTasks::updateStart() {
    s_.updateStarted = 1;
    os.kernel.cyclicMake(kUpdateRecord, kUpdateHandler, kUpdateOn, kUpdateTicks);
    s_.queue.q.post = Os::kObjects[Os::kUpdate];
    s_.queue.q.lock = kUpdateLock;
    os.kernel.setInterval(kUpdateTicks);
}

uint16_t Vc2SynthOsTasks::audioPass(uint32_t events) {
    if ((events & kInputEvent) != 0) {   // 0x89395b26: the mixer's input, the DMA's buffer just filled
        s_.mixer.w.inputAt = kInputBuffers + (s_.dmaBuffers[kInput] << kBufferShift);
        s_.input = input_;
    }
    if ((events & kOutputEvent) != 0) {   // 0x89395b56: the block, then not busy
        s_.block();
        s_.audioTask[kBusyWord] = 0;
    }
    return Os::kMasks[Os::kAudio];
}

uint16_t Vc2SynthOsTasks::clockPass(uint32_t events) {   // 0x89313c3a: its own alarm off first (0x89301800(0))
    if ((events & Vc2SynthClock::kAlarmEvent) != 0) os.kernel.setInterval(0);
    s_.clockPass(events);
    return Os::kMasks[Os::kClock];
}

uint16_t Vc2SynthOsTasks::notesPass(uint32_t events) {
    s_.notesPass(events);
    return Os::kMasks[Os::kNotes];
}

uint16_t Vc2SynthOsTasks::updatePass(uint32_t events) {
    s_.updatePass(events);
    return Os::kMasks[Os::kUpdate];
}

// The kernel's due cyclic handlers: the update task's (0x893952a0), the sensing check (0x89311330); MIDI out A's flush
// (0x893115e0) the MIDI out's, nothing here
void Vc2SynthOsTasks::cyclic(int handler) {
    if (handler == kUpdateHandler) {
        ++s_.ticks;
        os.raise(Os::kObjects[Os::kUpdate], kUpdateEvent);
    } else if (handler == kSensingHandler) {
        midi.sensing();
    }
}

// TMU0's application handler, after the kernel's tick: the OS clock the part reads (0x893018d0, the kernel's sysTime),
// then the clock's ms
void Vc2SynthOsTasks::appTick() {
    s_.clock = (int32_t)os.kernel.time;
    s_.clockMs();
}

void Vc2SynthOsTasks::inputDone() {
    s_.dmaBuffers[kInput] ^= 1;
    os.raise(Os::kObjects[Os::kAudio], kInputEvent);
}

void Vc2SynthOsTasks::outputDone() {
    uint32_t* const w = s_.audioTask.data();
    if (w[kBusyWord] == 0) {
        w[kBusyWord] = 1;
        s_.dmaBuffers[kOutput] ^= 1;
        os.raise(Os::kObjects[Os::kAudio], kOutputEvent);
        s_.outputFlag = 0;   // 0x893f36d0 answers 0
    } else {
        ++w[kMissedWord];
    }
    ++w[kBlocksWord];
}

void Vc2SynthOsTasks::block(const int32_t* input) {
    ++blocks;
    msAcc_ += kBlockFrames * kMsPerSecond;
    for (; msAcc_ >= kFramesPerMs; msAcc_ -= kFramesPerMs, ++ticks) interrupt([this] { os.tick(); });
    input_ = input;
    interrupt([this] { inputDone(); });
    interrupt([this] { outputDone(); });
}

// A channel message from MIDI IN or USB: its bytes, each its receive interrupt's (two for a program change or channel
// pressure, else three); any other the front's to send
void Vc2SynthOsTasks::message(const Vc2MidiMessage& m, uint32_t word) {
    ++messages;
    const bool channel = m.status >= kChannelFirst && m.status < kSystemFirst;
    if (channel && (m.source == kMidiIn || m.source == kUsb)) {
        const uint8_t cls = m.status & kClassBits;
        const uint8_t bytes[] = { m.status, m.data1, m.data2 };
        const size_t count = cls == kProgram || cls == kPressure ? 2 : 3;
        for (size_t k = 0; k < count; ++k) interrupt([&] { midi.receiveByte(m.source, bytes[k]); });
        return;
    }
    interrupt([&] { midi.give(m, word); });
}

void Vc2SynthOsTasks::deliver(const Vc2PartEvent& e) {
    if (e.block) block(e.input);
    else message(e.message, e.word);
}

bool Vc2SynthOsTasks::deliverNext() {
    Vc2PartEvent e;
    if (os.stuck() || !source_(e)) return false;
    deliver(e);
    return !os.stuck();
}

void Vc2SynthOsTasks::advance() {
    if (!deliverNext()) os.giveUp();
}
