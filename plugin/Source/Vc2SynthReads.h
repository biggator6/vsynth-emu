#pragma once
// C's reads of the part and its children's routines, the part's own (v343, Vc2Synth whole's fourth version): what C,
// M and C's children read past their own words, from the part's memory by rom.4's address (Vc2SynthMemory, through
// Vc2SetMemoryReads as xt_vc2sets reads rom.4's), and what runs rom.4's routines: a Matrix group's sums by its reader
// (vc2MatrixRead with the Matrix core on M by address, its sources on the channel state M's +0x168 names, G's seven
// assignments and the part's byte by the part table 0x8993d040), the OSC's four getters, the pitch source's ratio
// (0x89394a40, Auto Note's), the velocity curve and the shared routines (Vc2ToneMath), the step counts at their
// address, the waves the part is given; and each child's routine by its row in rom.4's table to the part's pool of that
// kind (the common, the OSC blocks, the mix, the envelopes, the LFOs, the COSM inputs, the TVA), its block the note
// W's at the address C gives.  Bit for bit: xt_vc2synth's reads part.  HARDWARE_FIRMWARE.md, "C's reads in the part".
// The notes' and update tasks.
#include "Vc2MatrixSources.h"
#include "Vc2SetMemoryReads.h"
#include "Vc2SynthMemory.h"

class Vc2SynthReads final : public Vc2SetMemoryReads {
public:
    explicit Vc2SynthReads(Vc2Synth& s) : s_(s), memory_(s) {}
    // one-off: Vc2SetParts' overrides, which each of its two sides (this and xt_vc2sets' SetPortParts) declares.  The
    // children's routines, each on the part's pool of the row's kind
    uint32_t allocate(const Vc2SetChild& c, uint32_t number, uint32_t index) override;
    void construct(const Vc2SetChild& c, const Vc2SetChildCall& call) override;
    void end(const Vc2SetChild&, uint32_t) override {}   // [FW] every end is a bare return
    void convert(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call, uint32_t noteAt) override;
    void update(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    void copy(const Vc2SetChild& c, uint32_t object, uint32_t noteAt) override;
    void release(const Vc2SetChild& c, uint32_t object, uint32_t velocity, uint32_t noteAt) override;
    uint32_t refresh(const Vc2SetChild& c, uint32_t object, const Vc2SetCall& call) override;
    // What runs rom.4's routines
    int velocityCurve(int velocity, int sens, int curve) override;
    int16_t matrixSum(uint32_t reader, uint32_t matrix, uint32_t offset) override;
    const Vc2ToneShared& shared() override;
    const int16_t* matrixGroup(uint32_t reader, uint32_t matrix) override;
    uint16_t nextOscStep(uint32_t part, uint32_t kind, uint32_t osc) override;
    Vc2MatrixGroups oscGroups(uint32_t matrix, uint32_t osc) override;
    float pitchSource() override;
    const Vc2WaveStore& waveStore() override;
    const Vc2WaveMemory& waveMemory() override;
    Vc2WaveSets& waveSets() override;
    // How many bytes the reads have reached that no region of the part holds (xt_vc2synth's)
    long unheld() const { return memory_.unheld(); }
    // v346: a word of the part's memory by rom.4's address (the OS MIDI's listeners' nodes)
    uint32_t wordAt(uint32_t at) { return rd32(at); }

protected:
    void read(uint32_t at, void* to, size_t bytes) override { memory_.read(at, to, bytes); }

private:
    // No waves given: a memory of 0s
    class NoWaves final : public Vc2WaveMemory {
    public:
        uint8_t byte(uint32_t) const override { return 0; }
    };
    template <class F>
    void onPool(const Vc2SetChild& c, F f);
    uint32_t* block(uint32_t noteAt);
    Vc2MatrixWords* matrixAt(uint32_t matrix);
    Vc2Synth& s_;
    Vc2SynthMemory memory_;
    Vc2MatrixChannel channel_ {};       // the channel state M names, read for its sources
    std::array<int16_t, 16> none_ {};   // the sums of an M the part doesn't hold
    NoWaves noWaves_ {};
};
