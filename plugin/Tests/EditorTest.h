#pragma once
// VSynthEmuEditorTest's parts (EditorTest.cpp): the rack's checks, EditorRackTest.cpp (UI step 2)
#include "LayoutCheck.h"
#include "TestChecks.h"
#include "../Source/UiEditor.h"

// Every instance's panel, on every face of its Type: its table's knobs all there, and its controls moving its own
// instance's parameters alone; a press on each panel's name selecting its block
void rackChecks(LayoutCheck& l, SynthEditor& ed, VSynthEmuProcessor& p, TestChecks& check);
