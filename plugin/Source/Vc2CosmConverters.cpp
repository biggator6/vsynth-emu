// The VC-2's COSM converters the port has, by type: see Vc2CosmConverters.h (v237, v239, v241: every one).
#include "Vc2CosmConverters.h"
#include "Vc2CosmBalanced.h"
#include "Vc2CosmConvert.h"
#include "Vc2CosmDrive.h"
#include "Vc2CosmDynamics.h"
#include "Vc2CosmFilters.h"
#include "Vc2CosmGrowl.h"
#include "Vc2CosmSideBand.h"
#include "Vc2CosmTvf.h"
#include <array>
#include <cstring>
#include <type_traits>
#include <utility>

namespace {
using Convert = void (*)(const Vc2CosmInput&, uint32_t*, Vc2CosmMatrix&, const Vc2ToneShared&);

// A converter on the block's words through its layout: what it doesn't write stays as it was.  The shared math only
// for the converters that reach rom.4's libm (v239)
template <class Block, auto Converter>
void throughLayout(const Vc2CosmInput& in, uint32_t* block, Vc2CosmMatrix& matrix, const Vc2ToneShared& shared) {
    static_assert(sizeof(Block) <= kVc2CosmBlockWords * 4, "a block within a COSM's");
    Block b {};
    std::memcpy(&b, block, sizeof b);
    if constexpr (std::is_invocable_v<decltype(Converter), const Vc2CosmInput&, Block&, Vc2CosmMatrix&>)
        Converter(in, b, matrix);
    else
        Converter(in, b, matrix, shared);
    std::memcpy(block, &b, sizeof b);
}

// [FW] the dispatcher's types, 1…16, by the converter its switch calls
constexpr auto kConverters = std::to_array<std::pair<int, Convert>>({
    { 1, throughLayout<Vc2CosmOdBlock, vc2OdConvert> },
    { 2, throughLayout<Vc2CosmWsBlock, vc2WsConvert> },
    { 3, throughLayout<Vc2CosmAmpBlock, vc2AmpConvert> },
    { 5, throughLayout<Vc2CosmResonatorBlock, vc2ResonatorConvert> },
    { 6, throughLayout<Vc2CosmSbf1Block, vc2Sbf1Convert> },
    { 7, throughLayout<Vc2CosmSbf2Block, vc2Sbf2Convert> },
    { 8, throughLayout<Vc2CosmGrowlBlock, vc2GrowlConvert> },
    { 9, throughLayout<Vc2CosmDualBlock, vc2DualConvert> },
    { 10, throughLayout<Vc2CosmTvfBlock, vc2TvfConvert> },
    { 11, throughLayout<Vc2CosmTvfBlock, vc2DtvfConvert> },
    { 12, throughLayout<Vc2CosmCompBlock, vc2CompConvert> },
    { 13, throughLayout<Vc2CosmLimBlock, vc2LimConvert> },
    { 14, throughLayout<Vc2CosmShifterBlock, vc2ShifterConvert> },
    { 15, throughLayout<Vc2CosmLofiBlock, vc2LofiConvert> },
    { 16, throughLayout<Vc2CosmTbBlock, vc2TbConvert> },
});

Convert converterOf(int type) {
    for (const auto& [t, convert] : kConverters)
        if (t == type) return convert;
    return nullptr;
}
} // namespace

bool vc2CosmPortedConvert(int type, const Vc2CosmInput& in, uint32_t* block, Vc2CosmMatrix& matrix,
                          const Vc2ToneShared& shared) {
    const Convert convert = converterOf(type);
    if (convert == nullptr) return false;
    convert(in, block, matrix, shared);
    return true;
}
