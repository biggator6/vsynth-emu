// The XT's own event edits, run in the interpreter (XtFwEvents.h, v481)
#include "XtFwEvents.h"
#include "Sh4Interp.h"
#include "XtFwAddrs.h"
#include "XtFwMachine.h"

#include <algorithm>
#include <array>
#include <utility>

using namespace xtfw;       // the machine's stack (XtFwMachine.h)
using namespace xtfwaddr;   // rom.2's entry points (XtFwAddrs.h)

namespace {

thread_local XtFwEventScope* g_scope = nullptr;

// The screen's own mapping of its routine's raw result to its message's number (0x894a91c0's for an add, 0x894a9250's
// for a delete), {raw, result}: done for any not below 0, the ones listed theirs, any other "Cannot Delete This Event!"
constexpr std::array<std::pair<int, int>, 2> kAddResults { { { -13, kEventTooNarrow }, { -11, kEventMemoryFull } } };
constexpr std::array<std::pair<int, int>, 1> kDeleteResults { { { -17, kEventNotHere } } };

template <size_t N>
int screenResult(int raw, const std::array<std::pair<int, int>, N>& known) {
    if (raw >= 0) return kEventDone;
    for (const auto& [from, to] : known)
        if (from == raw) return to;
    return kEventCantDelete;
}

// [MAN XT, the Message List] in the results' order from kEventCantDelete down
constexpr std::array<const char*, 4> kMessages { "Cannot Delete This Event!", "event memory full",
                                                 "Event Interval Too Narrow!", "Event Doesn't Exist Here!" };

} // namespace

const char* xtFwEventMessage(int result) {
    const int k = -result - 1;
    return k >= 0 && k < (int)kMessages.size() ? kMessages[(size_t)k] : "";
}

XtFwEventScope::XtFwEventScope(const std::vector<XtFwEventEdit>& e, std::vector<int>& r)
    : edits(e), results(r), outer_(g_scope) {
    g_scope = this;
}

XtFwEventScope::~XtFwEventScope() {
    g_scope = outer_;
}

XtFwEventScope* XtFwEventScope::current() {
    return g_scope;
}

bool xtFwEditEvents(Sh4& c, long frames, int depth) {
    XtFwEventScope* s = XtFwEventScope::current();
    if (s == nullptr) return true;
    s->results.clear();
    const uint32_t counted = (uint32_t)std::clamp(depth, 0, 127);
    bool taken = false;
    for (const XtFwEventEdit& e : s->edits) {
        // the delete's screen holds a frame at the end or past it to the frames, which the routine refuses (0x894a9250)
        const uint32_t at = !e.add && (long)e.at + 1 >= frames ? (uint32_t)frames : e.at;
        uint32_t rc = 0;
        const int raw = c.call(e.add ? kEventAdd : kEventDelete, { at, 0 }, kStack, &rc) ? (int)rc : -1;
        const int result = e.add ? screenResult(raw, kAddResults) : screenResult(raw, kDeleteResults);
        s->results.push_back(result);
        // taken: the screen marks its list stale (+0xc) and its refresh counts the markers again at the depth
        if (result == kEventDone && !c.call(kDetect, { counted }, kStack, &rc)) return false;
        taken = taken || result == kEventDone;
    }
    // Execute's encode start (0x89516720) commits the list before its driver: the deleted unlinked and the count the
    // driver takes set (0x89512d00); a list of more events than the length holds refused, as the XT refuses to encode it
    uint32_t events = 0;
    return !taken || (c.call(kEventsCommit, {}, kStack, &events) && (int32_t)events >= 0);
}
