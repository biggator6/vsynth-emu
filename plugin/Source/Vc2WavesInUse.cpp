// The VC-2's waves the process plays from (Vc2WavesInUse.h, v504).
#include "Vc2WavesInUse.h"
#include "Sh4Interp.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace {

bool fail(std::string* why, const char* what) {
    if (why) *why = what;
    return false;
}

// The one load: its disk image and files while it runs, then the waves.  One object, constinit as XtSynthRom.cpp's
// images are, so it's there before any instance asks.  Its destructor, at the process's end (or a plugin's unloading),
// stops the load and waits for its thread.
class WavesInUse {
public:
    constexpr WavesInUse() = default;
    WavesInUse(const WavesInUse&) = delete;
    WavesInUse& operator=(const WavesInUse&) = delete;
    ~WavesInUse() {
        stop_.store(true, std::memory_order_relaxed);
        if (thread_ && thread_->joinable()) thread_->join();
    }

    bool take(const std::string& path, const std::string& rom4, std::string* why) {
        const std::lock_guard<std::mutex> hold(lock_);
        if (taken_) return true;
        auto disk = std::make_unique<XtDiskImage>();
        if (!disk->open(path)) return fail(why, "not a disk image");
        auto files = std::make_unique<Vc2DiskBootFiles>(*disk);
        if (files->index().empty() || files->bank().empty())
            return fail(why, "no VC-2 wave index or sample bank in it");
        disk_ = std::move(disk);
        files_ = std::move(files);
        path_ = path;
        status_ = "loading from " + path;
        taken_ = true;
        thread_ = std::make_unique<std::thread>([this, rom4] { load(rom4); });
        return true;
    }

    bool taken() {
        const std::lock_guard<std::mutex> hold(lock_);
        return taken_;
    }

    const Vc2LoadedWaves* waves() const { return ready_.load(std::memory_order_acquire); }

    std::string status() {
        const std::lock_guard<std::mutex> hold(lock_);
        return taken_ ? status_ : "no disk image taken";
    }

    bool wait(double seconds) {
        std::unique_lock<std::mutex> hold(lock_);
        return ended_.wait_for(hold, std::chrono::duration<double>(seconds), [this] { return !taken_ || done_; });
    }

private:
    // On the load's thread: rom.4's load on the files taken, then the 65 MB of disk image let go
    void load(const std::string& rom4) {
        const Sh4StopScope scope(stop_);
        auto waves = std::make_unique<Vc2LoadedWaves>();
        std::string why;
        const bool loaded = vc2LoadWaves(rom4, *files_, *waves, &why);
        const std::lock_guard<std::mutex> hold(lock_);
        files_.reset();
        disk_.reset();
        status_ = loaded ? "loaded from " + path_ : "the load from " + path_ + " failed: " + why;
        if (loaded) waves_ = std::move(waves);
        ready_.store(waves_.get(), std::memory_order_release);
        done_ = true;
        ended_.notify_all();
    }

    std::mutex lock_;   // guards all but the stop flag and the published waves
    std::condition_variable ended_;
    std::unique_ptr<std::thread> thread_;
    std::atomic<bool> stop_ { false };
    std::atomic<const Vc2LoadedWaves*> ready_ { nullptr };
    std::unique_ptr<XtDiskImage> disk_;
    std::unique_ptr<Vc2DiskBootFiles> files_;
    std::unique_ptr<Vc2LoadedWaves> waves_;
    std::string path_, status_;
    bool taken_ = false, done_ = false;
};

constinit WavesInUse g_waves;

} // namespace

bool vc2TakeWaves(const std::string& path, const std::string& rom4, std::string* why) {
    return g_waves.take(path, rom4, why);
}

bool vc2WavesTaken() { return g_waves.taken(); }

const Vc2LoadedWaves* vc2Waves() { return g_waves.waves(); }

std::string vc2WavesStatus() { return g_waves.status(); }

bool vc2WavesWait(double seconds) { return g_waves.wait(seconds); }
