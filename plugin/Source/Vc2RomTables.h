#pragma once
// A VC-2 port's tables from rom.4, the process's (v210, from Vc2Unvoiced.cpp's loader of v208): read once, under a lock,
// by the first image to load, published by an atomic flag, then read without one, as XtSynthRom.cpp's rule has it.
// `Tables` is a plain struct with `bool read(Sh4& image)`, which fills it from an image after its C startup (so .data
// reads as its initial values) and says whether what it read can be used.  loadVc2 (XtSynthRom.cpp) loads each at the
// start, never at a switch of modes.
#include "Sh4Interp.h"
#include "Vc2Math.h"
#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <mutex>
#include <span>
#include <utility>

template <class Tables>
class Vc2RomTables {
public:
    bool load(const char* path) {
        const std::lock_guard<std::mutex> hold(lock_);
        if (loaded()) return true;
        Sh4 image;
        if (!image.loadRom(path) || !t.read(image)) return false;
        loaded_.store(true, std::memory_order_release);
        return true;
    }
    bool loaded() const { return loaded_.load(std::memory_order_acquire); }
    Tables t {};   // written before loaded_, never after

private:
    std::mutex lock_;
    std::atomic<bool> loaded_ { false };
};

// A table of floats in the image: where it starts and how many it holds
struct Vc2Span {
    uint32_t at;
    int floats;
};

// A table's floats from the image
inline void vc2ReadFloats(Sh4& image, Vc2Span span, float* out) {
    for (int i = 0; i < span.floats; ++i) out[i] = std::bit_cast<float>(image.rd32(span.at + 4 * (uint32_t)i));
}

// A routine's pool word and the float it fills (v224: the readers' one loop, the carrier's, vocoder's and tracker's)
using Vc2PoolWord = std::pair<uint32_t, float*>;

// Pool words from the image, each into its float
inline void vc2ReadPool(Sh4& image, std::span<const Vc2PoolWord> words) {
    for (const auto& [at, out] : words) *out = std::bit_cast<float>(image.rd32(at));
}

// A port's tables of pool words alone and the shared math routines' pool (v225: the carrier's, kind 6's): `Words(t)`
// gives each word's address and the float of `t` it fills; one instance, read once as Vc2RomTables reads
template <class Tables, auto Words>
class Vc2PoolTables {
public:
    static bool load(const char* path) { return tables_.load(path); }
    static bool loaded() { return tables_.loaded(); }
    static const Tables& get() { return tables_.t; }

private:
    struct Read : Tables {
        bool read(Sh4& image) {
            vc2ReadPool(image, Words(*this));
            vc2ReadMathPool(image, this->math);
            return true;
        }
    };
    static inline constinit Vc2RomTables<Read> tables_;
};
