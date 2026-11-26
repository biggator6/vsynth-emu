# The plugin's effect build (v462): VSynthEmu's processor and editor again, as an effect that takes MIDI, so a host runs
# it on its track's audio and plays what it puts out in that audio's place.  REAPER plays an instrument's track audio
# beside the instrument's output (HARDWARE_FIRMWARE.md, "The VC-2 heard dry in REAPER"), so the VC-2 mode, which takes
# its mic from the input, was heard under its own input there.  An AU music effect (aumf VsEf Gato: a code of its own,
# a plugin of its own to every host, beside the instrument's aumu VsEm Gato) and a VST3 Fx; no Standalone, which would
# be the instrument's again.  CMakeLists.txt includes this after the sources and vse_plugin_setup.
juce_add_plugin(VSynthEmuFx
    COMPANY_NAME            "Gato Gordo Labs"
    BUNDLE_ID               com.GatoGordoLabs.VSynthEmuFx
    PLUGIN_MANUFACTURER_CODE Gato
    PLUGIN_CODE             VsEf
    FORMATS                 VST3 AU
    PRODUCT_NAME            "V-Synth Emu FX"

    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        TRUE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    AU_MAIN_TYPE            kAudioUnitType_MusicEffect
    VST3_CATEGORIES         Fx
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_AUTO_MANIFEST      FALSE   # as the instrument's: the manifest helper can't load the ad-hoc-signed bundle
)

# The instrument's own sources, read back from its target, so a file added there is in both
get_target_property(vse_sources VSynthEmu SOURCES)
list(FILTER vse_sources INCLUDE REGEX "^Source/")
target_sources(VSynthEmuFx PRIVATE ${vse_sources})
vse_plugin_setup(VSynthEmuFx)
