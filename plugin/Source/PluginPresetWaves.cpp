// The V-Synth's preset waves a project's link reaches, once a process: see PluginPresetWaves.h (v600).
#include "PluginPresetWaves.h"
#include "XtEncoded.h"
#include "XtFirmware.h"
#include "XtPresetWaves.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace {
// Where the unpacker leaves it, beside both images (research/ghidra_scripts/unpack_firmware.py; PluginRom.cpp's name)
constexpr const char* kDiskImageName = "V-SynthXTDiskImage.bin.0.bin";

class PresetWavesInUse {
public:
    constexpr PresetWavesInUse() = default;
    PresetWavesInUse(const PresetWavesInUse&) = delete;
    PresetWavesInUse& operator=(const PresetWavesInUse&) = delete;

    bool read(int number, XtWaveFile& wave, std::string* why) {
        std::vector<uint8_t> stream;
        {
            const std::lock_guard<std::mutex> hold(lock_);
            if (!found(why)) return false;
            const XtPresetWave* w = waves_.wave(number);
            if (!w) return xtfile::fail(why, "no such preset wave in the V-Synth's library");
            if (!streamOf(*w, stream)) return xtfile::fail(why, "the disk image's stream can't be read");
        }
        return xtWaveRead(stream.data(), stream.size(), wave, why);
    }

private:
    // The image, sought once for each pair of images in (rom.2's and rom.4's paths): beside each in turn
    bool found(std::string* why) {
        const std::array<std::string, 2> beside { xtFwRomPath(), xtSynthVc2RomPath() };
        if (beside != soughtBeside_) {
            soughtBeside_ = beside;
            path_.clear();
            for (const std::string& rom : beside) {
                if (!path_.empty() || rom.empty()) continue;
                take((std::filesystem::path(rom).parent_path() / kDiskImageName).string());
            }
        }
        return !path_.empty() || xtfile::fail(why, "no disk image with the V-Synth's preset waves beside the firmware");
    }

    void take(const std::string& image) {
        XtDiskImage disk;
        if (disk.open(image) && waves_.build(disk)) path_ = image;
    }

    // A wave's stream, its bytes alone, from the image's file
    bool streamOf(const XtPresetWave& w, std::vector<uint8_t>& out) const {
        std::ifstream f(path_, std::ios::binary);
        out.resize(w.bytes);
        return f.seekg(static_cast<std::streamoff>(w.at))
               && f.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(w.bytes));
    }

    std::mutex lock_;                            // one load at a time; guards the three below
    std::array<std::string, 2> soughtBeside_;    // the images' paths the image was last sought beside
    std::string path_;                           // the image taken, empty for none
    XtPresetWaves waves_;                        // its waves' places
};

constinit PresetWavesInUse g_presets;
} // namespace

bool presetWaveRead(int number, XtWaveFile& wave, std::string* why) { return g_presets.read(number, wave, why); }
