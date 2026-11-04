#pragma once
// The V-Synth XT's effect chip, Roland's ESP4 (IC40, RA0A-101): MFX, Chorus, Reverb, the system's EQ, Master Level and Output Gain are its microcode, loaded from rom.2 by
// the SH-4's driver.  Two cores of 768 clocks a sample, each with 256 rotating registers (IRAM), sharing 256 more words (GRAM) and a delay memory (ERAM); core 1 alone
// drives the memory.  HARDWARE_FIRMWARE.md, "The chip is an ESP".
//
// The architecture is that of the JP-8000's TC170C140 as The Usual Suspects' JE-8086 has it (gearmulator, GPL-3.0; NOTICE.md).  This class holds only what rom.2's programs
// use, in integers, and XT_FX_MAIN checks it against their interpreter (research/third_party/gearmulator_esp): every word of its state, every sample, 0 different.  That says
// the port is their chip; only a capture from an XT can say their chip is this one.
//
// No program is in this source: the words reach it over the host bus, from the user's rom.2.
#include <cstdint>
#include <vector>

class XtFxChip {
public:
    XtFxChip();
    void reset();

    // The host bus, as the SH-4 has it at 0xb0000000: bytes, a word committed by its fourth.  0x2003 selects what a commit does (0x34 a word, 0x35 a coefficient into two
    // words, 0x36 a memory offset into five words' fields, 0x37 read a word into the mailbox).  Between frames only.
    void hostWrite(uint32_t address, uint8_t value);
    uint8_t hostRead(uint32_t address) const { return (uint8_t)((uint32_t)mailbox_ >> ((address & 3) * 8)); }
    // One transaction, as the driver makes them: 0x2003, then a word's four bytes at index × 4.
    void hostWord(uint8_t command, uint32_t index, uint32_t word) { hostWrite(0x2003, command); for (uint32_t k = 0; k < 4; k++) hostWrite(index * 4 + k, (uint8_t)(word >> (8 * k))); }
    // What a commit does to the words from its address on: how many it touches in a mode (0 a word, 1 a coefficient, 2 a memory offset), and their new values from the bytes.  XtFxControl's copy of the words goes through this too.
    static unsigned wordsOf(unsigned mode) { return mode == 0 ? 1 : mode == 1 ? 2 : 5; }
    static void merge(unsigned mode, const uint8_t bytes[4], uint32_t words[5]);

    // One sample: 768 clocks, then the registers, GRAM and the memory rotate by one.
    void runFrame();

    // GRAM relative to this sample's position, 24 bits signed, full scale 0x7fffff.  The frame comes in at 0x20…0x2e (MAIN, the Reverb's and Chorus's sends, MULTI, DIR), set
    // before runFrame() and read by the programs the sample after, and the mix leaves at 0xa0, 0xa2.  It rotates, so what a program wrote at m reads at m + 1 after runFrame().
    int32_t gram(unsigned m) const { return gram_[(pos_ + m) & 255]; }
    void setGram(unsigned m, int32_t v) { gram_[(pos_ + m) & 255] = v; }

    // The state XT_FX_MAIN compares.
    int32_t iram(unsigned core, unsigned m) const { return core_[core].iram[(pos_ + m) & 255]; }
    int32_t accA(unsigned core) const { return core_[core].acc[0]; }
    int32_t accB(unsigned core) const { return core_[core].acc[1]; }
    int32_t historyA(unsigned core, unsigned clocksBack) const { return core_[core].hist[0][(core_[core].h - clocksBack) & 3]; }
    int32_t historyB(unsigned core, unsigned clocksBack) const { return core_[core].hist[1][(core_[core].h - clocksBack) & 3]; }
    int32_t coefficient(unsigned i) const { return coef_[i]; }
    int32_t readLatch() const { return e_.readLatch; }
    int32_t writeLatch() const { return e_.writeLatch; }
    int32_t varOffset() const { return e_.var; }
    uint32_t memoryPosition() const { return e_.pos; }
    int32_t memoryWord(uint32_t a) const { return e_.m[a & 0x3ffff]; }
    uint32_t position() const { return pos_; }
    uint32_t word(unsigned i) const { return words_[i >> 10 & 1][i & 1023]; }

private:
    enum Kind : uint8_t { Nop, Mac, MacConstant, Dmac, StoreSat, GramRead, GramStoreAdd, SetCoef, MulCoef, Branch, SetAddress, SetWriteData, ReadData, StoreRaw, StoreRect, Compare, CompareConstant, FoldPos, FoldNeg };
    struct Instr {
        int32_t constant = 0;
        int32_t keep = -1;                                 // the accumulator's mask: 0 when the word replaces it
        uint16_t target = 0;
        Kind kind = Nop;
        uint8_t mem = 0, shift = 7, field = 0, coefIndex = 0, toB = 0, fromB = 0;
        int8_t coef = 0;
    };
    struct Core {
        int32_t iram[256];
        int32_t acc[2], hist[2][4];                        // A and B, and as they were, a ring at h
        uint32_t h;
        int32_t jumpAt, jumpTo, lastIn;                    // a branch taken lands two clocks on
        uint8_t lastTransform;
        bool lastMul;
        uint16_t skipPos, skipNeg;                         // a compare's result, on its way to gating the multiplicand
        uint8_t ready, values;
        bool pending, negative;
    };
    struct Eram {                                          // the memory's controller: an operation, four fields of offset, a transfer
        std::vector<int32_t> m;
        uint32_t pos, eff, imm;
        int32_t readLatch, writeLatch, var, stage, toCommit;
        uint8_t mode, modeCur;
    };

    static Instr decode(uint32_t word);
    void commit(uint32_t address);
    void setWord(uint32_t index, uint32_t value);
    int32_t coefValue(unsigned index, unsigned transform) const;
    void memoryClock(uint8_t field);
    template <unsigned K> void runCore();

    uint32_t words_[2][1024];
    Instr instr_[2][1024];
    Core core_[2];
    int32_t gram_[256], coef_[8], mailbox_;
    uint32_t pos_;
    Eram e_;
    int32_t hostTail_[2];
    uint8_t staging_[4], mode_;
};
