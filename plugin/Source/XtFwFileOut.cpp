// A firmware writer run with the OS's file calls hooked: XtFwFileOut.h (v482).
#include "XtFwFileOut.h"
#include "XtFwMachine.h"
#include <cstring>

namespace xtfw {

namespace {
// [FW] rom.2's OS calls a writer makes: open, write, close, and the remove it calls when a write comes up short
constexpr uint32_t kOpen = 0x89484850, kWrite = 0x89484930, kClose = 0x89486d80, kRemove = 0x89484a60;
constexpr uint32_t kHandle = 3, kWritten = 1;   // the handle the open gives; the writer's answer when it wrote all
} // namespace

bool runWriter(Sh4& c, const Writer& w, std::vector<uint8_t>& file, std::string* why) {
    file.clear();
    std::vector<uint8_t> written;
    bool removed = false;
    c.hook(kOpen, [](Sh4& s) { s.ret(kHandle); });
    c.hook(kWrite, [&written](Sh4& s) {   // (handle, bytes, count): all of them taken
        std::vector<uint8_t> bytes(s.r[6]);
        s.read(s.r[5], bytes.data(), bytes.size());
        written.insert(written.end(), bytes.begin(), bytes.end());
        s.ret(s.r[6]);
    });
    c.hook(kClose, [](Sh4& s) { s.ret(0); });
    c.hook(kRemove, [&removed](Sh4& s) {
        removed = true;
        s.ret(0);
    });
    c.write(w.pathAt, w.path, std::strlen(w.path) + 1);
    uint32_t rc = 0;
    if (!c.call(w.at, { w.pathAt, w.arg }, kStack, &rc)) {
        if (why) *why = "the firmware's writer failed: " + c.error();
        return false;
    }
    if (removed || rc != kWritten) {
        if (why) *why = "the firmware's writer gave the file up";
        return false;
    }
    file = std::move(written);
    return true;
}

} // namespace xtfw
