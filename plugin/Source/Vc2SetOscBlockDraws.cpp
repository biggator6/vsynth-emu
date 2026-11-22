// xt_vc2sets' eighth part's draws (v393, out of Vc2SetOscBlockCheck.cpp, v391, moved as they stood): the parts' OSC
// table, the glide keys and what they and the converters read, an input, and the refresh's records; from v393 the Harm
// Formant's bytes in their ranges, which EXTIN's converter reads, and the header's and systems' bytes it tests.
// XT_VC2SETS_MAIN only.
#ifdef XT_VC2SETS_MAIN
#include "Vc2SetOscBlockCheck.h"
#include <bit>
#include <iterator>

namespace {
constexpr uint32_t kRecordsAPart = 32;
constexpr uint32_t kAliasCancelAt = 9;   // [FW] the systems' Alias Cancel (+0x10209)
constexpr uint32_t kExtinSystemAt = 3, kExtinHeaderAt = 0x1e;   // [FW] the bytes EXTIN's converter reads (v393)
constexpr int kOscSums = 4, kLfoSums = 5;
constexpr uint32_t kWaveTypeAt = 0xa7;   // [FW] the input's wave type, PCM's start's (v394)

void bytesDrawn(uint8_t* b, uint32_t n, std::mt19937& rng) {
    for (uint32_t i = 0; i < n; ++i) b[i] = (uint8_t)rng();
}

// A setting's bytes in their ranges, by the record's offset: three depths ±63, a keyfollow (a halfword, ±200) and a
// choice of `choices` (now and then any byte)
struct SettingBytes {
    std::array<uint32_t, 3> depths;
    uint32_t followAt, choiceAt, choices;
};
// [FW] v393: the Harm Formant's, which EXTIN's converter reads (vc2params.py's IDs 0x371…0x375: its depth, keyfollow,
// LFO and envelope depths, its Style, the formant's row); v394: the Time's, which PCM's reads (and its mode)
constexpr SettingBytes kHarmFormant { { 0x4e, 0x52, 0x53 }, 0x50, 0x5e, 11 };
constexpr SettingBytes kPcmTime { { 0x44, 0x48, 0x49 }, 0x46, 0x5d, 3 };

void settingDrawn(uint8_t* r, std::mt19937& rng, const SettingBytes& s) {
    for (const uint32_t at : s.depths) r[at] = (uint8_t)((int)(rng() % 127) - 63);
    const auto follow = (int16_t)((int)(rng() % 401) - 200);
    std::memcpy(r + s.followAt, &follow, sizeof follow);
    r[s.choiceAt] = (uint8_t)(rng() % 8 ? rng() % s.choices : rng());
}

// A record: noise; the OSC on mostly, its type spread over the four and now and then any; the waveform one of the
// eleven, Pitch KF 0 or 100% now and then, a Vibrato Type of its four (now and then of its four bits), Random Pitch
// Depth none half the time, the Harm Formant's bytes in their ranges three times in four
void recordDrawn(uint8_t* r, std::mt19937& rng) {
    bytesDrawn(r, kOscRecordBytes, rng);
    r[0] = (uint8_t)(rng() % 4 ? 1 : rng() % 2 ? 0 : rng());
    const std::array<uint8_t, 6> types { 0, 0, 3, 1, 2, (uint8_t)rng() };
    r[1] = setsOneOf(types, rng);
    if (rng() % 4) r[0x20] = (uint8_t)(rng() % 11);
    if (rng() % 4 == 0) {
        const uint16_t kf = rng() % 2 ? 0 : 1000;
        std::memcpy(r + 6, &kf, sizeof kf);
    }
    r[0x1c] = (uint8_t)(rng() % 8 ? rng() % 4 : rng() % 16);
    if (rng() % 2) std::memset(r + 0x12, 0, 2);
    if (rng() % 4) settingDrawn(r, rng, kHarmFormant);
    if (rng() % 4) settingDrawn(r, rng, kPcmTime);
}

// A part's header: noise, mono and the portamento's byte mostly 0 or 1, the key's offset and coarse tune small, the
// byte EXTIN's converter tests 0 half the time
void headerDrawn(uint8_t* h, std::mt19937& rng) {
    bytesDrawn(h, kOscHeaderBytes, rng);
    h[4] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
    h[5] = (uint8_t)(rng() % 4 ? rng() % 3 : rng());
    h[0] = (uint8_t)(rng() % 4 ? (int)(rng() % 97) - 48 : (int)rng());
    h[1] = (uint8_t)(rng() % 4 ? (int)(rng() % 97) - 48 : (int)rng());
    if (rng() % 2) h[kExtinHeaderAt] = 0;   // v393: EXTIN's own words, which this byte set skips
}
} // namespace

void oscTableDrawn(Vc2Fw& fw, std::mt19937& rng, bool all) {
    for (uint32_t p = 0; p < kOscParts; ++p) {
        if (!all && rng() % 16) continue;
        std::array<uint8_t, kOscPartRow> row;
        headerDrawn(row.data(), rng);
        for (uint32_t k = 0; k < kRecordsAPart; ++k)
            recordDrawn(row.data() + kOscHeaderBytes + kOscRecordBytes * k, rng);
        fw.cpu.write(kOscTableAt + kOscPartRow * p, row.data(), sizeof row);
    }
    if (all || rng() % 8 == 0) {
        std::array<uint8_t, 60> sys;
        setsNoise(sys, rng);
        if (rng() % 2) sys[kAliasCancelAt] = 0;   // Alias Cancel off half the time, which noise nearly never is
        if (rng() % 2) sys[kExtinSystemAt] = 0;   // v393: EXTIN's +0xb0 flag either way
        fw.cpu.write(kOscSystemsAt, sys.data(), sizeof sys);
    }
}

void oscAroundDrawn(Vc2Fw& fw, std::mt19937& rng, bool all) {
    for (uint32_t j = 0; j < kOscObjects; ++j) {
        if (!(all || rng() % 4 == 0)) continue;
        std::array<uint8_t, 0x24> g;
        setsNoise(g, rng);
        const uint32_t table = kOscGlideTablesAt + kOscGlideTableBytes * j;
        std::memcpy(g.data(), &table, sizeof table);
        g[0x1b] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
        g[0x21] = (uint8_t)(rng() % 4 ? rng() % 2 : rng());
        fw.cpu.write(kOscGlidesAt + kOscGlideBytes * j, g.data(), sizeof g);
        fw.cpu.wr8(table + kOscHoldAt, setsByteDrawn<0, 63, 64, 127, 128>(rng));
        fw.cpu.wr32(kOscLegatosAt + kOscLegatoBytes * j, (uint32_t)rng());
        const uint32_t m = kSetsTvaMatricesAt + kSetsTvaMatrixBytes * j;
        fw.cpu.wr32(m + kOscBendAt, rng() % 2 ? 0 : rng() % 2 ? (uint32_t)(rng() % 4001) - 2000 : (uint32_t)rng());
        for (uint32_t b = 0; b < 4; ++b) fw.cpu.wr8(m + kOscBeamAt + b, (uint8_t)(rng() % 2 ? rng() % 128 : rng()));
    }
    for (uint32_t j = 0; j < kOscObjects; ++j)
        fw.cpu.wr32(kSetsVoicesAt + kSetsVoiceBytes * j + kSetsVoiceWords + kOscSourceWordAt,
                    kOscGlidesAt + kOscGlideBytes * (uint32_t)(rng() % kOscObjects));
    for (uint32_t k = 0; k < 4; ++k) {
        if (!(all || rng() % 4 == 0)) continue;
        for (int i = 0; i < (k < 2 ? kOscSums : kLfoSums); ++i) {
            const auto sums = std::to_array<int16_t>({ 0, (int16_t)(rng() % 141 - 70), (int16_t)(rng() % 26001 - 13000),
                                                       (int16_t)rng() });
            fw.cpu.wr16(kOscGroupsAt + kOscGroupBytes * k + 2 * i, (uint16_t)setsOneOf(sums, rng));
        }
    }
    if (all || rng() % 4 == 0) {
        const float source = rng() % 2 ? 1.0f : 0.25f + (float)(rng() % 3751) / 1000.0f;
        fw.cpu.wr32(kOscSourceAt, std::bit_cast<uint32_t>(source));
    }
}

uint32_t oscMatrixDrawn(std::mt19937& rng) {
    return kSetsTvaMatricesAt + kSetsTvaMatrixBytes * (uint32_t)(rng() % kOscObjects);
}

void oscInputDrawn(Vc2SetOscInput& w, std::mt19937& rng) {
    setsNoise(w, rng);
    w.part = (uint8_t)(rng() % kOscParts);
    w.kind = (uint8_t)(rng() % 4 ? rng() % 4 : rng() % 16);
    w.osc = (uint8_t)(w.kind > 3 || rng() % 8 ? rng() % 2 : rng() % 16);
    recordDrawn(w.record.data(), rng);
    w.matrix = oscMatrixDrawn(rng);
    // v394: the wave's type PCM's start would leave (+0xa7), one of its six mostly, as it calls by it
    reinterpret_cast<uint8_t*>(&w)[kWaveTypeAt] = (uint8_t)(rng() % 4 ? rng() % 6 : rng());
}

// Now and then an input's record put in the first four parts' tables at its kind and OSC, half the time with one of
// the bytes the refresh compares moved
void oscSameRecordsDrawn(Vc2Fw& fw, std::span<const Vc2SetOscInput> inputs, std::mt19937& rng) {
    constexpr uint32_t kCompared[] = { 1, 0x20, 0x3c, 0x5c, 0x59 };   // [FW] the type, waveform and PCM wave's words
    constexpr uint32_t kFewParts = 4;
    for (const Vc2SetOscInput& w : inputs) {
        if (rng() % 4) continue;
        std::array<uint8_t, kOscRecordBytes> r = w.record;
        if (rng() % 2) {
            const uint32_t at = kCompared[rng() % std::size(kCompared)];
            r[at] = (uint8_t)(r[at] ^ (1 + rng() % 255));
        }
        const uint32_t slot = kOscHeaderBytes + kOscKindRow * w.kind + kOscRecordBytes * w.osc;
        for (uint32_t p = 0; p < kFewParts; ++p) fw.cpu.write(kOscTableAt + kOscPartRow * p + slot, r.data(), sizeof r);
    }
}
#endif
