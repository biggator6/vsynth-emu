#pragma once
// The XT's own event edits on its SAMPLE Encode screen (v481; HARDWARE_FIRMWARE.md {#wave-edit-read}, the user's d-9):
// an event added at a frame, or the one there deleted, run in the interpreter on the markers the attack detection has
// just left, before the encode's driver, as the screen's add (0x894a91c0 → 0x89516b20) and delete (0x894a9250 →
// 0x89516c80) do them; each one's result the number of the screen's message for it (0x893820c0).
#include <cstdint>
#include <vector>

struct XtFwEventEdit {
    bool add = true;   // one added there, or the one there deleted
    uint32_t at = 0;   // its frame
    bool operator==(const XtFwEventEdit&) const = default;
};

// What an edit came to
enum XtFwEventResult : int {
    kEventDone = 1,
    kEventCantDelete = -1,   // "Cannot Delete This Event!" (the start, the end, or a frame outside the wave)
    kEventMemoryFull = -2,   // "event memory full"
    kEventTooNarrow = -3,    // "Event Interval Too Narrow!"
    kEventNotHere = -4,      // "Event Doesn't Exist Here!"
};
// The XT's message for a result, empty for one done
const char* xtFwEventMessage(int result);

// The edits the encodes on this thread put on the detection's markers (xtFwEncodeSolo and xtFwEncodeMulti, given a
// depth and no markers), and where each one's result goes, in order, while the scope stands
class XtFwEventScope {
public:
    XtFwEventScope(const std::vector<XtFwEventEdit>& edits, std::vector<int>& results);
    ~XtFwEventScope();
    XtFwEventScope(const XtFwEventScope&) = delete;
    XtFwEventScope& operator=(const XtFwEventScope&) = delete;
    static XtFwEventScope* current();
    const std::vector<XtFwEventEdit>& edits;
    std::vector<int>& results;

private:
    XtFwEventScope* outer_;
};

// The scope's edits put on the markers in `c` (the detection's at `depth`, just left), each as the screen's routine
// does, their results into the scope's, and after each one taken the list counted again at the depth as the screen's
// refresh does (0x894a9020 → 0x894ab1b0 → 0x895164c0, which with the analysis made only counts it again, 0x89516666);
// with any taken, the list committed as Execute's encode start does before its driver (0x89516720 → 0x89512d00: the
// deleted unlinked, the driver's count set). Nothing with no scope (the encodes' own call, XtFwEncode.cpp); false if a
// call fails or the commit refuses the list (more events than the length holds)
class Sh4;
bool xtFwEditEvents(Sh4& c, long frames, int depth);
