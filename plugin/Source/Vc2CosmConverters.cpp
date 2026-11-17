// The VC-2's COSM converters the port has, by type: see Vc2CosmConverters.h (v237).
#include "Vc2CosmConverters.h"
#include "Vc2CosmConvert.h"
#include "Vc2CosmDynamics.h"
#include "Vc2CosmFilters.h"
#include "Vc2CosmGrowl.h"
#include "Vc2CosmTvf.h"
#include <array>
#include <cstring>
#include <utility>

namespace {
using Convert = void (*)(const Vc2CosmInput&, uint32_t*, Vc2CosmMatrix&);

// A converter on the block's words through its layout: what it doesn't write stays as it was
template <class Block, void (*Converter)(const Vc2CosmInput&, Block&, Vc2CosmMatrix&)>
void onBlock(const Vc2CosmInput& in, uint32_t* block, Vc2CosmMatrix& matrix) {
    static_assert(sizeof(Block) <= kVc2CosmBlockWords * 4, "a block within a COSM's");
    Block b {};
    std::memcpy(&b, block, sizeof b);
    Converter(in, b, matrix);
    std::memcpy(block, &b, sizeof b);
}

// [FW] the dispatcher's types, 1…16, by the converter its switch calls
constexpr auto kConverters = std::to_array<std::pair<int, Convert>>({
    { 3, onBlock<Vc2CosmAmpBlock, vc2AmpConvert> },
    { 8, onBlock<Vc2CosmGrowlBlock, vc2GrowlConvert> },
    { 9, onBlock<Vc2CosmDualBlock, vc2DualConvert> },
    { 10, onBlock<Vc2CosmTvfBlock, vc2TvfConvert> },
    { 11, onBlock<Vc2CosmTvfBlock, vc2DtvfConvert> },
    { 12, onBlock<Vc2CosmCompBlock, vc2CompConvert> },
    { 13, onBlock<Vc2CosmLimBlock, vc2LimConvert> },
    { 16, onBlock<Vc2CosmTbBlock, vc2TbConvert> },
});

Convert converterOf(int type) {
    for (const auto& [t, convert] : kConverters)
        if (t == type) return convert;
    return nullptr;
}
} // namespace

bool vc2CosmPortedConvert(int type, const Vc2CosmInput& in, uint32_t* block, Vc2CosmMatrix& matrix) {
    const Convert convert = converterOf(type);
    if (convert == nullptr) return false;
    convert(in, block, matrix);
    return true;
}
