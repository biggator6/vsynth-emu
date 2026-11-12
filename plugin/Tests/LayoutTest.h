#pragma once
#include "../Source/PluginProcessor.h"

// TEST_UI_LAYOUT (LayoutTest.cpp, moved from PluginTest.cpp's main in v193): the editor's layout, then stop.  `ui` is the
// switch's value, "list" for the parameters with and without a control.  0 if it holds.
int uiLayoutTest(VSynthEmuProcessor& p, const char* ui);
