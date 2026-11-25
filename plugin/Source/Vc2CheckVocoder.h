#pragma once
// The vocoder object's words against rom.4's, for any check that holds one (v345, xt_vc2vocoder's until then, when
// xt_vc2synth's load came to need it with an engine playing): its words, the engine's and the hook's words by the
// port's engine, the edit words, both engines and the stages.  The checks' builds only.
#include "Vc2CheckWords.h"
#include "Vc2Vocoder.h"

// [FW] the vocoder object, its engine's and edit words, its two engines, the hook's word and each engine's routine
inline constexpr uint32_t kVc2VocoderAt = 0x89999758, kVc2VocoderEngineAt = kVc2VocoderAt + 0x20;
inline constexpr uint32_t kVc2VocoderHiresoAt = kVc2VocoderAt + 0x24, kVc2VocoderLegacyAt = kVc2VocoderAt + 0x2d94;
inline constexpr uint32_t kVc2VocoderEditAt = 0x8994025c, kVc2VocoderHookAt = 0x899d9d8c;
inline constexpr uint32_t kVc2HiresoHook = 0x893ed420, kVc2LegacyHook = 0x893f0480;

inline std::string vc2VocoderDifference(Vc2Fw& fw, const Vc2Vocoder& v) {
    std::string what = vc2FirstDifference(fw, kVc2VocoderAt, &v.w, sizeof v.w, "the vocoder's words");
    const bool hireso = v.engine == &v.hireso;
    const uint32_t engine = v.engine == nullptr ? 0 : hireso ? kVc2VocoderHiresoAt : kVc2VocoderLegacyAt;
    if (what.empty() && fw.cpu.rd32(kVc2VocoderEngineAt) != engine) what = "the engine's word";
    const uint32_t hook = hireso ? kVc2HiresoHook : kVc2LegacyHook;
    if (what.empty() && fw.cpu.rd32(kVc2VocoderHookAt) != hook) what = "the hook's word";
    if (what.empty()) what = vc2FirstDifference(fw, kVc2VocoderEditAt, &v.edit, sizeof v.edit, "the edit words");
    if (what.empty()) what = vc2HiresoDifference(fw, kVc2VocoderHiresoAt, v.hireso);
    if (what.empty()) what = vc2LegacyDifference(fw, kVc2VocoderLegacyAt, v.legacy);
    if (what.empty()) what = vc2UnvoicedDifference(fw, v.stage);
    return what;
}
