// The VC-2 part's conversion sets' COSM inputs: see Vc2SetCosmInput.h (v390).  The constructor's writes in rom.4's
// order; the dispatcher and its converters the ported ones, on the reads' Matrix readers and shared routines.
#include "Vc2SetCosmInput.h"
#include "Vc2CosmConvert.h"
#include "Vc2CosmConverters.h"

namespace {
constexpr uint32_t kReaders[2] = { 0x893a2960, 0x893a2aa0 };   // [FW] COSM 1's and COSM 2's Matrix readers

// The converters' Matrix sums: the reader the input's COSM names, through the reads, once a sum as rom.4 calls it
class SetCosmMatrix final : public Vc2CosmMatrix {
public:
    explicit SetCosmMatrix(Vc2SetReads& reads) : reads_(reads) {}
    const int16_t* sums(bool second, uint32_t matrix) override {
        return reads_.matrixGroup(kReaders[second ? 1 : 0], matrix);
    }

private:
    Vc2SetReads& reads_;
};

// The dispatcher's converters: every type its switch calls is ported
class SetCosmTypes final : public Vc2CosmTypes {
public:
    SetCosmTypes(Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) : matrix_(matrix), shared_(shared) {}
    void convert(int type, const Vc2CosmInput& in, uint32_t* block) override {
        (void)vc2CosmPortedConvert(type, in, block, matrix_, shared_);
    }

private:
    Vc2CosmMatrix& matrix_;
    const Vc2ToneShared& shared_;
};
} // namespace

void Vc2SetCosmInputs::blockOf(Vc2CosmInput& w, uint32_t* block, Vc2SetReads& reads) {
    SetCosmMatrix matrix(reads);
    SetCosmTypes types(matrix, reads.shared());
    vc2CosmConvert(w, block, types, reads.shared());
}

void Vc2SetCosmInputs::construct(const Vc2SetChildCall& c, uint32_t* block, Vc2SetReads& reads) {
    Vc2CosmInput& w = at(c.object);
    uint8_t* bytes = vc2ChildBytes(w);
    vc2ChildMade(bytes, c, reads);
    w.matrix = c.matrix;
    vc2PartWords(bytes, reads.cosmInputOf(vc2CallsPart(c.call, reads), c.kind, c.index));
    blockOf(w, block, reads);
}
