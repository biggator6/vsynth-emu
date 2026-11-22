// rom.4's parameters by its own IDs: see Vc2Params.h (v199).  Every address is rom.4's, its tables in rom.2's forms
// (research/VC2_VOCAL_DESIGNER.md, "The parameter tables"); its check is Vc2ParamsCheck.cpp's (xt_vc2params).
#include "Vc2Params.h"
#include "Sh4Interp.h"
#include "XtFxValueText.h"
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

namespace {

constexpr uint32_t kNames = 0x898b5798;         // [FW] a pointer an ID to its name, which has a space before it
constexpr uint32_t kDescriptors = 0x8950f0b0;   // [FW] the first bank: 12 bytes an ID
constexpr uint32_t kBank = 0x895131a8;          // [FW] the second: 8 bytes an ID from kVc2FirstTyped
constexpr uint32_t kStrings = 0x898b8154;       // [FW] a pointer an ID to its values' string, one table for every ID
constexpr uint32_t kValueText = 0x894fa070;     // [FW] (id, value, 1 to add the unit): rom.2's 0x89461a70, by rom.4's IDs
constexpr int kLongestName = 40, kLongestString = 4096;
constexpr size_t kDescBytes = 12, kTypedBytes = 8;

// rom.4's routine as it is: no ID run as another's format, each ID's own row of strings, no time by the driver.  What it
// lacks is for the VC-2's panel to settle (VC2_VOCAL_DESIGNER.md, "rom.4's values as text").
int asItIs(int id) { return id; }
bool noTime(int) { return false; }
constexpr XtTextImage kVc2Text { kValueText, kStrings, kStrings, 0, 0, asItIs, asItIs, noTime };

struct Tables {
    std::vector<std::string> names, strings;
    std::vector<XtParamDesc> descs;
    std::vector<XtTypedDesc> typed;
};

// `count` records of `bytes` each from `at` in the image, each by `decode`
template <typename T, typename Decode>
std::vector<T> records(Sh4& image, uint32_t at, int count, size_t bytes, Decode decode) {
    std::vector<uint8_t> raw(bytes * (size_t)count);
    image.read(at, raw.data(), raw.size());
    std::vector<T> out;
    for (size_t k = 0; k < raw.size(); k += bytes) out.push_back(decode(&raw[k]));
    return out;
}

// An ID's values' string as rom.4 holds it, its marks kept: names split by 1, a range by 2, the unit after a 3 (v453)
std::string valueString(Sh4& image, uint32_t at) {
    std::string t;
    for (int i = 0; i < kLongestString; ++i) {
        const char ch = (char)image.rd8(at + (uint32_t)i);
        if (!ch) break;
        t += ch;
    }
    return t;
}

// The process's: the tables written once, by the first load, and read with no lock once `loaded_` says so; the routine's
// interpreter behind the lock, as any instance's panel may ask
class Params {
public:
    bool load(const char* path);
    bool loaded() const { return loaded_.load(std::memory_order_acquire); }
    const Tables& tables() const { return tables_; }   // only once loaded()
    std::string text(int id, int value);

private:
    std::mutex lock_;                      // one load or text at a time; guards text_, and tables_ until loaded_
    std::atomic<bool> loaded_ { false };   // set once the tables are in, and never cleared
    Tables tables_;
    std::unique_ptr<XtFxValueText> text_;
};

bool Params::load(const char* path) {
    const std::lock_guard<std::mutex> hold(lock_);
    if (loaded()) return true;
    Sh4 image;
    if (!image.loadRom(path)) return false;
    Tables t;
    for (int id = 0; id < kVc2Ids; ++id) {
        t.names.push_back(xtFirmwareName(image, image.rd32(kNames + 4 * (uint32_t)id), kLongestName));
        if (t.names.back().empty()) return false;   // every ID has a name: an empty one is a table misread
        t.strings.push_back(valueString(image, image.rd32(kStrings + 4 * (uint32_t)id)));
    }
    t.descs = records<XtParamDesc>(image, kDescriptors, kVc2FirstTyped, kDescBytes, xtParamDescOf);
    t.typed = records<XtTypedDesc>(image, kBank, kVc2Ids - kVc2FirstTyped, kTypedBytes, xtTypedDescOf);
    auto text = std::make_unique<XtFxValueText>(path, kVc2Text);
    if (!text->ok()) return false;
    tables_ = std::move(t);
    text_ = std::move(text);
    loaded_.store(true, std::memory_order_release);
    return true;
}

std::string Params::text(int id, int value) {
    const std::lock_guard<std::mutex> hold(lock_);
    return text_ ? text_->text(id, value) : std::string();
}

constinit Params g_params;

} // namespace

bool vc2LoadParams(const char* path) { return g_params.load(path); }

namespace {
// The entry `at` of one of the tables, if it's in and has one
template <typename T> const T* entry(const std::vector<T> Tables::*table, int at) {
    if (!g_params.loaded() || at < 0) return nullptr;
    const std::vector<T>& t = g_params.tables().*table;
    return (size_t)at < t.size() ? &t[(size_t)at] : nullptr;
}
} // namespace

const XtParamDesc* vc2ParamDesc(int id) { return entry(&Tables::descs, id); }

const XtTypedDesc* vc2TypedDesc(int id) { return entry(&Tables::typed, id - kVc2FirstTyped); }

std::string vc2ParamName(int id) {
    const std::string* name = entry(&Tables::names, id);
    return name ? *name : std::string();
}

std::string vc2ValueString(int id) {
    const std::string* string = entry(&Tables::strings, id);
    return string ? *string : std::string();
}

std::string vc2ValueText(int id, int value) { return g_params.loaded() ? g_params.text(id, value) : std::string(); }
