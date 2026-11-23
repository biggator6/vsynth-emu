#pragma once
// rom.4's (the VC-2's) parameters by its own IDs (v199): each ID's name, its descriptor in either bank, and its values'
// text by rom.4's own routine.  Read from the image once, at the start (loadVc2, XtSynthRom.cpp; the user's decision,
// 2026-11-14: a switch of modes reads nothing), and the process's from then.  rom.4's IDs are its own: from 0x37 they
// are not rom.2's (research/VC2_VOCAL_DESIGNER.md, "The parameter tables").  Nothing is answered until they're in.
#include "XtFiles.h"
#include <string>

constexpr int kVc2Ids = 0x9ac;          // [FW] the names table's IDs, both banks (2476)
constexpr int kVc2FirstTyped = 0x56a;   // [FW] the second bank's first ID

// rom.4's tables from its image at `path`: true once they're in, and from then with nothing read again
[[nodiscard]] bool vc2LoadParams(const char* path);
// An ID's descriptor: the first bank's below kVc2FirstTyped, the second's from it; nullptr for the other bank's IDs,
// past the table, or before the tables are in
const XtParamDesc* vc2ParamDesc(int id);
const XtTypedDesc* vc2TypedDesc(int id);
// As the screens show it: up to a mark for the manual's footnotes, its spaces round it gone; empty past the table
std::string vc2ParamName(int id);
// The ID's values' string as rom.4 holds it, which its routine formats a value by: names split by a 1, a range's ends by
// a 2, the unit after a 3 (the editor's controls' kind and text, v453: UiVc2Text.cpp).  Empty before the tables are in.
std::string vc2ValueString(int id);
// A value's text by rom.4's routine, as XtFxValueText gives rom.2's: the routine's format, and the ID's unit after a bare
// number.  `value` within the ID's range, as the store's read gives it.  The routine has no format for many of the
// VC-2's own IDs and faults on a few values; those read as their number (VC2_VOCAL_DESIGNER.md, "rom.4's values as
// text").  Empty before the tables are in.
std::string vc2ValueText(int id, int value);
