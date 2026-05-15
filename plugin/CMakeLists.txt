cmake_minimum_required(VERSION 3.22)
project(VSynthEmu VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── JUCE ────────────────────────────────────────────────────────────────────
# Assumes JUCE is either a sibling directory or installed via FetchContent.
# Option 1 (recommended for dev): clone JUCE alongside this project
#   git clone https://github.com/juce-framework/JUCE.git ../JUCE
# Option 2: FetchContent (uncomment below)

# include(FetchContent)
# FetchContent_Declare(JUCE
#     GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
#     GIT_TAG        7.0.9
# )
# FetchContent_MakeAvailable(JUCE)

# Local JUCE path (adjust if needed)
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../JUCE"
    CACHE PATH "Path to JUCE framework")

if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
    message(FATAL_ERROR
        "JUCE not found at ${JUCE_PATH}. "
        "Clone JUCE alongside this directory or set JUCE_PATH.")
endif()

add_subdirectory(${JUCE_PATH} JUCE)

# ─── Plugin Target ───────────────────────────────────────────────────────────
juce_add_plugin(VSynthEmu
    COMPANY_NAME            "YourStudio"
    PLUGIN_MANUFACTURER_CODE Ystu
    PLUGIN_CODE             VsEm
    FORMATS                 VST3 AU Standalone
    PRODUCT_NAME            "V-Synth Emu"

    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    COPY_PLUGIN_AFTER_BUILD TRUE
)

# ─── Sources ─────────────────────────────────────────────────────────────────
target_sources(VSynthEmu PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/VariphraseEngine.cpp
    Source/PhaseVocoder.cpp
    Source/OfflineRenderer.cpp
)

# ─── JUCE Modules ────────────────────────────────────────────────────────────
target_link_libraries(VSynthEmu PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

target_compile_definitions(VSynthEmu PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ─── Optional: Python bindings for testing (pybind11) ────────────────────────
# Uncomment when you want to call VariphraseEngine from Python for batch tests.
# find_package(pybind11 REQUIRED)
# pybind11_add_module(variphrase_py Source/VariphraseEngine.cpp Source/PhaseVocoder.cpp
#                                   Source/pybind11_bindings.cpp)
# target_link_libraries(variphrase_py PRIVATE juce::juce_audio_basics juce::juce_dsp)
