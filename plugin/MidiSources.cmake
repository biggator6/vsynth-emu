# The plugin's own MIDI ahead of the firmware's (v462 on; research/NEXT_ISSUES.md items 1 and 2), in the plugin's
# target: the editor's queue, rom.2's edits of the patch from a control change and the parameters they go onto.
# CMakeLists.txt includes this after Vc2UiSources.cmake, before the effect build reads the sources back; a new file of
# this part goes here.
target_sources(VSynthEmu PRIVATE
    Source/XtMidiEdits.cpp
    Source/PluginMidiEdits.cpp
    Source/PluginMidiIn.cpp
    Source/PluginMidiLearn.cpp
    Source/UiMidiLearn.cpp
    Source/UiMidiMap.cpp
    Source/XtSynthView.cpp
    Source/UiModRings.cpp
)
