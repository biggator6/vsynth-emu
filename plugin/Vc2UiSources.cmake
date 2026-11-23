# The VC-2 mode's pages in the editor, D's parts made from the VC-2's tables (v453; research/UI_DESIGN_VC2.md), in the
# plugin's target: CMakeLists.txt includes this after Vc2Sources.cmake.  A VC-2 page's new file goes here.
target_sources(VSynthEmu PRIVATE
    Source/UiVc2Text.cpp
    Source/UiVc2Kinds.cpp
    Source/UiVc2Details.cpp
    Source/UiVc2Rack.cpp
    Source/UiVc2Mic.cpp
    Source/UiVc2SynthPage.cpp
    Source/UiVc2Pages.cpp
    Source/UiVc2VocoderPage.cpp
    Source/UiVc2FxPage.cpp
)
