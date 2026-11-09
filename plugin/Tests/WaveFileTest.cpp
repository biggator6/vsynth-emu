// A V-Synth wave whose header claims more than its data holds (R2 of .claude/claude-packs-review.md).  Every real wave
// has at least frames x channels bytes of data (745 measured 2026-11-09: the disk image's 416 presets and 329 waves of
// two projects; the most over it is 30, the padding of a last block of 16 frames), so one with fewer is damaged or
// made up.  Read as it was, a 100-byte RDAC wave claiming 64M stereo frames took 10.3 s and 268 MB and read "ok", with
// silence where its data wasn't, and a count past INT_MAX went negative in the plugin's int-sized buffers.
// Run: VSynthEmuWaveFileTest <rom.2.bin>.  Exit 0 if every check holds.
#include "../Source/XtFiles.h"
#include "../Source/XtFirmware.h"
#include <juce_core/juce_core.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <future>
#include <string>
#include <vector>

namespace {

const std::string kShort = "fewer samples than the header says", kLong = "more samples than a wave can have";

void put32(std::vector<uint8_t>& v, uint32_t x) {
    for (int k = 0; k < 4; ++k) v.push_back((uint8_t)(x >> (8 * k)));
}

// An RDAC wave's chunks, claiming `frames`, with `samples` bytes after the data's 8-byte header (one byte a sample).
std::vector<uint8_t> rdacWave(uint32_t frames, bool stereo, uint32_t samples) {
    std::vector<uint8_t> file;
    const auto chunk = [&file](const char* id, uint32_t bytes) {
        file.insert(file.end(), id, id + 4);
        put32(file, bytes);
    };
    chunk("vpfl", 44);
    file.resize(file.size() + 44, 0);
    std::memcpy(&file[file.size() - 4], "RDAC", 4);
    chunk("vpsc", 8);
    const uint8_t sc[4] = { 0, 1, (uint8_t)(stereo ? 1 : 0), 0 };   // +1 SOLO, +2 stereo
    file.insert(file.end(), sc, sc + 4);
    put32(file, frames);
    chunk("data", 8 + samples);
    const uint8_t head[8] = { 0, 8 };                                 // what 0x8952ee80 checks
    file.insert(file.end(), head, head + 8);
    file.resize(file.size() + samples, 0);
    return file;
}

struct Checks {
    int count = 0, failed = 0;
    void operator()(bool ok, const std::string& what) {
        ++count;
        if (!ok) {
            ++failed;
            std::fprintf(stderr, "wave file: %s\n", what.c_str());
        }
    }
};

// At the edge, on both sides of it, mono and stereo; then the review's file.
void edges(Checks& check) {
    XtWaveFile w;
    std::string why;
    const auto mono = rdacWave(32, false, 32), monoOver = rdacWave(33, false, 32);
    const auto stereo = rdacWave(16, true, 32), stereoOver = rdacWave(16, true, 31);
    check(xtWaveRead(mono.data(), mono.size(), w, &why) && w.pcm.size() == 32,
          "32 frames on 32 bytes should read: " + why);
    check(!xtWaveRead(monoOver.data(), monoOver.size(), w, &why) && why == kShort, "33 frames on 32 bytes: " + why);
    check(xtWaveRead(stereo.data(), stereo.size(), w, &why) && w.pcm.size() == 32,
          "16 stereo frames on 32 bytes should read: " + why);
    check(!xtWaveRead(stereoOver.data(), stereoOver.size(), w, &why) && why == kShort, "16 stereo on 31 bytes: " + why);
    const auto made = rdacWave(64u << 20, true, 16);
    const auto t0 = std::chrono::steady_clock::now();
    const bool read = xtWaveRead(made.data(), made.size(), w, &why);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    check(!read && why == kShort && ms < 100.0,
          "64M stereo frames on 16 bytes should be refused at once: " + why + ", " + std::to_string((int)ms) + " ms");
}

// 2^31 mono frames with the data to match, past what an int counts: a sparse file of 2 GB, mapped.  Refused before any
// decoding; read as it was, it would decode for minutes, so a run that hasn't come back in 3 s fails and exits.
void pastInt(Checks& check, const juce::File& folder) {
    const uint32_t frames = 0x80000000u;
    auto head = rdacWave(frames, false, 0);
    const size_t dataSize = head.size() - 12;                                       // where the data chunk's size is
    head[dataSize] = 8;
    head[dataSize + 3] = 0x80;                                                       // 8 + 2^31
    const juce::File file = folder.getChildFile("past_int.vpw");
    if (!file.replaceWithData(head.data(), head.size())) { check(false, "can't write the sparse file"); return; }
    std::error_code err;
    std::filesystem::resize_file(file.getFullPathName().toStdString(), head.size() + frames, err);
    juce::MemoryMappedFile mapped(file, juce::MemoryMappedFile::readOnly);
    if (err || mapped.getData() == nullptr) { check(false, "can't map the sparse file: " + err.message()); return; }
    XtWaveFile w;
    std::string why;
    const auto* bytes = static_cast<const uint8_t*>(mapped.getData());
    auto run = std::async(std::launch::async, [&] { return xtWaveRead(bytes, mapped.getSize(), w, &why); });
    if (run.wait_for(std::chrono::seconds(3)) != std::future_status::ready) {
        std::fprintf(stderr, "wave file: 2^31 frames on their data should be refused at once, not decoded\n");
        std::fflush(stderr);
        std::_Exit(1);   // the decode can't be stopped, and the future would wait for it
    }
    check(!run.get() && why == kLong, "2^31 frames on their data: " + why);
    file.deleteFile();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2 || !xtFwOpen(argv[1])) {
        std::fprintf(stderr, "usage: %s rom.2.bin\n", argc > 0 ? argv[0] : "");
        return 1;
    }
    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    const juce::File folder = temp.getNonexistentChildFile("vse-wavetest", "");
    folder.createDirectory();
    Checks check;
    edges(check);
    pastInt(check, folder);
    folder.deleteRecursively();
    std::printf("wave file: %d/%d checks hold\n", check.count - check.failed, check.count);
    return check.failed ? 1 : 0;
}
