// End-to-end check of the plugin processor's XT path: ROM, file load,
// background encode, sample-accurate MIDI, render — against XtSynth driven
// directly on the same input.  Build: cmake --build <dir> --target VSynthEmuTest
// Run:   VSynthEmuTest <rom.2.bin> <file.wav> [note]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/FxPanel.h"
#include "../Source/Sh4Interp.h"
#include "../Source/XtPatch.h"
#include "../Source/XtBuildId.h"
#include "../Source/XtVpwFormat.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "TestAppDir.h"

// TEST_ROM_SEARCH=1: where a new instance finds the image, first to last, and what's kept for the next (VSynthEmuTest
// alone: it stops there).  A symbolic link stands for each copy, so the image's 8 MB isn't copied about.
static int romSearchTest(const juce::File& rom, const juce::File& appDir, const juce::File& wave) {
    unsetenv("VSE_XT_ROM");
    const char* const name = "V-SynthXT.rom.2.bin";
    const auto beside = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile(name);
    const auto inApp = appDir.getChildFile(name), elsewhere = appDir.getChildFile("elsewhere/some other name.bin");
    const auto settings = appDir.getChildFile("settings.xml");
    if (beside.exists()) { std::fprintf(stderr, "rom search: there's already an image at %s\n", beside.getFullPathName().toRawUTF8()); return 1; }
    elsewhere.getParentDirectory().createDirectory();
    struct Tidy { juce::File f; ~Tidy() { f.deleteFile(); } } tidy { beside };

    int checks = 0, failed = 0;
    const auto check = [&](bool ok, const char* what) { ++checks; if (!ok) { ++failed; std::fprintf(stderr, "rom search: %s\n", what); } };
    const auto found = [] { VSynthEmuProcessor p; return p.apvts.state.getProperty("romPath").toString(); };
    const auto link = [&](const juce::File& at) { at.deleteFile(); return rom.createSymbolicLink(at, true); };
    const auto is = [&](const juce::File& f) { return found() == f.getFullPathName(); };

    { VSynthEmuProcessor p; check(!p.hasRom() && p.getStatusText().startsWith("No ROM") && p.apvts.state.getProperty("romPath").toString().isEmpty(), "with nothing to find, it should say so"); }

    // With no image the engine plays the file, at the level of the XT's voices: ⅛ of it, near enough, as the engine isn't exactly unity.
    { juce::AudioFormatManager formats; formats.registerBasicFormats();
      float lo = 0, hi = 0, lo2 = 0, hi2 = 0;
      if (std::unique_ptr<juce::AudioFormatReader> r { formats.createReaderFor(wave) }) r->readMaxLevels(0, r->lengthInSamples, lo, hi, lo2, hi2);
      const double top = std::max({ std::fabs((double)lo), std::fabs((double)hi), std::fabs((double)lo2), std::fabs((double)hi2) });
      VSynthEmuProcessor p;
      p.prepareToPlay(44100.0, 512);
      p.loadFile(wave);
      juce::AudioBuffer<float> buf(2, 512);
      juce::MidiBuffer midi;
      midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
      double peak = 0;
      for (int b = 0; b < 400; ++b) { p.processBlock(buf, midi); midi.clear(); peak = std::max(peak, (double)buf.getMagnitude(0, 512)); }
      std::printf("the engine with no image: the file's peak %.4f, out %.4f\n", top, peak);
      check(top > 0.0 && peak > top * 0.0625 && peak < top * 0.25, "with no image, the engine should play at the XT's level"); }

    // A project's path is that project's: used, and not kept for the others.
    { juce::MemoryBlock saved;
      { VSynthEmuProcessor p; p.apvts.state.setProperty("romPath", rom.getFullPathName(), nullptr); p.getStateInformation(saved); }
      VSynthEmuProcessor p; p.setStateInformation(saved.getData(), (int)saved.getSize());
      check(p.hasRom() && p.getStatusText() == "ROM ready", "a project's path should be used");
      check(!settings.exists(), "a project's path shouldn't be kept for other projects"); }
    check(found().isEmpty(), "a project's path shouldn't reach a new instance");

    check(link(inApp) && is(inApp), "the plugin's own folder");
    check(link(beside) && is(beside), "beside the plugin, ahead of its own folder");
    // Already in memory, so not read again: a bad file at that path shows whether it was.
    beside.deleteFile(); beside.replaceWithText("not a ROM");
    check(is(beside), "an image in memory shouldn't be read again");
    beside.deleteFile();
    check(is(inApp), "the plugin's own folder again");
    // A bad file falls through to the next, and costs the instances playing nothing.
    beside.replaceWithText("not a ROM");
    { VSynthEmuProcessor p; check(p.apvts.state.getProperty("romPath").toString() == inApp.getFullPathName() && p.hasRom() && p.getStatusText() == "ROM ready", "a bad file beside the plugin should fall through"); }
    beside.deleteFile();
    inApp.deleteFile();
    check(found().isEmpty(), "nothing left to find");

    // Chosen by hand: kept for the next instance, behind the two places.
    { VSynthEmuProcessor p; check(p.setRomFile(rom) && settings.existsAsFile(), "a chosen image should be kept"); }
    check(is(rom), "the chosen image, for a new instance");
    check(link(inApp) && is(inApp), "the plugin's own folder, ahead of the chosen image");
    inApp.deleteFile();
    // A wrong choice: refused, and what was in use goes on, here and for the next.
    { VSynthEmuProcessor p; check(!p.setRomFile(settings) && p.getStatusText().startsWith("Not a usable") && p.hasRom(), "a bad file chosen should be refused, the image kept"); }
    check(is(rom), "a refused file shouldn't be kept");

    check(link(elsewhere) && link(beside), "links");
    setenv("VSE_XT_ROM", elsewhere.getFullPathName().toRawUTF8(), 1);
    check(is(elsewhere), "VSE_XT_ROM, ahead of everything");
    unsetenv("VSE_XT_ROM");
    beside.deleteFile();

    // The chosen image moved away: back to asking, no harm done.
    { VSynthEmuProcessor p; check(p.setRomFile(elsewhere), "any file name will do, chosen by hand"); }
    elsewhere.deleteFile();
    { VSynthEmuProcessor p; check(p.getStatusText().startsWith("No ROM"), "a chosen image that's gone should mean asking again"); }

    if (failed) { std::fprintf(stderr, "rom search: %d of %d wrong\n", failed, checks); return 1; }
    std::printf("rom search: %d checks, the order and what's kept as they should be\n", checks);
    return 0;
}

// TEST_PATCH_APPLY=1: every zone of every patch in the firmware's image through the parameters and the effects, and back through what the processor plays from; then a
// project on disk, a patch and its wave made from the image's, with a tuning and a loop of its own, against XtSynth driven directly (VSynthEmuTest alone: it stops there).
// TEST_PATCH_DIR: a folder of .p00 besides, not in verify.sh.
static int patchApplyTest(const juce::File& rom) {
    VSynthEmuProcessor p;
    if (!p.setRomFile(rom)) { std::fprintf(stderr, "ROM rejected\n"); return 1; }
    int checks = 0, failed = 0;
    const auto check = [&](bool ok, const juce::String& what) { ++checks; if (!ok && ++failed <= 20) std::fprintf(stderr, "patch apply: %s\n", what.toRawUTF8()); };
    XtDiskImage disk;
    if (!disk.open(rom.getSiblingFile("V-SynthXTDiskImage.bin.0.bin").getFullPathName().toStdString())) { std::fprintf(stderr, "patch apply: no disk image beside the ROM\n"); return 1; }
    std::vector<std::vector<uint8_t>> files;
    for (const auto& e : disk.list("Internal.prj/Patch")) files.push_back(disk.read(e));
    if (const char* dir = std::getenv("TEST_PATCH_DIR"))
        for (const auto& f : juce::File(juce::String::fromUTF8(dir)).findChildFiles(juce::File::findFiles, false, "*.p00")) {
            juce::MemoryBlock m;
            f.loadFileAsData(m);
            files.emplace_back(static_cast<const uint8_t*>(m.getData()), static_cast<const uint8_t*>(m.getData()) + m.getSize());
        }
    int zonesDone = 0, stepEvent = 0;
    long fxValues = 0;
    for (const auto& f : files) {
        XtPatchFile patch;
        if (!xtPatchRead(f.data(), f.size(), patch)) { check(false, "a patch that won't read"); continue; }
        for (int z = 0; z < xtPatchZones(patch.store); ++z) {
            const auto want = xtWithPatchArp(patch.store, xtPatchControls(patch.store, z, p.currentControls()));   // and its arpeggio (v190)
            p.applyPatch(patch.name, patch.store, z, juce::File());
            const XtSynth::Controls got = p.currentControls();
            check(got == want, juce::String(patch.name) + " zone " + juce::String(z) + ": the controls differ");
            ++zonesDone;
            // Playback Mode STEP and EVENT: done on every kind of wave (v156), so never said to be missing
            for (int k = 0; k < 2; ++k) {
                const int b = 0x34 * k, mode = xtPatchValue(patch.store, 0x2d2 + b, z);
                const bool pcm = xtPatchValue(patch.store, 0x2a4 + b, z) != 0 && xtPatchValue(patch.store, 0x2a5 + b, z) == 1;
                stepEvent += pcm && mode >= 2;
                for (const auto& m : xtPatchMissing(patch.store, z))
                    check(m.find("Playback Mode") == std::string::npos, juce::String(patch.name) + " zone " + juce::String(z) + ": " + m + ", said wrongly");
            }
        }
        check(p.getPatchName() == juce::String(patch.name), "its name");
        for (const auto& fx : xtPatchEffects(patch.store)) {
            int knob = INT_MIN;
            for (const char* id : { "fxMfxOn", "fxChoOn", "fxRevOn", "fxMfxType", "fxMfxMaster", "fxMfxCho", "fxMfxRev", "fxChoType", "fxChoMaster", "fxChoRev", "fxRevType", "fxRevMaster" })
                if (VSynthEmuProcessor::fxKnobId(id) == fx.id) knob = (int)std::lround(p.apvts.getRawParameterValue(id)->load());
            const int got = knob != INT_MIN ? knob : p.fxParam(fx.id, INT_MIN);
            ++fxValues;
            check(got == fx.value, juce::String(patch.name) + ": effect ID " + juce::String::toHexString(fx.id) + " is " + juce::String(got) + ", the patch says " + juce::String(fx.value));
        }
    }

    // A project.  The wave: a preset wave of the image's, its own settings put in its chunks
    const auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("vse-patchtest", "");
    struct Tidy { juce::File f; ~Tidy() { f.deleteRecursively(); } } tidy { dir };
    const auto patchDir = dir.getChildFile("Patch"), waveDir = dir.getChildFile("Wave");
    patchDir.createDirectory(); waveDir.createDirectory();
    std::vector<uint8_t> chosen;
    XtPatchFile patch;
    int number = 0;
    for (int pass = 0; pass < 2 && chosen.empty(); ++pass)   // one with Playback Mode STEP or EVENT on OSC 1 if there is one, for the note of what's missing
        for (const auto& f : files) {
            XtPatchFile t;
            if (!xtPatchRead(f.data(), f.size(), t)) continue;
            // one zone: the sound below is held to XtSynth on zone 1's settings alone
            if (xtPatchZones(t.store) == 1 && xtPatchValue(t.store, 0x2a4) && xtPatchValue(t.store, 0x2a5) == 1 && xtPatchWaveNumber(t.store, 0, 0) > 0 && xtPatchValue(t.store, 0x2d9) != 1
                && (pass == 1 || xtPatchValue(t.store, 0x2d2) >= 2)) { chosen = f; patch = t; number = xtPatchWaveNumber(t.store, 0, 0); break; }
        }
    if (chosen.empty()) { std::fprintf(stderr, "patch apply: no patch with a wave on OSC 1\n"); return 1; }
    const auto presets = disk.presetWaves();
    XtWaveFile stream;
    if (presets.empty() || !xtWaveRead(&disk.bytes()[presets[0]], disk.bytes().size() - presets[0], stream)) { std::fprintf(stderr, "patch apply: no preset wave\n"); return 1; }
    std::vector<uint8_t> waveBytes(disk.bytes().begin() + (long)presets[0], disk.bytes().begin() + (long)(presets[0] + stream.size));
    const auto chunk = [&](const char* tag) { for (size_t i = 0; i + 8 < waveBytes.size(); ++i) if (!std::memcmp(&waveBytes[i], tag, 4)) return i + 8; return (size_t)0; };
    const auto put32 = [&](size_t at, int32_t v) { for (int i = 0; i < 4; ++i) waveBytes[at + (size_t)i] = (uint8_t)((uint32_t)v >> (8 * i)); };
    const size_t sc = chunk("vpsc"), lp = chunk("vplp");
    const long last = stream.frames - 1;
    const int loopA = (int)(last / 3), loopB = (int)(last * 3 / 4), bwdA = (int)(last / 5), bwdB = (int)(last / 2);
    waveBytes[sc + 0xa] = 65;                 // the Original Key
    put32(sc + 0x18, 15);                     // cents
    put32(sc + 0x1c, 999991);                 // the Original Tempo
    put32(lp, 55534);                         // 5.55 semitones
    put32(lp + 4, loopA); put32(lp + 8, loopB); put32(lp + 0xc, bwdB); put32(lp + 0x10, bwdA);
    waveBytes[lp + 0x18] = 2; waveBytes[lp + 0x19] = 1;
    waveBytes[lp + 0x1a] = 0;                 // it loops, whatever the switch says
    waveBytes[lp + 0x1b] = (uint8_t)(int8_t)-30;
    const size_t no = chunk("vpno");          // its list of events for STEP and EVENT: three, out of order
    check(no != 0, "the preset wave's list of events (vpno)");
    if (no) { put32(no + 4, 3); for (int i = 0; i < 3; ++i) { waveBytes[no + 12 + 2 * (size_t)i] = (uint8_t)(i == 0 ? 2 : i - 1); waveBytes[no + 13 + 2 * (size_t)i] = 0; } }
    const juce::String waveName = juce::String(number).paddedLeft('0', 3) + " Test.vpw";
    waveDir.getChildFile(waveName).replaceWithData(waveBytes.data(), waveBytes.size());
    waveDir.getChildFile(juce::String(number).paddedLeft('0', 3) + " Test 2.vpw").replaceWithData(waveBytes.data(), 16);   // a sync's copy, longer in name: not this one
    const auto patchFile = patchDir.getChildFile("x.p00");
    patchFile.replaceWithData(chosen.data(), chosen.size());

    const double rate = 48000.0;
    const int block = 512;
    p.prepareToPlay(rate, block);
    check(!p.loadPatch(patchDir.getChildFile("nothing.p00")) && !p.getStatusText().isEmpty(), "a missing file should be refused");
    for (const char* id : { "encodeType", "osc2EncodeType" }) {   // left on something else, for the patch to put back
        auto* type = p.apvts.getParameter(id);
        type->setValueNotifyingHost(type->convertTo0to1(2.0f));
    }
    check(p.loadPatch(patchFile), "the patch should load");
    check(p.getPatchName() == juce::String(patch.name), "its name");
    {   // STEP or EVENT on OSC 1, never said to be missing (v156)
        const int mode = xtPatchValue(patch.store, 0x2d2);
        const bool said = p.getPatchNote().contains("Playback Mode");
        check(!said, "Playback Mode " + juce::String(mode) + " on encode type " + juce::String(stream.encodeType) + ": the note says \"" + p.getPatchNote() + "\"");
        std::printf("patch apply: the project's patch %s, OSC 1 Playback Mode %d on encode type %d\n", patch.name.c_str(), mode, stream.encodeType);
    }
    check(p.hasFileLoaded(0) && p.getLoadedFileName(0) == waveName, "its wave on OSC 1, by its number: " + p.getLoadedFileName(0));
    check(p.lastPatchFolder() == patchDir, "the folder kept for next time");
    const auto wait = [&] {
        for (int i = 0; i < 600 && !p.getStatusText().contains("Encoded"); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return p.getStatusText().contains("Encoded");
    };
    check(wait(), "its wave should encode: " + p.getStatusText());
    XtSynth::Controls c = p.currentControls();
    check(c.rootKey == 65, "the wave's Original Key");
    check(c.origTempo == 999991, "the wave's Original Tempo: " + juce::String(c.origTempo));
    check(c.loopMode == 2 && c.loop, "the wave's Loop Mode, and its loop on whatever the patch's switch says");
    check(std::lround((double)c.loopStart * (double)last) == loopA && std::lround((double)c.loopEnd * (double)last) == loopB, "the wave's loop, to the sample");
    check(p.apvts.getRawParameterValue("encodeType")->load() == 0.0f && p.apvts.getRawParameterValue("osc2EncodeType")->load() == 0.0f, "Encode Type on Auto, the wave's own");

    // The sound against XtSynth, as this wave and these controls: the wave's tuning and backward loop are nothing but what's on its encoded form
    {
        XtWaveFile wave;
        const auto bytes = waveBytes;
        check(xtWaveRead(bytes.data(), bytes.size(), wave), "the wave reads");
        std::vector<std::vector<float>> chans((size_t)wave.channels);
        for (long i = 0; i < wave.frames; ++i)
            for (int k = 0; k < wave.channels; ++k) chans[(size_t)k].push_back((float)wave.pcm[(size_t)(i * wave.channels + k)] / 32768.0f);
        const int type = wave.encodeType == 0 || wave.encodeType == 1 || wave.encodeType == 3 || wave.encodeType == 5 ? wave.encodeType : 1;
        auto own = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &wave.events, &wave.vpdt), plain = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &wave.events, &wave.vpdt);
        xtWaveOntoEncoded(wave, *own);
        check(own->vpnoCount == 3 && own->vpno.size() >= 3 && own->vpno[0] == 2 && own->vpno[1] == 0 && own->vpno[2] == 1, "the wave's own list of events, not the encode's");
        c = p.currentControls();
        c.pitchSemis = 0;
        XtSynth ref, untuned;
        ref.prepare(rate); ref.setSample(own); ref.setControls(c);
        untuned.prepare(rate); untuned.setSample(plain); untuned.setControls(c);
        juce::AudioBuffer<float> buf(2, block);
        juce::MidiBuffer midi;
        std::vector<float> rl((size_t)block), rr((size_t)block);
        long different = 0, frames = 0, away = 0;
        double energy = 0;
        for (int b = 0; b < 200; ++b) {
            midi.clear();
            if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0);
            buf.clear();
            p.processBlock(buf, midi);
            juce::ScopedNoDenormals ftz;
            if (b == 0) { ref.noteOn(72, 100 / 127.0f); untuned.noteOn(72, 100 / 127.0f); }
            ref.render(rl.data(), rr.data(), block);
            for (int i = 0; i < block; ++i) {
                ++frames;
                if (buf.getSample(0, i) != rl[(size_t)i] || buf.getSample(1, i) != rr[(size_t)i]) ++different;
                energy += (double)rl[(size_t)i] * rl[(size_t)i];
            }
            untuned.render(rl.data(), rr.data(), block);
            for (int i = 0; i < block; ++i) if (buf.getSample(0, i) != rl[(size_t)i]) ++away;
        }
        midi.clear();
        midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        buf.clear();
        p.processBlock(buf, midi);
        check(different == 0, juce::String(different) + " of " + juce::String(frames) + " frames differ from XtSynth with the wave's tuning");
        check(energy > 0.0, "it should sound");
        check(away > frames / 2, "the tuning should change the sound: " + juce::String(away) + " of " + juce::String(frames));
    }

    // Its effects, through the chip: the driver takes what the patch says, and something still comes out
    {
        p.fxManualTicks();
        auto* fxOn = p.apvts.getParameter("fxOn");
        fxOn->setValueNotifyingHost(1.0f);
        juce::AudioBuffer<float> buf(2, block);
        juce::MidiBuffer midi;
        double energy = 0;
        for (int b = 0; b < 300; ++b) {
            p.fxTick();
            midi.clear();
            if (b == 100) midi.addEvent(juce::MidiMessage::noteOn(1, 72, (juce::uint8)100), 0);
            buf.clear();
            p.processBlock(buf, midi);
            energy += (double)buf.getRMSLevel(0, 0, block);
        }
        check(p.effectsRunning(), "the effects should come up on a patch");
        check(energy > 0.0, "it should sound through the effects");
        fxOn->setValueNotifyingHost(0.0f);
        midi.clear();
        midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        p.processBlock(buf, midi);
    }

    // A saved session: the parameters as they stood, the wave's settings not put over them
    {
        auto* key = p.apvts.getParameter("rootKey");
        key->setValueNotifyingHost(key->convertTo0to1(70.0f));
        juce::MemoryBlock state;
        p.getStateInformation(state);
        VSynthEmuProcessor q;
        check(q.setRomFile(rom), "the ROM again");
        q.setStateInformation(state.getData(), (int)state.getSize());
        check(q.getPatchName() == p.getPatchName(), "the patch's name comes back");
        check(q.hasFileLoaded(0) && q.getLoadedFileName(0) == waveName, "the wave comes back");
        check(q.currentControls().rootKey == 70, "a saved session's Original Key isn't the wave's");
        check(q.currentControls() == p.currentControls(), "a saved session's controls come back");
    }

    std::string zoneWhat = "no patch of zones";
    // A patch of several zones (v154): zones 2…16 play as the patch has them, each its own settings and waves, while the parameters are zone 1's.
    // The image's first with three zones or more and waves on two of them, every wave it names this one; the sound against XtSynth given the
    // same zones, a note on each of four zones, then the same from a saved session.  On a processor of its own, as XtSynth's is new: what a
    // synth keeps from note to note (the free-running LFOs, the random pitch's generator, the LFOs' seeds) has moved on in this one.
    {
        VSynthEmuProcessor p;
        check(p.setRomFile(rom), "the ROM for the zones");
        p.prepareToPlay(rate, block);
        std::vector<uint8_t> zoneFile;
        XtPatchFile zp;
        // first one with a zone past the first whose OSC 2 plays a wave of its own, then any
        for (int pass = 0; pass < 2 && zoneFile.empty(); ++pass)
            for (const auto& f : files) {
                XtPatchFile t;
                if (!xtPatchRead(f.data(), f.size(), t) || xtPatchZones(t.store) < 3) continue;
                int pcm = 0;
                bool osc2 = false;
                for (int z = 0; z < xtPatchZones(t.store); ++z) {
                    if (xtPatchValue(t.store, 0x2a4, z) && xtPatchValue(t.store, 0x2a5, z) == 1 && xtPatchWaveNumber(t.store, z, 0) > 0) ++pcm;
                    const int n2 = xtPatchWaveNumber(t.store, z, 1);
                    if (z && xtPatchValue(t.store, 0x2d8, z) && xtPatchValue(t.store, 0x2d9, z) == 1 && n2 > 0 && n2 != xtPatchWaveNumber(t.store, z, 0)) osc2 = true;
                }
                if (pcm >= 2 && (pass || osc2)) { zoneFile = f; zp = t; break; }
            }
        check(!zoneFile.empty(), "a patch of several zones in the image");
        if (!zoneFile.empty()) {
            const int zones = xtPatchZones(zp.store);
            std::set<int> numbers;
            for (int z = 0; z < zones; ++z)
                for (int k = 0; k < 2; ++k) if (const int n = xtPatchWaveNumber(zp.store, z, k); n > 0 && n != number) numbers.insert(n);
            // The odd numbers' waves a SOLO preset wave with attacks: a zone plays its stored vpdt, whose records an older encoder wrote, where a
            // new encode would give others (preset 0 is a band wave, played on its own vpdt too)
            std::vector<uint8_t> soloBytes;
            long soloLast = 0;
            for (size_t i = 1; i < presets.size() && soloBytes.empty(); ++i) {
                XtWaveFile w;
                if (!xtWaveRead(&disk.bytes()[presets[i]], disk.bytes().size() - presets[i], w) || w.encodeType != 1 || w.channels != 1 || w.events.size() < 3
                    || w.frames > 150000) continue;
                soloBytes.assign(disk.bytes().begin() + (long)presets[i], disk.bytes().begin() + (long)(presets[i] + w.size));
                soloLast = w.frames - 1;
            }
            check(!soloBytes.empty(), "a SOLO preset wave with attacks for the zones");
            // each number its own wave: the same audio, its own Original Key, tuning and loop, so a zone given another's shows
            const auto bytesOf = [&](int n) {
                const bool solo = n % 2 && !soloBytes.empty();
                std::vector<uint8_t> b = solo ? soloBytes : waveBytes;
                if (n == number) return waveBytes;
                const auto at = [&](const char* tag) { for (size_t i = 0; i + 8 < b.size(); ++i) if (!std::memcmp(&b[i], tag, 4)) return i + 8; return (size_t)0; };
                const size_t s2 = at("vpsc"), l2 = at("vplp");
                b[s2 + 0xa] = (uint8_t)(40 + n % 40);
                const auto put = [&](size_t at, int32_t v) { for (int i = 0; i < 4; ++i) b[at + (size_t)i] = (uint8_t)((uint32_t)v >> (8 * i)); };
                put(l2, 1000 * (n % 13) - 6000);
                put(l2 + 4, (solo ? (int)(soloLast / 3) : loopA) / (1 + n % 3));
                return b;
            };
            for (int n : numbers) { const auto b = bytesOf(n); waveDir.getChildFile(juce::String(n).paddedLeft('0', 3) + " Zone.vpw").replaceWithData(b.data(), b.size()); }
            const auto zoneFileOnDisk = patchDir.getChildFile("zones.p00");
            zoneFileOnDisk.replaceWithData(zoneFile.data(), zoneFile.size());
            const auto ready = [&](VSynthEmuProcessor& q) {
                for (int i = 0; i < 1200 && !(q.zonesReady() && (!q.hasFileLoaded(0) || q.getStatusText().contains("Encoded"))); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return q.zonesReady();
            };
            check(p.loadPatch(zoneFileOnDisk), "the patch of zones should load");
            check(ready(p), "its zones' waves should encode");
            check(p.patchZones() == zones, "its zones: " + juce::String(p.patchZones()) + " of " + juce::String(zones));
            check(p.getPatchNote().contains(juce::String(zones) + " zones"), "its zones in its note: " + p.getPatchNote());
            // XtSynth on the same: zone 1 as the parameters stand, zones 2… from the store, each wave the one written for its number
            std::map<int, std::pair<XtWaveFile, std::shared_ptr<XtEncoded>>> waveOf;
            const auto encOf = [&](int n) -> std::pair<XtWaveFile, std::shared_ptr<XtEncoded>>& {
                auto& e = waveOf[n];
                if (e.second) return e;
                const auto b = bytesOf(n);
                check(xtWaveRead(b.data(), b.size(), e.first), "wave " + juce::String(n) + " reads");
                std::vector<std::vector<float>> chans((size_t)e.first.channels);
                for (long i = 0; i < e.first.frames; ++i)
                    for (int k = 0; k < e.first.channels; ++k) chans[(size_t)k].push_back((float)e.first.pcm[(size_t)(i * e.first.channels + k)] / 32768.0f);
                const int type = e.first.encodeType == 0 || e.first.encodeType == 1 || e.first.encodeType == 3 || e.first.encodeType == 5 ? e.first.encodeType : 1;
                e.second = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &e.first.events, &e.first.vpdt);
                xtWaveOntoEncoded(e.first, *e.second);
                return e;
            };
            const std::shared_ptr<XtEncoded> enc = encOf(number).second;   // the patch of one zone's
            XtSynth::Controls c1 = p.currentControls();
            c1.pitchSemis = 0;
            auto zs = std::make_shared<XtSynth::Zones>();
            xtPatchZoneSet(zp.store, c1, *zs);
            for (int z = 1; z < zones; ++z)
                for (int k = 0; k < 2; ++k) {
                    XtSynth::OscControls& o = k ? zs->ctl[z].osc2 : static_cast<XtSynth::OscControls&>(zs->ctl[z]);
                    const int n = xtPatchWaveNumber(zp.store, z, k);
                    if (o.oscType != 1 || n <= 0) continue;
                    xtWaveOntoControls(encOf(n).first, o);
                    zs->wave[z][k] = encOf(n).second;
                }
            // a key on each of four zones, the first of zone 1's and three others'
            std::vector<int> keys;
            std::set<int> seen;
            for (int note = 0; note < 128 && keys.size() < 4; ++note) {
                const int z = XtSynth::xtZoneOf(zs->upper, std::clamp(note + 12 * c1.octave, 0, 127));
                if (seen.insert(z).second && (z || keys.empty())) keys.push_back(note);
            }
            check(keys.size() >= 3, "keys on three zones at least");
            bool soloPlayed = false;
            for (int note : keys) {
                const int z = XtSynth::xtZoneOf(zs->upper, std::clamp(note + 12 * c1.octave, 0, 127));
                for (int k = 0; k < 2; ++k) {
                    const XtSynth::OscControls& o = k ? zs->ctl[z].osc2 : static_cast<const XtSynth::OscControls&>(zs->ctl[z]);
                    const int n = xtPatchWaveNumber(zp.store, z, k);
                    soloPlayed |= z > 0 && o.on && o.oscType == 1 && n > 0 && n != number && n % 2;
                }
            }
            check(soloPlayed, "a zone played on the SOLO wave of its own events");
            zoneWhat = std::string(zp.name) + "'s " + std::to_string(zones) + " zones, " + std::to_string(keys.size()) + " of them played";
            const auto play = [&](VSynthEmuProcessor& q, bool withZones, long& away) {
                XtSynth ref, alone;
                ref.prepare(rate); alone.prepare(rate);
                for (XtSynth* s : { &ref, &alone }) {
                    for (int k = 0; k < 2; ++k) if (q.hasFileLoaded(k)) s->setSample(encOf(xtPatchWaveNumber(zp.store, 0, k)).second, k);
                    s->setControls(c1);
                }
                if (withZones) ref.setZones(zs);
                juce::AudioBuffer<float> buf(2, block);
                juce::MidiBuffer midi;
                std::vector<float> rl((size_t)block), rr((size_t)block), al((size_t)block), ar((size_t)block);
                long different = 0;
                double energy = 0;
                for (size_t n = 0; n < keys.size(); ++n)
                    for (int b = 0; b < 40; ++b) {
                        midi.clear();
                        if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, keys[n], (juce::uint8)100), 0);
                        if (b == 30) midi.addEvent(juce::MidiMessage::noteOff(1, keys[n]), 0);
                        buf.clear();
                        q.processBlock(buf, midi);
                        juce::ScopedNoDenormals ftz;
                        if (b == 0) { ref.noteOn(keys[n], 100 / 127.0f); alone.noteOn(keys[n], 100 / 127.0f); }
                        if (b == 30) { ref.noteOff(keys[n]); alone.noteOff(keys[n]); }
                        ref.render(rl.data(), rr.data(), block);
                        alone.render(al.data(), ar.data(), block);
                        for (int i = 0; i < block; ++i) {
                            if (buf.getSample(0, i) != rl[(size_t)i] || buf.getSample(1, i) != rr[(size_t)i]) ++different;
                            if (buf.getSample(0, i) != al[(size_t)i]) ++away;
                            energy += (double)rl[(size_t)i] * rl[(size_t)i];
                        }
                    }
                midi.clear();
                midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
                buf.clear();
                q.processBlock(buf, midi);
                check(energy > 0.0, "the zones should sound");
                return different;
            };
            long away = 0;
            const long different = play(p, true, away);
            check(different == 0, juce::String(different) + " frames differ from XtSynth given the patch's zones");
            check(away > 0, "zones 2… should sound as their own, not as zone 1");
            // a saved session brings its zones back
            juce::MemoryBlock state;
            p.getStateInformation(state);
            VSynthEmuProcessor q;
            check(q.setRomFile(rom), "the ROM again");
            q.prepareToPlay(rate, block);
            q.setStateInformation(state.getData(), (int)state.getSize());
            check(ready(q), "a saved session's zones should encode");
            check(q.patchZones() == zones, "a saved session's zones: " + juce::String(q.patchZones()));
            long unused = 0;
            const long again = play(q, true, unused);
            check(again == 0, juce::String(again) + " frames of a saved session's zones differ");
            q.releaseResources();
            p.releaseResources();
            // And a patch of one zone after it leaves none of the last's: on a new processor, the zones' patch then that one, and a key
            // that was on another zone plays zone 1
            {
                VSynthEmuProcessor r;
                check(r.setRomFile(rom), "the ROM once more");
                r.prepareToPlay(rate, block);
                check(r.loadPatch(zoneFileOnDisk) && ready(r) && r.loadPatch(patchFile) && ready(r), "a patch of one zone after one of zones");
                check(r.patchZones() == 1, "a patch of one zone leaves no zones of the last");
                XtSynth alone;
                alone.prepare(rate);
                for (int k = 0; k < 2; ++k) if (r.hasFileLoaded(k)) alone.setSample(enc, k);
                XtSynth::Controls c = r.currentControls();
                c.pitchSemis = 0;
                alone.setControls(c);
                juce::AudioBuffer<float> buf(2, block);
                juce::MidiBuffer midi;
                std::vector<float> al((size_t)block), ar((size_t)block);
                long off = 0;
                for (int b = 0; b < 40; ++b) {
                    midi.clear();
                    if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, keys[1], (juce::uint8)100), 0);
                    buf.clear();
                    r.processBlock(buf, midi);
                    juce::ScopedNoDenormals ftz;
                    if (b == 0) alone.noteOn(keys[1], 100 / 127.0f);
                    alone.render(al.data(), ar.data(), block);
                    for (int i = 0; i < block; ++i) if (buf.getSample(0, i) != al[(size_t)i] || buf.getSample(1, i) != ar[(size_t)i]) ++off;
                }
                check(off == 0, juce::String(off) + " frames of zone 1 differ after a patch of zones");
                r.releaseResources();
            }
            for (int n : numbers) waveDir.getChildFile(juce::String(n).paddedLeft('0', 3) + " Zone.vpw").deleteFile();
        }
    }

    // A wave it can't find: said so, nothing left on the OSC
    waveDir.getChildFile(waveName).deleteFile();
    waveDir.getChildFile(juce::String(number).paddedLeft('0', 3) + " Test 2.vpw").deleteFile();
    check(p.loadPatch(patchFile), "the patch without its wave");
    check(!p.hasFileLoaded(0), "no wave left on OSC 1");
    check(p.getPatchNote().contains("no wave " + juce::String(number)), "the wave not found, in its note: " + p.getPatchNote());
    // And a file that's no patch
    const auto notPatch = patchDir.getChildFile("no.p00");
    notPatch.replaceWithText("not a patch at all, though long enough to look at");
    check(!p.loadPatch(notPatch) && p.getStatusText().contains("not a V-Synth patch"), "something else should be refused: " + p.getStatusText());
    p.releaseResources();
    std::printf("patch apply: %d patches, %d zones (%d PCM OSCs on STEP or EVENT), %ld effect values, %s, %d checks, %d failed\n", (int)files.size(), zonesDone, stepEvent, fxValues, zoneWhat.c_str(), checks, failed);
    return failed ? 1 : 0;
}

// TEST_PATCH_PLAY=folder [TEST_PATCH_EVERY=n]: a project's patches as a user loads them, one in n, with their waves, a note on each: its level and what isn't here.
// Not in verify.sh: it needs a project, and takes a while.  A patch that's silent is worth a look, not always a fault: a patch can want a controller or the external input.
static int patchPlayTest(const juce::File& rom, const juce::File& folder, int every) {
    VSynthEmuProcessor p;
    if (!p.setRomFile(rom)) { std::fprintf(stderr, "ROM rejected\n"); return 1; }
    const double rate = 44100.0;
    const int block = 512;
    p.prepareToPlay(rate, block);
    auto files = folder.findChildFiles(juce::File::findFiles, false, "*.p00");
    files.sort();
    int played = 0, silent = 0, refused = 0, n = 0;
    for (const auto& f : files) {
        if (n++ % every) continue;
        if (!p.loadPatch(f)) { ++refused; std::printf("%-28s refused: %s\n", f.getFileName().toRawUTF8(), p.getStatusText().toRawUTF8()); continue; }
        const char* want = p.hasFileLoaded(1) ? "OSC 2: Encoded" : p.hasFileLoaded(0) ? "Encoded" : nullptr;
        for (int i = 0; want && i < 1200 && !p.getStatusText().startsWith(want); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (int i = 0; i < 1200 && !p.zonesReady(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(50));   // and its zones' waves
        juce::AudioBuffer<float> buf(2, block);
        juce::MidiBuffer midi;
        double energy = 0, peak = 0;
        for (int b = 0; b < 150; ++b) {
            midi.clear();
            if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
            if (b == 120) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
            buf.clear();
            p.processBlock(buf, midi);
            energy += (double)buf.getRMSLevel(0, 0, block) + (double)buf.getRMSLevel(1, 0, block);
            peak = std::max(peak, (double)buf.getMagnitude(0, block));
        }
        midi.clear();
        midi.addEvent(juce::MidiMessage::allSoundOff(1), 0);
        buf.clear();
        p.processBlock(buf, midi);
        ++played;
        if (energy <= 0.0) ++silent;
        std::printf("%-28s peak %.3f  %s  %s%s%s\n", f.getFileName().toRawUTF8(), peak, energy > 0.0 ? "" : "SILENT", p.getLoadedFileName(0).toRawUTF8(), p.getPatchNote().isNotEmpty() ? " | " : "", p.getPatchNote().toRawUTF8());
    }
    p.releaseResources();
    std::printf("patch play: %d played, %d silent, %d refused\n", played, silent, refused);
    return refused ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s rom.2.bin file.wav [note] [auto|solo|backing|ensemble]\n", argv[0]); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    TestAppDir testAppDir;
    if (std::getenv("TEST_ROM_SEARCH")) return romSearchTest(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]), testAppDir.dir, juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]));
    if (std::getenv("TEST_PATCH_APPLY")) return patchApplyTest(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]));
    if (const char* folder = std::getenv("TEST_PATCH_PLAY")) {
        const char* every = std::getenv("TEST_PATCH_EVERY");
        return patchPlayTest(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]), juce::File(juce::String::fromUTF8(folder)), every ? std::max(1, std::atoi(every)) : 1);
    }
    const int note = argc > 3 ? std::atoi(argv[3]) : 60;
    const double rate = 48000.0;
    const int block = 512;

    VSynthEmuProcessor p;
    if (!p.setRomFile(juce::File(argv[1]))) { std::fprintf(stderr, "ROM rejected: %s\n", p.getStatusText().toRawUTF8()); return 1; }
    p.prepareToPlay(rate, block);
    // TEST_SNAPSHOT=file.png: the editor drawn offscreen, for checking the
    // layout, on the open tab only (TEST_SNAPSHOT_TAB=n: that tab, 0 Oscillator … 7 Effects;
    // TEST_SNAPSHOT_SIZE=WxH: at that size;
    // TEST_SNAPSHOT_COSM2=1: the COSM tab, on COSM 2;
    // TEST_SNAPSHOT_TYPE=n: the COSM tab, COSM 1's type, e.g. 6 for the 2nd Side Band rows;
    // TEST_SNAPSHOT_ANALOG=1: OSC Type ANALOG, its rows shown;
    // TEST_SNAPSHOT_OSC2=1: with the OSC rows on OSC 2; =pcm: with OSC 2 on PCM too;
    // TEST_SNAPSHOT_FX=page: the Effects tab, 0 MFX, 1 Chorus, 2 Reverb, 3 EQ, with TEST_SNAPSHOT_FXTYPE=n the page's block's type)
    // TEST_SNAPSHOT_PATCH=file.p00: with that patch loaded
    if (const char* f = std::getenv("TEST_SNAPSHOT_PATCH")) p.loadPatch(juce::File::getCurrentWorkingDirectory().getChildFile(f));
    if (const char* snap = std::getenv("TEST_SNAPSHOT")) {
        if (std::getenv("TEST_SNAPSHOT_ANALOG")) {
            auto* prm = p.apvts.getParameter("oscType");
            prm->setValueNotifyingHost(prm->convertTo0to1(0.0f));
        }
        if (const char* t = std::getenv("TEST_SNAPSHOT_TYPE")) {
            auto* prm = p.apvts.getParameter("cosmType");
            prm->setValueNotifyingHost(prm->convertTo0to1((float)std::atoi(t)));
        }
        if (const char* o2 = std::getenv("TEST_SNAPSHOT_OSC2"); o2 && juce::String(o2) == "pcm") {
            auto* prm = p.apvts.getParameter(VSynthEmuProcessor::oscParamId(1, "oscType"));
            prm->setValueNotifyingHost(prm->convertTo0to1(1.0f));
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* editor = dynamic_cast<VSynthEmuEditor*>(ed.get());
        if (std::getenv("TEST_SNAPSHOT_TYPE") || std::getenv("TEST_SNAPSHOT_COSM2")) editor->showTab(3);
        if (const char* size = std::getenv("TEST_SNAPSHOT_SIZE")) ed->setSize(std::atoi(size), juce::String(size).fromFirstOccurrenceOf("x", false, false).getIntValue());
        if (std::getenv("TEST_SNAPSHOT_COSM2"))      // the COSM rows switched to COSM 2 (the selector re-attaches them)
            for (juce::Component* ch : ed->getChildren())
                if (auto* cb = dynamic_cast<juce::ComboBox*>(ch); cb && cb->getItemText(0) == "COSM 1") cb->setSelectedId(2, juce::sendNotificationSync);
        if (std::getenv("TEST_SNAPSHOT_OSC2"))       // the OSC rows switched to OSC 2
            for (juce::Component* ch : ed->getChildren())
                if (auto* cb = dynamic_cast<juce::ComboBox*>(ch); cb && cb->getItemText(0) == "OSC 1") cb->setSelectedId(2, juce::sendNotificationSync);
        if (const char* page = std::getenv("TEST_SNAPSHOT_FX")) {
            static const char* const typeOf[] = { "fxMfxType", "fxChoType", "fxRevType", "fxMfxType" };
            if (const char* t = std::getenv("TEST_SNAPSHOT_FXTYPE")) { auto* prm = p.apvts.getParameter(typeOf[std::atoi(page) & 3]); prm->setValueNotifyingHost(prm->convertTo0to1((float)std::atoi(t))); }
            editor->showTab(VSynthEmuEditor::kEffectsTab);
            for (juce::Component* ch : ed->getChildren())
                if (auto* panel = dynamic_cast<FxPanel*>(ch)) panel->showPage(std::atoi(page));
        }
        if (const char* tab = std::getenv("TEST_SNAPSHOT_TAB")) editor->showTab(std::atoi(tab));
        const juce::Image img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
        juce::File f(juce::File::getCurrentWorkingDirectory().getChildFile(snap));
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat png;
        if (!os.openedOk() || !png.writeImageToStream(img, os)) { std::fprintf(stderr, "snapshot failed\n"); return 1; }
        std::printf("snapshot: %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }
    // TEST_UI_LAYOUT=1: the editor's layout, then stop.  At its smallest, default and largest sizes, on every tab, for both OSC types and every COSM type: no control outside its parent, over another or
    // without a size, and no text cut short.  Every control is moved, to see which parameter it sets: none may move nothing, and no parameter may be left without a control, bar the engine's three, shown
    // only without a ROM.  (buildLayout drops what it doesn't place, so a control left out of it goes quietly.)  And each tab by a click, and the joined buttons both ways.  =list: the parameters with no control, and those with.
    if (const char* ui = std::getenv("TEST_UI_LAYOUT")) {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        auto* editor = dynamic_cast<VSynthEmuEditor*>(ed.get());
        int errors = 0, states = 0, checked = 0;
        const auto fail = [&](const juce::String& m) { if (++errors <= 40) std::fprintf(stderr, "layout: %s\n", m.toRawUTF8()); };
        FxPanel* panel = nullptr;
        for (juce::Component* ch : ed->getChildren()) if (auto* fp = dynamic_cast<FxPanel*>(ch)) panel = fp;
        juce::Viewport* view = nullptr;
        if (panel) for (juce::Component* ch : panel->getChildren()) if (auto* v = dynamic_cast<juce::Viewport*>(ch)) view = v;
        if (!view) { std::fprintf(stderr, "layout: no effects' panel\n"); return 1; }
        const auto isControl = [](juce::Component* c) { return dynamic_cast<juce::Label*>(c) || dynamic_cast<juce::Slider*>(c) || dynamic_cast<juce::ComboBox*>(c) || dynamic_cast<juce::Button*>(c); };
        const auto nameOf = [](juce::Component* c) -> juce::String {
            if (auto* l = dynamic_cast<juce::Label*>(c)) return "label \"" + l->getText() + "\"";
            if (auto* b = dynamic_cast<juce::Button*>(c)) return "button \"" + b->getButtonText() + "\"";
            if (auto* x = dynamic_cast<juce::ComboBox*>(c)) return "box \"" + x->getText() + "\"";
            if (dynamic_cast<juce::Slider*>(c)) return "slider at " + c->getBounds().toString();
            return "component at " + c->getBounds().toString();
        };
        // Each visible parent under c: not into a control's own parts, nor the effects' rows, which TEST_FX_PANEL goes through
        std::function<void(juce::Component&, const std::function<void(juce::Component&)>&)> walk = [&](juce::Component& c, const std::function<void(juce::Component&)>& visit) {
            visit(c);
            for (juce::Component* ch : c.getChildren())
                if (ch->isVisible() && !isControl(ch) && !dynamic_cast<juce::Viewport*>(ch)) walk(*ch, visit);
        };
        std::function<juce::Component*(juce::Component&, const std::function<bool(juce::Component*)>&)> find = [&](juce::Component& c, const std::function<bool(juce::Component*)>& is) -> juce::Component* {
            for (juce::Component* ch : c.getChildren()) {
                if (is(ch)) return ch;
                if (juce::Component* in = find(*ch, is)) return in;
            }
            return nullptr;
        };
        const auto boxOf = [&](const juce::String& first) { return dynamic_cast<juce::ComboBox*>(find(*ed, [&](juce::Component* c) { auto* x = dynamic_cast<juce::ComboBox*>(c); return x && x->getItemText(0) == first; })); };
        const auto buttonOf = [&](const juce::String& text) { return dynamic_cast<juce::Button*>(find(*ed, [&](juce::Component* c) { auto* b = dynamic_cast<juce::Button*>(c); return b && b->getButtonText() == text; })); };
        const auto setParam = [&](const juce::String& id, int v) { auto* prm = p.apvts.getParameter(id); prm->setValueNotifyingHost(prm->convertTo0to1((float)v)); };
        const auto cutShort = [](juce::Label& l) {                                 // JUCE squeezes a text to its minimum scale, then cuts it
            const float scale = l.getMinimumHorizontalScale() > 0 ? l.getMinimumHorizontalScale() : 0.7f;
            return juce::GlyphArrangement::getStringWidth(l.getFont(), l.getText()) * scale > (float)(l.getWidth() - l.getBorderSize().getLeftAndRight());
        };
        const auto boxLabelOf = [](juce::Component& c) { juce::Label* box = nullptr; for (juce::Component* ch : c.getChildren()) if (auto* l = dynamic_cast<juce::Label*>(ch)) box = l; return box; };
        // The effects' list: its names and values at their defaults, on each type
        const auto checkList = [&](const juce::String& where) {
            for (juce::Component* ch : view->getViewedComponent()->getChildren()) {
                if (!ch->isVisible()) continue;
                if (auto* l = dynamic_cast<juce::Label*>(ch)) { if (cutShort(*l)) fail(where + ": " + nameOf(ch) + " is cut short"); }
                else if (juce::Label* box = boxLabelOf(*ch); box && cutShort(*box)) fail(where + ": the value " + nameOf(box) + " is cut short");
            }
        };
        const auto check = [&](const juce::String& where) {
            ++states;
            walk(*ed, [&](juce::Component& c) {
                // a card that isn't shown takes its controls with it, the title line's too, unless a card that is holds them
                if (auto* pg = dynamic_cast<Ui::Page*>(&c)) {
                    std::set<juce::Component*> held, hidden;
                    for (const auto& card : pg->cards) {
                        auto& into = card.shown && !card.shown() ? hidden : held;
                        for (const auto& h : card.head) if (h.c) into.insert(h.c);
                        for (const auto& r : card.rows) for (const auto& it : r.items) if (it.c) into.insert(it.c);
                        for (juce::Component* x : card.extra) if (x) into.insert(x);
                    }
                    for (juce::Component* k : hidden)
                        if (!held.count(k) && k->isVisible()) fail(where + ": " + nameOf(k) + " shows on a card that isn't shown");
                }
                std::vector<juce::Component*> kids;
                for (juce::Component* ch : c.getChildren()) if (ch->isVisible() && !dynamic_cast<juce::ResizableCornerComponent*>(ch)) kids.push_back(ch);
                for (size_t i = 0; i < kids.size(); ++i) {
                    juce::Component* k = kids[i];
                    ++checked;
                    if (isControl(k) && k->getBounds().isEmpty()) fail(where + ": " + nameOf(k) + " has no size");
                    if (!c.getLocalBounds().contains(k->getBounds())) fail(where + ": " + nameOf(k) + " is outside its parent");
                    for (size_t j = i + 1; j < kids.size(); ++j)
                        if (k->getBounds().intersects(kids[j]->getBounds())) fail(where + ": " + nameOf(k) + " is over " + nameOf(kids[j]));
                    if (auto* l = dynamic_cast<juce::Label*>(k)) {
                        if (cutShort(*l)) fail(where + ": " + nameOf(k) + " is cut short");
                    } else if (dynamic_cast<juce::ComboBox*>(k) || dynamic_cast<juce::Slider*>(k)) {   // its value box (v190)
                        if (juce::Label* box = boxLabelOf(*k); box && cutShort(*box)) fail(where + ": " + nameOf(k) + " is cut short");
                    } else if (auto* b = dynamic_cast<juce::ToggleButton*>(k)) {   // as LookAndFeel_V4 draws it: the tick, then the text
                        const float size = juce::jmin(15.0f, (float)b->getHeight() * 0.75f);
                        if (juce::GlyphArrangement::getStringWidth(juce::Font(juce::FontOptions(size)), b->getButtonText()) * 0.7f > (float)(b->getWidth() - juce::roundToInt(size * 1.1f) - 12))
                            fail(where + ": " + nameOf(k) + " is cut short");
                    }
                }
            });
        };
        // Every control moved, and the parameters it changes put back
        std::vector<juce::RangedAudioParameter*> params;
        for (auto* prm : p.getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) params.push_back(r);
        std::set<juce::String> reached;
        const auto changedBy = [&](const std::function<void()>& move) {
            std::vector<float> before;
            for (auto* r : params) before.push_back(r->getValue());
            move();
            juce::RangedAudioParameter* changed = nullptr;
            for (size_t i = 0; i < params.size(); ++i)
                if (params[i]->getValue() != before[i]) { changed = params[i]; reached.insert(params[i]->paramID); params[i]->setValueNotifyingHost(before[i]); }
            return changed;
        };
        const auto moveAll = [&](const juce::String& where) {
            std::vector<juce::Component*> controls;
            walk(*ed, [&](juce::Component& c) { for (juce::Component* ch : c.getChildren()) if (ch->isVisible() && isControl(ch) && !dynamic_cast<juce::Label*>(ch)) controls.push_back(ch); });
            for (juce::Component* k : controls) {
                std::function<void()> move;
                if (auto* s = dynamic_cast<juce::Slider*>(k)) move = [s] { s->setValue(s->getValue() == s->getMaximum() ? s->getMinimum() : s->getMaximum(), juce::sendNotificationSync); };
                else if (auto* x = dynamic_cast<juce::ComboBox*>(k)) {             // what chooses what the others show, or isn't a parameter
                    const juce::String first = x->getItemText(0);
                    if (first == "OSC 1" || first == "COSM 1" || first == "Control 1" || first == VSynthEmuProcessor::assignName(0) || first == "Passthrough (baseline)") continue;
                    move = [x] { x->setSelectedItemIndex(x->getSelectedItemIndex() == 0 ? 1 : 0, juce::sendNotificationSync); };
                } else if (auto* b = dynamic_cast<juce::Button*>(k)) {
                    if (dynamic_cast<juce::TextButton*>(b) && !b->getClickingTogglesState()) continue;                  // Load, ROM
                    const juce::String text = b->getButtonText();
                    if (text.startsWith("OSC ") || text.startsWith("COSM ") || text == "MFX" || text == "Chorus" || text == "Reverb" || text == "System") continue;      // as the boxes above
                    if (b->getRadioGroupId() != 0 && b->getToggleState()) continue;                                     // one of a pair: the other moves it
                    move = [b] { b->setToggleState(!b->getToggleState(), juce::sendNotificationSync); };
                }
                if (!changedBy(move)) fail(where + ": " + nameOf(k) + " moves no parameter");
            }
        };
        const int cosmTypes = p.apvts.getParameter("cosmType")->getNumSteps();
        for (int k = 0; k < 2; ++k) {
            boxOf("OSC 1")->setSelectedId(k + 1, juce::sendNotificationSync);
            for (int type = 0; type < 2; ++type) {
                setParam(VSynthEmuProcessor::oscParamId(k, "oscType"), type);
                for (int tab = 0; tab <= VSynthEmuEditor::kEffectsTab; ++tab) { editor->showTab(tab); moveAll("OSC " + juce::String(k + 1) + " type " + juce::String(type) + " tab " + juce::String(tab)); }
            }
        }
        boxOf("OSC 1")->setSelectedId(1, juce::sendNotificationSync);
        for (int k = 0; k < 2; ++k) {
            boxOf("COSM 1")->setSelectedId(k + 1, juce::sendNotificationSync);
            for (int tab = 0; tab <= VSynthEmuEditor::kEffectsTab; ++tab) { editor->showTab(tab); moveAll("COSM " + juce::String(k + 1) + " tab " + juce::String(tab)); }   // the COSM's envelope is on another tab
            editor->showTab(3);
            for (int type = 0; type < cosmTypes; ++type) { setParam(k ? "cosm2Type" : "cosmType", type); moveAll("COSM " + juce::String(k + 1) + " type " + juce::String(type)); }
        }
        boxOf("COSM 1")->setSelectedId(1, juce::sendNotificationSync);
        editor->showTab(VSynthEmuEditor::kEffectsTab);
        for (int page = 0; page < 4; ++page) { panel->showPage(page); moveAll("Effects page " + juce::String(page)); }
        panel->showPage(0);
        editor->showTab(6);
        for (int m = 0; m < 8; ++m) { boxOf("Control 1")->setSelectedItemIndex(m, juce::sendNotificationSync); moveAll("Matrix Control " + juce::String(m + 1)); }
        for (int a = 0; a < VSynthEmuProcessor::kAssigns; ++a) { boxOf(VSynthEmuProcessor::assignName(0))->setSelectedItemIndex(a, juce::sendNotificationSync); moveAll(juce::String(VSynthEmuProcessor::assignName(a)) + " assign"); }
        // No editor has shown these (v143's didn't either): the engine's three, shown only without a ROM; Scale Tune; the COSMs' second envelope, the first's velocity and key follow, their LFOs' fade and offset
        const auto hostOnly = [](const juce::String& id) {
            if (id == "pitchShift" || id == "timeStretch" || id == "formantShift" || id.startsWith("scaleTune")) return true;
            for (const char* env : { "itchEnv", "imeEnv", "ormantEnv" })
                for (const char* knob : { "VelCurve", "VelSens", "AttackSens", "DecaySens", "ReleaseSens", "TimeKF" })
                    if (id.endsWith(juce::String(env) + knob)) return true;
            const juce::String c = id.startsWith("cosm2") ? id.substring(5) : id.startsWith("cosm") ? id.substring(4) : juce::String();
            return c.startsWith("EnvB") || c == "EnvAVelCurve" || c == "EnvAVelSens" || c == "EnvAAttackSens" || c == "EnvADecaySens" || c == "EnvAReleaseSens" || c == "EnvATimeKF"
                || c == "LfoOffset" || c == "LfoDelay" || c == "LfoFadeMode" || c == "LfoFade";
        };
        int have = 0;
        for (auto* r : params) {
            if (reached.count(r->paramID)) ++have;
            else if (juce::String(ui) == "list") std::printf("no control: %s\n", r->paramID.toRawUTF8());
            else if (!hostOnly(r->paramID)) fail("no control sets " + r->paramID);
        }
        if (juce::String(ui) == "list") { for (const auto& id : reached) std::printf("control: %s\n", id.toRawUTF8()); return 0; }
        // The source bar's two buttons, which no parameter would miss
        for (const char* text : { "Load WAV...", "Load Patch..." }) {
            bool there = false;
            for (auto* child : ed->getChildren())
                if (auto* b = dynamic_cast<juce::TextButton*>(child))
                    if (b->getButtonText() == text && b->isVisible() && !b->getBounds().isEmpty()) there = true;
            if (!there) fail(juce::String(text) + " isn't on the editor");
        }
        // Which build this is, on the header, whole
        {
            const juce::String want = juce::String(xtBuildId()) + "  " + xtBuildTime();
            bool there = false;
            for (auto* child : ed->getChildren())
                if (auto* l = dynamic_cast<juce::Label*>(child))
                    if (l->getText() == want && l->isVisible() && !l->getBounds().isEmpty()) there = true;
            if (!there) fail("the build, " + want + ", isn't on the editor");
        }
        // The layout, at each size
        const juce::ComponentBoundsConstrainer* limits = ed->getConstrainer();
        const juce::Point<int> sizes[] = { { limits->getMinimumWidth(), limits->getMinimumHeight() }, { ed->getWidth(), ed->getHeight() }, { limits->getMaximumWidth(), limits->getMaximumHeight() } };
        for (const auto& size : sizes) {
            ed->setSize(size.x, size.y);
            const juce::String at = juce::String(size.x) + "x" + juce::String(size.y);
            for (int type = 0; type < 2; ++type) {
                setParam("oscType", type);
                for (int tab = 0; tab <= VSynthEmuEditor::kEffectsTab; ++tab) { editor->showTab(tab); check(at + " tab " + juce::String(tab) + " OSC type " + juce::String(type)); }
            }
            editor->showTab(3);
            for (int type = 0; type < cosmTypes; ++type) { setParam("cosmType", type); check(at + " COSM type " + juce::String(type)); }
            setParam("cosmType", 0);
            // The Effects tab: each page, and each block's types, as what's on a page follows them
            editor->showTab(VSynthEmuEditor::kEffectsTab);
            static const char* const typeOf[] = { "fxMfxType", "fxChoType", "fxRevType" };
            for (int page = 0; page < 4; ++page) {
                const int types = page < 3 ? p.apvts.getParameter(typeOf[page])->getNumSteps() : 1;
                for (int type = 0; type < types; ++type) {
                    if (page < 3) setParam(typeOf[page], type);
                    panel->showPage(page);
                    const juce::String where = at + " Effects page " + juce::String(page) + " type " + juce::String(type);
                    check(where);
                    checkList(where);
                    if (view->getHeight() < 15 * 24) fail(where + ": the list shows only " + juce::String(view->getHeight() / 24) + " rows");
                }
                if (page < 3) setParam(typeOf[page], 0);
            }
            panel->showPage(0);
        }
        // Each tab by a click, in order, at the smallest width
        ed->setSize(sizes[0].x, sizes[0].y);
        editor->showTab(0);
        if (juce::Component* bar = ed->findChildWithID("tabs")) {
            int seen = 0;
            for (int x = 0; x < bar->getWidth(); x += 4) {
                const juce::Point<float> at((float)x, (float)bar->getHeight() / 2.0f);
                const juce::Time now = juce::Time::getCurrentTime();
                bar->mouseDown(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, {}, juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
                                                juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY, bar, bar, now, at, now, 1, false));
                if (editor->currentTab() == seen + 1) ++seen;
                else if (editor->currentTab() != seen) fail("the click at x " + juce::String(x) + " opens tab " + juce::String(editor->currentTab()) + " after tab " + juce::String(seen));
            }
            if (seen != VSynthEmuEditor::kEffectsTab) fail("clicks reach only as far as tab " + juce::String(seen));
        } else fail("no tab bar");
        // The joined buttons drive their boxes, and follow them
        for (const char* first : { "OSC 1", "COSM 1" }) {
            juce::Button* one = buttonOf(first);
            juce::Button* two = buttonOf(juce::String(first).upToFirstOccurrenceOf(" ", false, false) + " 2");
            juce::ComboBox* box = boxOf(first);
            if (!one || !two || !box) { fail(juce::String("no selector for ") + first); continue; }
            two->setToggleState(true, juce::sendNotificationSync);
            if (box->getSelectedId() != 2) fail(juce::String("the second button of ") + first + " doesn't set its box");
            box->setSelectedId(1, juce::sendNotificationSync);
            if (!one->getToggleState()) fail(juce::String("the first button of ") + first + " doesn't follow its box");
        }
        if (juce::Button* mono = buttonOf("MONO"); mono && buttonOf("POLY")) {
            const bool poly = buttonOf("POLY")->getToggleState();
            juce::Button* other = poly ? mono : buttonOf("POLY");
            if (!changedBy([other] { other->setToggleState(true, juce::sendNotificationSync); })) fail("MONO/POLY set no parameter");
            if (buttonOf("POLY")->getToggleState() != poly) fail("MONO/POLY doesn't follow its parameter");
        } else fail("no MONO/POLY");
        if (juce::Button* chorus = buttonOf("Chorus"); chorus && buttonOf("System")) {
            chorus->setToggleState(true, juce::sendNotificationSync);
            if (panel->page() != 1) fail("the Effects tab's Chorus button doesn't open its page");
            panel->showPage(3);
            if (!buttonOf("System")->getToggleState()) fail("the Effects tab's System button doesn't follow its page");
        } else fail("no page buttons on the Effects tab");
        if (errors) { std::fprintf(stderr, "layout: %d errors\n", errors); return 1; }
        std::printf("the layout: %d states at 3 sizes, %d placements, tabs by click; %d of %d parameters have a control\n", states, checked, have, (int)params.size());
        return 0;
    }
    // TEST_FX_PANEL=1: every page and type of the effects' panel, then stop: it lists the catalog's parameters by name and range, shows each one's value, and a slider sets its parameter.  And what
    // each box says, at each end of the range and the default, against the firmware's own routine, run here with its unit and without; a value's text typed into the box sets it.
    if (std::getenv("TEST_FX_PANEL")) {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        FxPanel* panel = nullptr;
        for (juce::Component* ch : ed->getChildren()) if (auto* fp = dynamic_cast<FxPanel*>(ch)) panel = fp;
        juce::Viewport* view = nullptr;
        if (panel) for (juce::Component* ch : panel->getChildren()) if (auto* v = dynamic_cast<juce::Viewport*>(ch)) view = v;
        if (!view) { std::fprintf(stderr, "panel: not found\n"); return 1; }
        panel->setVisible(true);
        const XtFxCatalog& cat = p.fxCatalog();
        if (!cat.ok) { std::fprintf(stderr, "panel: no catalog\n"); return 1; }
        int checked = 0, texts = 0;
        Sh4 fw;
        if (!fw.loadRom(argv[1])) { std::fprintf(stderr, "panel: the firmware's text: no ROM\n"); return 1; }
        fw.mapRam(0x8C000000, 0x00100000);
        const auto routine = [&](uint32_t fn, int id, int v) {
            uint32_t at = 0; std::string t;
            if (!fw.call(fn, { (uint32_t)id, (uint32_t)v }, 0x8C0E0000, &at, 2000000)) return std::string("(a fault)");
            for (uint8_t ch = fw.rd8(at); ch; ch = fw.rd8(++at)) t += (char)ch;
            return t;
        };
        const auto firmwareText = [&](int id, int v) {
            // The Reverb Times read as the driver's table has them, in samples at 44.1 kHz (the user's decision).
            for (const int time : { 0x814, 0x815, 0x828, 0x839, 0x84b, 0x84c, 0x861, 0x872, 0x884, 0x896, 0x897 })
                if (id == time) return (juce::String((int)(fw.rd32(0x897ac574 + 4 * (uint32_t)v) / 44100.0 * 100.0 + 0.5) / 100.0, 2) + " sec").toStdString();
            // The four the routine has no format for go by an ID of the same range that has one (the user's decision), and keep their own units.
            static const std::map<int, int> borrowed = { { 0x864, 0x82b }, { 0x875, 0x83c }, { 0x887, 0x83c }, { 0x898, 0x84d } };
            const auto b = borrowed.find(id);
            const std::string bare = routine(0x89462120, b != borrowed.end() ? b->second : id, v), own = routine(0x89462120, id, v);
            std::string unit = routine(0x89462130, id, v);                                                                     // with the unit
            unit = unit.size() > own.size() ? unit.substr(own.size() + 1, unit.size() - own.size() - 2) : std::string();       // what the unit adds, its brackets off
            if (id == 0x8b6) unit = "ms"; else if (id == 0x8b7) unit = "%"; else if (id == 0x8b8) unit = "Hz";                 // the three of Non-Linear whose strings are a row out
            const std::map<int, const char*> notes = { { 0x11, "1/16" }, { 0x12, "1/8" }, { 0x18, "1/4" }, { 0x13, "1/2" }, { 0x14, "1/1" }, { 0x15, "2/1" } };            // measured by xt_fx control
            const auto note = notes.find((uint8_t)bare[0]);
            if (note != notes.end()) return note->second + std::string(bare.size() > 1 && bare[1] == 0x17 ? "T" : bare.size() > 1 && bare[1] == '.' ? "." : "");
            juce::String t = juce::String(bare).trim();
            return (t.containsOnly("+-.0123456789") && unit.size() ? t + " " + juce::String(unit) : t).toStdString();          // a unit only on a number: THRU isn't in Hz
        };
        const auto boxOf = [](juce::Slider& s) { juce::Label* box = nullptr; for (juce::Component* ch : s.getChildren()) if (auto* l = dynamic_cast<juce::Label*>(ch)) box = l; return box; };
        const auto boxSays = [&](juce::Slider& s, int id, int v, const char* where) {
            texts++;
            const std::string want = firmwareText(id, v), says = boxOf(s) ? boxOf(s)->getText().toStdString() : "(no box)";
            if (says == want) return true;
            std::fprintf(stderr, "panel: ID %x at %d says \"%s\" against the firmware's \"%s\", %s\n", id, v, says.c_str(), want.c_str(), where);
            return false;
        };
        const auto page = [&](int pg, const char* typeParam, int type, const std::vector<XtFxParam>& list) {
            if (typeParam) { auto* prm = p.apvts.getParameter(typeParam); prm->setValueNotifyingHost(prm->convertTo0to1((float)type)); }
            panel->showPage(pg);
            std::vector<juce::Slider*> sliders; std::vector<juce::Label*> labels;
            for (juce::Component* ch : view->getViewedComponent()->getChildren())
                if (ch->isVisible()) { if (auto* sl = dynamic_cast<juce::Slider*>(ch)) sliders.push_back(sl); else if (auto* lb = dynamic_cast<juce::Label*>(ch)) labels.push_back(lb); }
            if (sliders.size() != list.size() || labels.size() != list.size()) { std::fprintf(stderr, "panel: page %d type %d lists %zu, %zu names, against %zu\n", pg, type, sliders.size(), labels.size(), list.size()); return false; }
            if (view->getViewedComponent()->getHeight() < 24 * (int)list.size()) { std::fprintf(stderr, "panel: page %d type %d doesn't fit its rows\n", pg, type); return false; }
            for (size_t i = 0; i < list.size(); ++i) {
                const XtFxParam& q = list[i];
                if (labels[i]->getText().toStdString() != q.name || sliders[i]->getMinimum() != q.low || sliders[i]->getMaximum() != q.high || (int)sliders[i]->getValue() != q.def) { std::fprintf(stderr, "panel: page %d type %d row %zu shows %s %g..%g %g against %s %d..%d %d\n", pg, type, i, labels[i]->getText().toRawUTF8(), sliders[i]->getMinimum(), sliders[i]->getMaximum(), sliders[i]->getValue(), q.name.c_str(), q.low, q.high, q.def); return false; }
                if (!boxSays(*sliders[i], q.id, q.def, "at its default on showing")) return false;
                const int to = q.def == q.high ? q.low : q.high;
                sliders[i]->setValue(to, juce::sendNotificationSync);
                if (!boxSays(*sliders[i], q.id, to, "moved to an end")) return false;
                const int other = to == q.low ? q.high : q.low;
                sliders[i]->setValue(other, juce::sendNotificationSync);
                if (!boxSays(*sliders[i], q.id, other, "moved to the other end")) return false;
                sliders[i]->setValue(to, juce::sendNotificationSync);
                if (p.fxParam(q.id, INT_MIN) != to) { std::fprintf(stderr, "panel: %s's slider didn't set ID %x\n", q.name.c_str(), q.id); return false; }
                panel->showPage(pg);
                if ((int)sliders[i]->getValue() != to) { std::fprintf(stderr, "panel: %s's slider didn't keep its value\n", q.name.c_str()); return false; }
                p.setFxParam(q.id, q.def);                                  // as a host's saved state does
                panel->showPage(pg);
                if ((int)sliders[i]->getValue() != q.def) { std::fprintf(stderr, "panel: %s's slider doesn't follow ID %x\n", q.name.c_str(), q.id); return false; }
                if (!boxSays(*sliders[i], q.id, q.def, "back from the processor")) return false;
                // The other end's text, typed into the box.  What reaches the processor is its number.
                if (juce::Label* box = boxOf(*sliders[i])) {
                    box->setText(firmwareText(q.id, to), juce::sendNotificationSync);
                    if (p.fxParam(q.id, INT_MIN) != to) { std::fprintf(stderr, "panel: \"%s\" typed into %s's box sets ID %x to %d, not %d\n", firmwareText(q.id, to).c_str(), q.name.c_str(), q.id, p.fxParam(q.id, INT_MIN), to); return false; }
                    if (!boxSays(*sliders[i], q.id, to, "typed")) return false;
                    p.setFxParam(q.id, q.def);
                    panel->showPage(pg);
                }
                checked++;
            }
            return true;
        };
        for (int t = 0; t < 42; ++t) if (!page(0, "fxMfxType", t, cat.mfx[(size_t)t])) return 1;
        for (int t = 0; t < 9; ++t) if (!page(1, "fxChoType", t, cat.chorus[(size_t)t])) return 1;
        for (int t = 0; t < 11; ++t) if (!page(2, "fxRevType", t, cat.reverb[(size_t)t])) return 1;
        if (!page(3, nullptr, 0, cat.eq)) return 1;
        // A page's change alone, with each block on the same type number.
        for (int t = 1; t < 4; ++t) { if (!page(0, "fxMfxType", t, cat.mfx[(size_t)t]) || !page(1, "fxChoType", t, cat.chorus[(size_t)t]) || !page(2, "fxRevType", t, cat.reverb[(size_t)t])) return 1; }
        // A Rate and a Delay Time through all their values, so that every note value goes through a box, and every Reverb Time.
        const auto sweepOn = [&](const char* typeParam, int pg, const std::vector<std::vector<XtFxParam>>& lists, int type, int id) {
            auto* prm = p.apvts.getParameter(typeParam); prm->setValueNotifyingHost(prm->convertTo0to1((float)type));
            panel->showPage(pg);
            std::vector<juce::Slider*> sliders;
            for (juce::Component* ch : view->getViewedComponent()->getChildren()) if (ch->isVisible()) if (auto* sl = dynamic_cast<juce::Slider*>(ch)) sliders.push_back(sl);
            const std::vector<XtFxParam>& list = lists[(size_t)type];
            for (size_t i = 0; i < list.size() && i < sliders.size(); ++i) if (list[i].id == id) {
                for (int v = list[i].low; v <= list[i].high; ++v) { sliders[i]->setValue(v, juce::sendNotificationSync); if (!boxSays(*sliders[i], id, v, "swept")) return false; }
                p.setFxParam(id, list[i].def);
                return true;
            }
            std::fprintf(stderr, "panel: ID %x isn't on page %d type %d\n", id, pg, type);
            return false;
        };
        if (!sweepOn("fxMfxType", 0, cat.mfx, 3, 0x57a) || !sweepOn("fxMfxType", 0, cat.mfx, 11, 0x5de)) return 1;
        for (const auto& [type, id] : std::initializer_list<std::pair<int, int>>{ { 1, 0x814 }, { 1, 0x815 }, { 2, 0x828 }, { 3, 0x839 }, { 4, 0x84b }, { 4, 0x84c }, { 5, 0x861 }, { 6, 0x872 }, { 7, 0x884 }, { 8, 0x896 }, { 8, 0x897 } })
            if (!sweepOn("fxRevType", 2, cat.reverb, type, id)) return 1;
        // Output Gain is a host's parameter, in the panel's own row, and the one level with a unit.
        {
            juce::Slider* gain = nullptr;
            for (juce::Component* ch : panel->getChildren()) if (auto* sl = dynamic_cast<juce::Slider*>(ch)) gain = sl;   // the last of the levels
            auto* prm = p.apvts.getParameter("fxGain");
            if (!gain || !boxSays(*gain, 0x76, (int)prm->convertFrom0to1(prm->getValue()), "Output Gain as it opened")) return 1;
            for (const int v : { -12, 3, 12 }) { prm->setValueNotifyingHost(prm->convertTo0to1((float)v)); if (!boxSays(*gain, 0x76, v, "Output Gain")) return 1; }
            if (juce::Label* box = boxOf(*gain)) {
                box->setText("+5 dB", juce::sendNotificationSync);
                if ((int)prm->convertFrom0to1(prm->getValue()) != 5) { std::fprintf(stderr, "panel: \"+5 dB\" typed into Output Gain's box doesn't set it\n"); return 1; }
                if (!boxSays(*gain, 0x76, 5, "Output Gain typed")) return 1;
            }
        }
        std::printf("the panel: %d parameters as the catalog has them, %d of the boxes' texts as the firmware's\n", checked, texts);
        return 0;
    }
    if (argc > 4) {   // Encode Type (choice index 0 Auto, 1 SOLO, 2 BACKING, 3 ENSEMBLE, 4 LITE)
        const juce::String t(argv[4]);
        const int idx = t == "solo" ? 1 : t == "backing" ? 2 : t == "ensemble" ? 3 : t == "lite" ? 4 : 0;
        auto* prm = p.apvts.getParameter("encodeType");
        prm->setValueNotifyingHost(prm->convertTo0to1((float)idx));
    }
    // TEST_ADSR="a,d,s,r[,aSens,dSens,rSens,timeKF]": amp envelope knobs for the
    // processor and the reference
    XtSynth::Controls refCtl;
    if (const char* e = std::getenv("TEST_ADSR")) {
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &refCtl.attack, &refCtl.decay, &refCtl.sustain, &refCtl.release,
                    &refCtl.aSens, &refCtl.dSens, &refCtl.rSens, &refCtl.timeKF);
        const std::pair<const char*, int> knobs[] = { { "ampAttack", refCtl.attack }, { "ampDecay", refCtl.decay },
                                                      { "ampSustain", refCtl.sustain }, { "ampRelease", refCtl.release },
                                                      { "ampAttackSens", refCtl.aSens }, { "ampDecaySens", refCtl.dSens },
                                                      { "ampReleaseSens", refCtl.rSens }, { "ampTimeKF", refCtl.timeKF } };
        for (auto [id, v] : knobs) {
            auto* prm = p.apvts.getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
    }
    // TEST_LFO="wave,rate,depth,delay,mode,fade,offset,keysync": the LFO
    if (const char* e = std::getenv("TEST_LFO")) {
        int ks = 1;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &refCtl.lfoWave, &refCtl.lfoRate, &refCtl.lfoPitchDepth, &refCtl.lfoDelay,
                    &refCtl.lfoFadeMode, &refCtl.lfoFade, &refCtl.lfoOffset, &ks);
        refCtl.lfoKeySync = ks != 0;
        const std::pair<const char*, int> knobs[] = { { "lfoWave", refCtl.lfoWave }, { "lfoRate", refCtl.lfoRate },
                                                      { "lfoPitchDepth", refCtl.lfoPitchDepth }, { "lfoDelay", refCtl.lfoDelay },
                                                      { "lfoFadeMode", refCtl.lfoFadeMode }, { "lfoFade", refCtl.lfoFade },
                                                      { "lfoOffset", refCtl.lfoOffset }, { "lfoKeySync", ks } };
        for (auto [id, v] : knobs) {
            auto* prm = p.apvts.getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
    }
    // TEST_PENV="a,d,s,r,depth": the pitch envelope
    if (const char* e = std::getenv("TEST_PENV")) {
        std::sscanf(e, "%d,%d,%d,%d,%d", &refCtl.pAttack, &refCtl.pDecay, &refCtl.pSustain, &refCtl.pRelease, &refCtl.pDepth);
        const std::pair<const char*, int> knobs[] = { { "pitchEnvAttack", refCtl.pAttack }, { "pitchEnvDecay", refCtl.pDecay },
                                                      { "pitchEnvSustain", refCtl.pSustain }, { "pitchEnvRelease", refCtl.pRelease },
                                                      { "pitchEnvDepth", refCtl.pDepth } };
        for (auto [id, v] : knobs) {
            auto* prm = p.apvts.getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
    }
    // TEST_OSC="time,offset,timeKF,formant,formantKF,energy[,lfoTime,lfoFormant]":
    // the VariPhrase OSC knobs (Time Offset 0 BWD, 1 ZERO, 2 FWD) and the LFO's
    // time and formant depths
    if (const char* e = std::getenv("TEST_OSC")) {
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &refCtl.oscTime, &refCtl.oscTimeOffset, &refCtl.oscTimeKF, &refCtl.oscFormant,
                    &refCtl.oscFormantKF, &refCtl.energy, &refCtl.lfoTimeDepth, &refCtl.lfoFormantDepth);
        const std::pair<const char*, int> knobs[] = { { "oscTime", refCtl.oscTime }, { "oscTimeOffset", refCtl.oscTimeOffset },
                                                      { "oscTimeKF", refCtl.oscTimeKF }, { "oscFormant", refCtl.oscFormant },
                                                      { "oscFormantKF", refCtl.oscFormantKF }, { "energy", refCtl.energy },
                                                      { "lfoTimeDepth", refCtl.lfoTimeDepth }, { "lfoFormantDepth", refCtl.lfoFormantDepth } };
        for (auto [id, v] : knobs) {
            auto* prm = p.apvts.getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
    }
    // TEST_ANALOG="wave,impact,pw,pwKf,pwLfo,pwEnv,fat,fatKf,fatLfo,fatEnv[,detune[,subOctave,subLevel]]": OSC
    // Type ANALOG, the analog oscillator's knobs (KFs in %; PW on the time
    // envelope, Fat on the formant envelope)
    if (const char* e = std::getenv("TEST_ANALOG")) {
        XtAnalogParams& a = refCtl.ana;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &a.wave, &a.impact, &a.pw, &a.pwKf, &a.pwLfo, &a.pwEnv, &a.fat, &a.fatKf, &a.fatLfo,
                    &a.fatEnv, &a.detune, &a.subOctave, &a.subLevel);
        if (a.pwKf % 10 || a.fatKf % 10) {           // the parameters step by 10%: another value would snap and differ from refCtl
            std::fprintf(stderr, "TEST_ANALOG: the KFs must be multiples of 10 (%%)\n");
            return 1;
        }
        refCtl.oscType = 0;
        const std::pair<const char*, int> knobs[] = { { "oscType", 0 }, { "anaWave", a.wave }, { "anaImpact", a.impact }, { "anaPw", a.pw },
                                                      { "anaPwKF", a.pwKf }, { "anaPwLfo", a.pwLfo }, { "anaPwEnv", a.pwEnv }, { "anaFat", a.fat },
                                                      { "anaFatKF", a.fatKf }, { "anaFatLfo", a.fatLfo }, { "anaFatEnv", a.fatEnv },
                                                      { "anaDetune", a.detune }, { "anaSubOct", a.subOctave }, { "anaSubLevel", a.subLevel } };
        for (auto [id, v] : knobs) {
            auto* prm = p.apvts.getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
        a.pwKf /= 10;                                // as the patch stores them
        a.fatKf /= 10;
    }
    // TEST_OSC2="wave,impact,pw,pwKf,pwLfo,pwEnv,fat,fatKf,fatLfo,fatEnv[,detune[,subOctave,subLevel]]": OSC 2 on, ANALOG.
    // TEST_OSC2_PITCH="pitch,pitchKF,coarse,fine,random"; TEST_OSC2_ENV="pa,pd,ps,pr,pdepth,ta,td,ts,tr,tdepth,fa,fd,fs,fr,fdepth";
    // TEST_OSC2_LEV="level,levelKF,levelLfo,gain,oa,od,os,or,oaSens,odSens,orSens,oTimeKF,velSens,velCurve";
    // TEST_OSC2_LFO="wave,rate,pitchDepth,delay,mode,fade,offset,keysync"; TEST_OSC1_OFF=1: OSC 1's Switch off
    // TEST_OSC2_WAVE="file.wav,solo|backing|ensemble": OSC 2 on, PCM, its own wave and Encode Type;
    // TEST_OSC2_OSC="time,offset,timeKF,formant,formantKF,energy"; TEST_OSC2_LOOP="mode,start%,end%";
    // TEST_OSC2_KEY=n (its Original Key); TEST_OSC2_ROBOT=1
    {
        XtSynth::OscControls& o = refCtl.osc2;
        XtAnalogParams& a = o.ana;
        auto set2 = [&](const char* id, int v) {
            auto* prm = p.apvts.getParameter(VSynthEmuProcessor::oscParamId(1, id));
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        };
        if (const char* e = std::getenv("TEST_OSC2")) {
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &a.wave, &a.impact, &a.pw, &a.pwKf, &a.pwLfo, &a.pwEnv, &a.fat, &a.fatKf, &a.fatLfo,
                        &a.fatEnv, &a.detune, &a.subOctave, &a.subLevel);
            if (a.pwKf % 10 || a.fatKf % 10) {
                std::fprintf(stderr, "TEST_OSC2: the KFs must be multiples of 10 (%%)\n");
                return 1;
            }
            o.on = true;
            o.oscType = 0;
            const std::pair<const char*, int> knobs[] = { { "oscSwitch", 1 }, { "oscType", 0 }, { "anaWave", a.wave }, { "anaImpact", a.impact }, { "anaPw", a.pw },
                                                          { "anaPwKF", a.pwKf }, { "anaPwLfo", a.pwLfo }, { "anaPwEnv", a.pwEnv }, { "anaFat", a.fat },
                                                          { "anaFatKF", a.fatKf }, { "anaFatLfo", a.fatLfo }, { "anaFatEnv", a.fatEnv },
                                                          { "anaDetune", a.detune }, { "anaSubOct", a.subOctave }, { "anaSubLevel", a.subLevel } };
            for (auto [id, v] : knobs) set2(id, v);
            a.pwKf /= 10;
            a.fatKf /= 10;
        }
        if (const char* e = std::getenv("TEST_OSC2_PITCH")) {
            std::sscanf(e, "%d,%d,%d,%d,%d", &o.oscPitch, &o.pitchKF, &o.oscCoarse, &o.oscFine, &o.randomPitch);
            const std::pair<const char*, int> knobs[] = { { "oscPitch", o.oscPitch }, { "pitchKF", o.pitchKF }, { "oscCoarse", o.oscCoarse }, { "oscFine", o.oscFine },
                                                          { "randomPitch", o.randomPitch } };
            for (auto [id, v] : knobs) set2(id, v);
        }
        if (const char* e = std::getenv("TEST_OSC2_ENV")) {
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &o.pAttack, &o.pDecay, &o.pSustain, &o.pRelease, &o.pDepth, &o.tAttack, &o.tDecay, &o.tSustain,
                        &o.tRelease, &o.tDepth, &o.fAttack, &o.fDecay, &o.fSustain, &o.fRelease, &o.fDepth);
            const std::pair<const char*, int> knobs[] = { { "pitchEnvAttack", o.pAttack }, { "pitchEnvDecay", o.pDecay }, { "pitchEnvSustain", o.pSustain }, { "pitchEnvRelease", o.pRelease },
                                                          { "pitchEnvDepth", o.pDepth }, { "timeEnvAttack", o.tAttack }, { "timeEnvDecay", o.tDecay }, { "timeEnvSustain", o.tSustain },
                                                          { "timeEnvRelease", o.tRelease }, { "timeEnvDepth", o.tDepth }, { "formantEnvAttack", o.fAttack }, { "formantEnvDecay", o.fDecay },
                                                          { "formantEnvSustain", o.fSustain }, { "formantEnvRelease", o.fRelease }, { "formantEnvDepth", o.fDepth } };
            for (auto [id, v] : knobs) set2(id, v);
        }
        if (const char* e = std::getenv("TEST_OSC2_LEV")) {
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &o.oscLevel, &o.oscLevelKF, &o.oscLevelLfoDepth, &o.waveGain, &o.oAttack, &o.oDecay, &o.oSustain, &o.oRelease,
                        &o.oASens, &o.oDSens, &o.oRSens, &o.oTimeKF, &o.oscVelSens, &o.oscVelCurve);
            const std::pair<const char*, int> knobs[] = { { "oscLevel", o.oscLevel }, { "oscLevelKF", o.oscLevelKF }, { "oscLevelLfoDepth", o.oscLevelLfoDepth }, { "waveGain", o.waveGain },
                                                          { "oscEnvAttack", o.oAttack }, { "oscEnvDecay", o.oDecay }, { "oscEnvSustain", o.oSustain }, { "oscEnvRelease", o.oRelease },
                                                          { "oscEnvAttackSens", o.oASens }, { "oscEnvDecaySens", o.oDSens }, { "oscEnvReleaseSens", o.oRSens }, { "oscEnvTimeKF", o.oTimeKF },
                                                          { "oscVelSens", o.oscVelSens }, { "oscVelCurve", o.oscVelCurve } };
            for (auto [id, v] : knobs) set2(id, v);
        }
        if (const char* e = std::getenv("TEST_OSC2_LFO")) {
            int ks = 1;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &o.lfoWave, &o.lfoRate, &o.lfoPitchDepth, &o.lfoDelay, &o.lfoFadeMode, &o.lfoFade, &o.lfoOffset, &ks);
            o.lfoKeySync = ks != 0;
            const std::pair<const char*, int> knobs[] = { { "lfoWave", o.lfoWave }, { "lfoRate", o.lfoRate }, { "lfoPitchDepth", o.lfoPitchDepth }, { "lfoDelay", o.lfoDelay },
                                                          { "lfoFadeMode", o.lfoFadeMode }, { "lfoFade", o.lfoFade }, { "lfoOffset", o.lfoOffset }, { "lfoKeySync", ks } };
            for (auto [id, v] : knobs) set2(id, v);
        }
        if (const char* e = std::getenv("TEST_OSC2_OSC")) {
            std::sscanf(e, "%d,%d,%d,%d,%d,%d", &o.oscTime, &o.oscTimeOffset, &o.oscTimeKF, &o.oscFormant, &o.oscFormantKF, &o.energy);
            const std::pair<const char*, int> knobs[] = { { "oscTime", o.oscTime }, { "oscTimeOffset", o.oscTimeOffset }, { "oscTimeKF", o.oscTimeKF }, { "oscFormant", o.oscFormant },
                                                          { "oscFormantKF", o.oscFormantKF }, { "energy", o.energy } };
            for (auto [id, v] : knobs) set2(id, v);
        }
        if (const char* e = std::getenv("TEST_OSC2_LOOP")) {
            float a = 0, b = 100;
            std::sscanf(e, "%d,%f,%f", &o.loopMode, &a, &b);
            o.loop = true; o.loopStart = a / 100.0f; o.loopEnd = b / 100.0f;
            set2("loop", 1); set2("loopMode", o.loopMode);
            for (auto [id, v] : { std::pair<const char*, float>{ "loopStart", a }, { "loopEnd", b } }) {
                auto* prm = p.apvts.getParameter(VSynthEmuProcessor::oscParamId(1, id));
                prm->setValueNotifyingHost(prm->convertTo0to1(v));
            }
        }
        if (const char* e = std::getenv("TEST_OSC2_KEY")) { o.rootKey = std::atoi(e); set2("rootKey", o.rootKey); }
        if (std::getenv("TEST_OSC2_ROBOT") && std::atoi(std::getenv("TEST_OSC2_ROBOT")) != 0) { o.robot = true; set2("robot", 1); }
        if (std::getenv("TEST_OSC2_VARI") && std::atoi(std::getenv("TEST_OSC2_VARI")) == 0) { o.vari = false; set2("vari", 0); }
        if (const char* e = std::getenv("TEST_OSC2_START")) { o.startOffset = std::atoi(e); set2("startOffset", o.startOffset); }
        if (const char* e = std::getenv("TEST_OSC2_PLAYBACK")) { o.playbackMode = std::clamp(std::atoi(e), 0, 3); set2("playbackMode", o.playbackMode); }
        if (std::getenv("TEST_OSC1_OFF") && std::atoi(std::getenv("TEST_OSC1_OFF")) != 0) {
            refCtl.on = false;
            auto* prm = p.apvts.getParameter("oscSwitch");
            prm->setValueNotifyingHost(prm->convertTo0to1(0.0f));
        }
    }
    // TEST_TENV / TEST_FENV="a,d,s,r,depth": the time and formant envelopes
    for (const char* which : { "TEST_TENV", "TEST_FENV" }) {
        const char* e = std::getenv(which);
        if (!e) continue;
        const bool t = which[5] == 'T';
        int k[5] = { 0, 64, 0, 64, 0 };
        std::sscanf(e, "%d,%d,%d,%d,%d", &k[0], &k[1], &k[2], &k[3], &k[4]);
        int* dst[5] = { t ? &refCtl.tAttack : &refCtl.fAttack, t ? &refCtl.tDecay : &refCtl.fDecay,
                        t ? &refCtl.tSustain : &refCtl.fSustain, t ? &refCtl.tRelease : &refCtl.fRelease,
                        t ? &refCtl.tDepth : &refCtl.fDepth };
        static const char* ids[2][5] = { { "formantEnvAttack", "formantEnvDecay", "formantEnvSustain", "formantEnvRelease", "formantEnvDepth" },
                                         { "timeEnvAttack", "timeEnvDecay", "timeEnvSustain", "timeEnvRelease", "timeEnvDepth" } };
        for (int i = 0; i < 5; ++i) {
            *dst[i] = k[i];
            auto* prm = p.apvts.getParameter(ids[t][i]);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)k[i]));
        }
    }
    // TEST_OLEV="level,levelKF,lfoDepth,waveGainDb", TEST_OENV="a,d,s,r,aSens,dSens,rSens,timeKF",
    // TEST_VEL="ampSens,ampCurve,oscSens,oscCurve": the OSC TVA and velocity to level
    auto setKnobs = [&](std::initializer_list<std::pair<const char*, int>> knobs) {
        for (auto [id, v] : knobs) {
            auto* prm = p.apvts.getParameter(id);
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
    };
    if (const char* e = std::getenv("TEST_OLEV")) {
        std::sscanf(e, "%d,%d,%d,%d", &refCtl.oscLevel, &refCtl.oscLevelKF, &refCtl.oscLevelLfoDepth, &refCtl.waveGain);
        setKnobs({ { "oscLevel", refCtl.oscLevel }, { "oscLevelKF", refCtl.oscLevelKF }, { "oscLevelLfoDepth", refCtl.oscLevelLfoDepth },
                   { "waveGain", refCtl.waveGain } });
    }
    if (const char* e = std::getenv("TEST_OENV")) {
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &refCtl.oAttack, &refCtl.oDecay, &refCtl.oSustain, &refCtl.oRelease,
                    &refCtl.oASens, &refCtl.oDSens, &refCtl.oRSens, &refCtl.oTimeKF);
        setKnobs({ { "oscEnvAttack", refCtl.oAttack }, { "oscEnvDecay", refCtl.oDecay }, { "oscEnvSustain", refCtl.oSustain },
                   { "oscEnvRelease", refCtl.oRelease }, { "oscEnvAttackSens", refCtl.oASens }, { "oscEnvDecaySens", refCtl.oDSens },
                   { "oscEnvReleaseSens", refCtl.oRSens }, { "oscEnvTimeKF", refCtl.oTimeKF } });
    }
    if (const char* e = std::getenv("TEST_VEL")) {
        std::sscanf(e, "%d,%d,%d,%d", &refCtl.ampVelSens, &refCtl.ampVelCurve, &refCtl.oscVelSens, &refCtl.oscVelCurve);
        setKnobs({ { "ampVelSens", refCtl.ampVelSens }, { "ampVelCurve", refCtl.ampVelCurve }, { "oscVelSens", refCtl.oscVelSens },
                   { "oscVelCurve", refCtl.oscVelCurve } });
    }
    // TEST_VARI=0: Vari Sw off (OSC 1), so the wave's time follows its pitch
    if (std::getenv("TEST_VARI") && std::atoi(std::getenv("TEST_VARI")) == 0) {
        refCtl.vari = false;
        setKnobs({ { "vari", 0 } });
    }
    // TEST_START=n: OSC 1's Start Offset, n samples into the wave
    if (const char* e = std::getenv("TEST_START")) {
        refCtl.startOffset = std::atoi(e);
        setKnobs({ { "startOffset", refCtl.startOffset } });
    }
    // TEST_PLAYBACK=n: OSC 1's Playback Mode, 0 RETRIGGER, 1 LEGATO, 2 STEP, 3 EVENT (with TEST_NOTE2 for a second note)
    if (const char* e = std::getenv("TEST_PLAYBACK")) {
        refCtl.playbackMode = std::clamp(std::atoi(e), 0, 3);
        setKnobs({ { "playbackMode", refCtl.playbackMode } });
    }
    // TEST_ROBOT=1: Robot Voice
    if (std::getenv("TEST_ROBOT") && std::atoi(std::getenv("TEST_ROBOT")) != 0) {
        refCtl.robot = true;
        setKnobs({ { "robot", 1 } });
    }
    // TEST_LOOP="mode,start%,end%": Loop on, Loop Mode (0 One Shot, 1 One
    // Way, 2 Alternate) and the loop region in % of the wave
    if (const char* e = std::getenv("TEST_LOOP")) {
        float st = 0, en = 100;
        refCtl.loop = true;
        std::sscanf(e, "%d,%f,%f", &refCtl.loopMode, &st, &en);
        auto* ps = p.apvts.getParameter("loopStart");
        ps->setValueNotifyingHost(ps->convertTo0to1(st));
        auto* pe = p.apvts.getParameter("loopEnd");
        pe->setValueNotifyingHost(pe->convertTo0to1(en));
        setKnobs({ { "loop", 1 }, { "loopMode", refCtl.loopMode } });
        // as the processor reads them back: snapped to the 0.01% steps
        refCtl.loopStart = p.apvts.getRawParameterValue("loopStart")->load() / 100.0f;
        refCtl.loopEnd = p.apvts.getRawParameterValue("loopEnd")->load() / 100.0f;
    }
    // TEST_PORTA="mode,type,time,sens": Portamento on, Mode (0 NORMAL,
    // 1 LEGATO), Type (0 RATE, 1 TIME), Time, Time Velo Sens; with
    // TEST_NOTE2="note,frame" a second note glides from the first
    if (const char* e = std::getenv("TEST_PORTA")) {
        refCtl.portamento = true;
        std::sscanf(e, "%d,%d,%d,%d", &refCtl.portaMode, &refCtl.portaType, &refCtl.portaTime, &refCtl.portaVeloSens);
        setKnobs({ { "portamento", 1 }, { "portaMode", refCtl.portaMode }, { "portaType", refCtl.portaType },
                   { "portaTime", refCtl.portaTime }, { "portaVeloSens", refCtl.portaVeloSens } });
    }
    // TEST_COSM="filter,cutoff,reso,kf,cutEnv,cutLfo,resoEnv,resoLfo[,type,
    // resoOwn,cutOwn]": COSM 1, a TVF (type 0) or Dynamic TVF (1); TEST_CENV="a,d,s,r,velCurve,velSens": its cutoff envelope
    // (Env A); TEST_CLFO="wave,rate": its LFO.  TEST_COSM2, TEST_CENV2,
    // TEST_CLFO2: the same for COSM 2.  TEST_DUAL="type,freq1,kf1,q1,lfo1,env1,
    // freq2,kf2,q2,lfo2,env2": COSM 1 a Dual Filter (KF in %); TEST_CENVB its
    // Env B (Filter 2's); TEST_DUAL2, TEST_CENVB2: COSM 2's.  TEST_TB="cutoff,kf,
    // lfo,env,q,qFollow,hpf,hpfKf,hpfLfo,hpfEnv,gain": a TB Filter (KF in %);
    // TEST_TB2: COSM 2's.  TEST_COMB="octave,detune,detuneLfo,detuneEnv,tone,
    // toneLfo,toneEnv": a Comb Filter (octave 0..4); TEST_COMB2: COSM 2's.
    // TEST_SBF1="hpf,lpf,lpfKf,subDetune,octave,detune,detuneLfo,detuneOwn,
    // detuneEnv,width,widthKf,widthLfo,widthOwn,widthEnv": the 1st Side Band
    // Filter (KFs in %, octave 0..2); TEST_SBF2="q,lpf,…" (no subDetune) the
    // 2nd; TEST_SBF12, TEST_SBF22: COSM 2's.  TEST_RES="type,size,sizeKf,
    // sizeLfo,sizeEnv,balance,balanceLfo,balanceEnv": a Resonator (KF in %);
    // TEST_RES2: COSM 2's.  TEST_FS="depth,depthKf,depthLfo,depthEnv,balance,
    // balanceLfo,balanceEnv": a Frequency Shifter (KF in %); TEST_FS2: COSM 2's.
    // TEST_LOFI="bit,rate,filter,filterKf,filterLfo,filterEnv,balance,
    // balanceLfo,balanceEnv": Lo-Fi (KF in %); TEST_LOFI2: COSM 2's.
    // TEST_COMP="attack,sustain,level": a Compressor; TEST_COMP2: COSM 2's.
    // TEST_LIM="threshold,ratio,attack,release,level": a Limiter (ratio 0..3);
    // TEST_LIM2: COSM 2's.  TEST_WS="type,level,drive,driveLfo,driveEnv,tone,
    // toneKf,toneLfo,toneEnv": Wave Shape (type 0..5, KF in %); TEST_WS2: COSM 2's.
    // TEST_OD="type,…": OD/Dist (type 0 OD, 1 DS; the rest as TEST_WS); TEST_OD2.
    // TEST_AMP="type,bass,middle,treble,level,gain,gainLfo,gainEnv": the Amp
    // Simulator (type 0..2); TEST_AMP2: COSM 2's.  TEST_SPK="type": the
    // Speaker Simulator (type 0..11); TEST_SPK2: COSM 2's.
    for (int k = 0; k < 2; ++k) {
        auto& m = refCtl.cosm[k];
        const std::string sfx = k ? "2" : "", pre = k ? "cosm2" : "cosm";
        auto set = [&](const char* name, int v) {
            auto* prm = p.apvts.getParameter(juce::String(pre + name));
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        };
        if (const char* e = std::getenv(("TEST_COSM" + sfx).c_str())) {
            m.on = true;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.filter, &m.cutoff, &m.reso, &m.kf, &m.cutoffEnv, &m.cutoffLfo, &m.resoEnv,
                        &m.resoLfo, &m.type, &m.resoOwn, &m.cutoffOwn);
            set("Switch", 1); set("Type", m.type); set("ResoOwn", m.resoOwn); set("CutoffOwn", m.cutoffOwn); set("Filter", m.filter); set("Cutoff", m.cutoff); set("Reso", m.reso); set("CutoffKF", m.kf);
            set("CutoffEnv", m.cutoffEnv); set("CutoffLfo", m.cutoffLfo); set("ResoEnv", m.resoEnv); set("ResoLfo", m.resoLfo);
        }
        if (const char* e = std::getenv(("TEST_DUAL" + sfx).c_str())) {
            m.on = true;
            m.type = 2;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.dualType, &m.freq1, &m.kf1, &m.q1, &m.lfo1, &m.env1, &m.freq2, &m.kf2,
                        &m.q2, &m.lfo2, &m.env2);
            set("Switch", 1); set("Type", 2); set("DualType", m.dualType);
            set("Dual1Freq", m.freq1); set("Dual1KF", m.kf1); set("Dual1Q", m.q1); set("Dual1Lfo", m.lfo1); set("Dual1Env", m.env1);
            set("Dual2Freq", m.freq2); set("Dual2KF", m.kf2); set("Dual2Q", m.q2); set("Dual2Lfo", m.lfo2); set("Dual2Env", m.env2);
        }
        if (const char* e = std::getenv(("TEST_TB" + sfx).c_str())) {
            m.on = true;
            m.type = 3;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.tbCutoff, &m.tbKf, &m.tbLfo, &m.tbEnv, &m.tbQ, &m.tbQFollow, &m.tbHpf,
                        &m.tbHpfKf, &m.tbHpfLfo, &m.tbHpfEnv, &m.tbGain);
            set("Switch", 1); set("Type", 3);
            set("TbCutoff", m.tbCutoff); set("TbKF", m.tbKf); set("TbLfo", m.tbLfo); set("TbEnv", m.tbEnv); set("TbQ", m.tbQ);
            set("TbQFollow", m.tbQFollow); set("TbHpf", m.tbHpf); set("TbHpfKF", m.tbHpfKf); set("TbHpfLfo", m.tbHpfLfo);
            set("TbHpfEnv", m.tbHpfEnv); set("TbGain", m.tbGain);
        }
        if (const char* e = std::getenv(("TEST_COMB" + sfx).c_str())) {
            m.on = true;
            m.type = 4;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &m.combOctave, &m.combDetune, &m.combDetuneLfo, &m.combDetuneEnv, &m.combTone, &m.combToneLfo,
                        &m.combToneEnv);
            set("Switch", 1); set("Type", 4); set("CombOctave", m.combOctave); set("CombDetune", m.combDetune);
            set("CombDetuneLfo", m.combDetuneLfo); set("CombDetuneEnv", m.combDetuneEnv); set("CombTone", m.combTone);
            set("CombToneLfo", m.combToneLfo); set("CombToneEnv", m.combToneEnv);
        }
        if (const char* e = std::getenv(("TEST_COMP" + sfx).c_str())) {
            m.on = true;
            m.type = 10;
            std::sscanf(e, "%d,%d,%d", &m.compAttack, &m.compSustain, &m.compLevel);
            set("Switch", 1); set("Type", 10); set("CompAttack", m.compAttack); set("CompSustain", m.compSustain); set("CompLevel", m.compLevel);
        }
        if (const char* e = std::getenv(("TEST_SPK" + sfx).c_str())) {
            m.on = true;
            m.type = 15;
            m.spkType = std::atoi(e);
            set("Switch", 1); set("Type", 15); set("SpkType", m.spkType);
        }
        if (const char* e = std::getenv(("TEST_AMP" + sfx).c_str())) {
            m.on = true;
            m.type = 14;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &m.ampType, &m.ampBass, &m.ampMiddle, &m.ampTreble, &m.ampLevel, &m.ampGain,
                        &m.ampGainLfo, &m.ampGainEnv);
            set("Switch", 1); set("Type", 14); set("AmpType", m.ampType); set("AmpBass", m.ampBass); set("AmpMiddle", m.ampMiddle);
            set("AmpTreble", m.ampTreble); set("AmpLevel", m.ampLevel); set("AmpGain", m.ampGain); set("AmpGainLfo", m.ampGainLfo);
            set("AmpGainEnv", m.ampGainEnv);
        }
        if (const char* e = std::getenv(("TEST_OD" + sfx).c_str())) {
            m.on = true;
            m.type = 13;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.odType, &m.odLevel, &m.odDrive, &m.odDriveLfo, &m.odDriveEnv, &m.odTone,
                        &m.odToneKf, &m.odToneLfo, &m.odToneEnv);
            set("Switch", 1); set("Type", 13); set("OdType", m.odType); set("OdLevel", m.odLevel); set("OdDrive", m.odDrive);
            set("OdDriveLfo", m.odDriveLfo); set("OdDriveEnv", m.odDriveEnv); set("OdTone", m.odTone); set("OdToneKF", m.odToneKf);
            set("OdToneLfo", m.odToneLfo); set("OdToneEnv", m.odToneEnv);
        }
        if (const char* e = std::getenv(("TEST_WS" + sfx).c_str())) {
            m.on = true;
            m.type = 12;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.wsType, &m.wsLevel, &m.wsDrive, &m.wsDriveLfo, &m.wsDriveEnv, &m.wsTone,
                        &m.wsToneKf, &m.wsToneLfo, &m.wsToneEnv);
            set("Switch", 1); set("Type", 12); set("WsType", m.wsType); set("WsLevel", m.wsLevel); set("WsDrive", m.wsDrive);
            set("WsDriveLfo", m.wsDriveLfo); set("WsDriveEnv", m.wsDriveEnv); set("WsTone", m.wsTone); set("WsToneKF", m.wsToneKf);
            set("WsToneLfo", m.wsToneLfo); set("WsToneEnv", m.wsToneEnv);
        }
        if (const char* e = std::getenv(("TEST_LIM" + sfx).c_str())) {
            m.on = true;
            m.type = 11;
            std::sscanf(e, "%d,%d,%d,%d,%d", &m.limThreshold, &m.limRatio, &m.limAttack, &m.limRelease, &m.limLevel);
            set("Switch", 1); set("Type", 11); set("LimThreshold", m.limThreshold); set("LimRatio", m.limRatio); set("LimAttack", m.limAttack);
            set("LimRelease", m.limRelease); set("LimLevel", m.limLevel);
        }
        if (const char* e = std::getenv(("TEST_LOFI" + sfx).c_str())) {
            m.on = true;
            m.type = 9;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d", &m.lofiBit, &m.lofiRate, &m.lofiFilter, &m.lofiFilterKf, &m.lofiFilterLfo,
                        &m.lofiFilterEnv, &m.lofiBalance, &m.lofiBalanceLfo, &m.lofiBalanceEnv);
            set("Switch", 1); set("Type", 9); set("LofiBit", m.lofiBit); set("LofiRate", m.lofiRate); set("LofiFilter", m.lofiFilter);
            set("LofiFilterKF", m.lofiFilterKf); set("LofiFilterLfo", m.lofiFilterLfo); set("LofiFilterEnv", m.lofiFilterEnv);
            set("LofiBalance", m.lofiBalance); set("LofiBalanceLfo", m.lofiBalanceLfo); set("LofiBalanceEnv", m.lofiBalanceEnv);
        }
        if (const char* e = std::getenv(("TEST_FS" + sfx).c_str())) {
            m.on = true;
            m.type = 8;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &m.fsDepth, &m.fsDepthKf, &m.fsDepthLfo, &m.fsDepthEnv, &m.fsBalance, &m.fsBalanceLfo,
                        &m.fsBalanceEnv);
            set("Switch", 1); set("Type", 8); set("FsDepth", m.fsDepth); set("FsDepthKF", m.fsDepthKf); set("FsDepthLfo", m.fsDepthLfo);
            set("FsDepthEnv", m.fsDepthEnv); set("FsBalance", m.fsBalance); set("FsBalanceLfo", m.fsBalanceLfo); set("FsBalanceEnv", m.fsBalanceEnv);
        }
        if (const char* e = std::getenv(("TEST_RES" + sfx).c_str())) {
            m.on = true;
            m.type = 7;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d", &m.resType, &m.resSize, &m.resSizeKf, &m.resSizeLfo, &m.resSizeEnv, &m.resBalance,
                        &m.resBalanceLfo, &m.resBalanceEnv);
            set("Switch", 1); set("Type", 7); set("ResType", m.resType); set("ResSize", m.resSize); set("ResSizeKF", m.resSizeKf);
            set("ResSizeLfo", m.resSizeLfo); set("ResSizeEnv", m.resSizeEnv); set("ResBalance", m.resBalance);
            set("ResBalanceLfo", m.resBalanceLfo); set("ResBalanceEnv", m.resBalanceEnv);
        }
        for (int t = 0; t < 2; ++t)
            if (const char* e = std::getenv(("TEST_SBF" + std::to_string(t + 1) + sfx).c_str())) {
                auto& b = m.sb[t];
                m.on = true;
                m.type = 5 + t;
                if (t == 0)
                    std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &b.hpf, &b.lpf, &b.lpfKf, &b.subDetune, &b.octave, &b.detune,
                                &b.detuneLfo, &b.detuneOwn, &b.detuneEnv, &b.width, &b.widthKf, &b.widthLfo, &b.widthOwn, &b.widthEnv);
                else
                    std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &b.q, &b.lpf, &b.lpfKf, &b.octave, &b.detune, &b.detuneLfo,
                                &b.detuneOwn, &b.detuneEnv, &b.width, &b.widthKf, &b.widthLfo, &b.widthOwn, &b.widthEnv);
                const std::string id = "Sb" + std::to_string(t + 1);
                auto sbSet = [&](const char* name, int v) { set((id + name).c_str(), v); };
                set("Switch", 1); set("Type", m.type);
                if (t == 0) { sbSet("Hpf", b.hpf); sbSet("SubDetune", b.subDetune); }
                else sbSet("Q", b.q);
                sbSet("Lpf", b.lpf); sbSet("LpfKF", b.lpfKf); sbSet("Octave", b.octave); sbSet("Detune", b.detune);
                sbSet("DetuneLfo", b.detuneLfo); sbSet("DetuneOwn", b.detuneOwn); sbSet("DetuneEnv", b.detuneEnv);
                sbSet("Width", b.width); sbSet("WidthKF", b.widthKf); sbSet("WidthLfo", b.widthLfo); sbSet("WidthOwn", b.widthOwn);
                sbSet("WidthEnv", b.widthEnv);
            }
        if (const char* e = std::getenv(("TEST_CENVB" + sfx).c_str())) {
            auto& b = m.envB;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d", &b.attack, &b.decay, &b.sustain, &b.release, &b.velCurve, &b.velSens);
            set("EnvBAttack", b.attack); set("EnvBDecay", b.decay); set("EnvBSustain", b.sustain); set("EnvBRelease", b.release);
            set("EnvBVelCurve", b.velCurve); set("EnvBVelSens", b.velSens);
        }
        if (const char* e = std::getenv(("TEST_CENV" + sfx).c_str())) {
            auto& a = m.envA;
            std::sscanf(e, "%d,%d,%d,%d,%d,%d", &a.attack, &a.decay, &a.sustain, &a.release, &a.velCurve, &a.velSens);
            set("EnvAAttack", a.attack); set("EnvADecay", a.decay); set("EnvASustain", a.sustain); set("EnvARelease", a.release);
            set("EnvAVelCurve", a.velCurve); set("EnvAVelSens", a.velSens);
        }
        if (const char* e = std::getenv(("TEST_CLFO" + sfx).c_str())) {
            std::sscanf(e, "%d,%d", &m.lfoWave, &m.lfoRate);
            set("LfoWave", m.lfoWave); set("LfoRate", m.lfoRate);
        }
    }
    // TEST_TVA_ON=1: the TVA Switch on
    if (const char* e = std::getenv("TEST_TVA_ON")) {
        refCtl.tvaOn = std::atoi(e) != 0;
        setKnobs({ { "tvaSwitch", refCtl.tvaOn ? 1 : 0 } });
    }
    // TEST_TVA="level,levelKF,levelLfo,pan,panKF,panLfo,send": the patch TVA (KFs in %)
    if (const char* e = std::getenv("TEST_TVA")) {
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &refCtl.tvaLevel, &refCtl.tvaLevelKF, &refCtl.tvaLevelLfo, &refCtl.tvaPan, &refCtl.tvaPanKF, &refCtl.tvaPanLfo, &refCtl.mfxSend);
        setKnobs({ { "tvaLevel", refCtl.tvaLevel }, { "tvaLevelKF", refCtl.tvaLevelKF }, { "tvaLevelLfo", refCtl.tvaLevelLfo }, { "tvaPan", refCtl.tvaPan },
                   { "tvaPanKF", refCtl.tvaPanKF }, { "tvaPanLfo", refCtl.tvaPanLfo }, { "mfxSend", refCtl.mfxSend } });
    }
    // TEST_BUS="chorus,reverb,assign,parallel": the patch TVA's other sends, Output Assign, Output Mode
    if (const char* e = std::getenv("TEST_BUS")) {
        int parallel = 1;
        std::sscanf(e, "%d,%d,%d,%d", &refCtl.chorusSend, &refCtl.reverbSend, &refCtl.outputAssign, &parallel);
        refCtl.parallel = parallel != 0;
        setKnobs({ { "tvaChoSend", refCtl.chorusSend }, { "tvaRevSend", refCtl.reverbSend }, { "outputAssign", refCtl.outputAssign }, { "outputMode", parallel } });
    }
    // TEST_MATRIX="source,dest1,sens1,dest2,sens2": Matrix Control 1 (with TEST_CC to move its source)
    if (const char* e = std::getenv("TEST_MATRIX")) {
        XtMatrixControl& x = refCtl.matrix[0];
        std::sscanf(e, "%d,%d,%d,%d,%d", &x.source, &x.dest[0], &x.sens[0], &x.dest[1], &x.sens[1]);
        setKnobs({ { "matrix1Source", x.source }, { "matrix1Dest1", x.dest[0] }, { "matrix1Sens1", x.sens[0] }, { "matrix1Dest2", x.dest[1] }, { "matrix1Sens2", x.sens[1] } });
    }
    // TEST_ASSIGN="padX": PAD-X's controller, a CC number
    if (const char* e = std::getenv("TEST_ASSIGN")) {
        refCtl.assign[6] = std::atoi(e);
        setKnobs({ { "assignPadX", refCtl.assign[6] } });
    }
    // TEST_TRIPCC="x,y": Time Trip Assign X and Y, a CC number or 0
    if (const char* e = std::getenv("TEST_TRIPCC")) {
        std::sscanf(e, "%d,%d", &refCtl.assign[10], &refCtl.assign[11]);
        setKnobs({ { "assignTripX", refCtl.assign[10] }, { "assignTripY", refCtl.assign[11] } });
    }
    // TEST_LFO5="wave,rate,offset,delay,mode,fade,keysync": LFO 5
    if (const char* e = std::getenv("TEST_LFO5")) {
        int ks = 0;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d", &refCtl.tvaLfoWave, &refCtl.tvaLfoRate, &refCtl.tvaLfoOffset, &refCtl.tvaLfoDelay, &refCtl.tvaLfoFadeMode, &refCtl.tvaLfoFade, &ks);
        refCtl.tvaLfoKeySync = ks != 0;
        setKnobs({ { "tvaLfoWave", refCtl.tvaLfoWave }, { "tvaLfoRate", refCtl.tvaLfoRate }, { "tvaLfoOffset", refCtl.tvaLfoOffset }, { "tvaLfoDelay", refCtl.tvaLfoDelay },
                   { "tvaLfoFadeMode", refCtl.tvaLfoFadeMode }, { "tvaLfoFade", refCtl.tvaLfoFade }, { "tvaLfoKeySync", ks } });
    }
    // TEST_TEMPO=n: the Patch Tempo, BPM (whole); TEST_SYNC="sync,origBpm": Tempo Sync and the Original Tempo;
    // TEST_RATE=n: the LFO Rate, 128..144 a note value; TEST_ATTACK=n: the OSC amp envelope's Attack, likewise
    if (const char* e = std::getenv("TEST_TEMPO")) {
        const int bpm = std::atoi(e);
        refCtl.patchTempo = bpm * 10;
        setKnobs({ { "patchTempo", bpm } });
    }
    if (const char* e = std::getenv("TEST_SYNC")) {
        int sw = 0, bpm = 120;
        std::sscanf(e, "%d,%d", &sw, &bpm);
        refCtl.tempoSync = sw != 0;
        refCtl.origTempo = bpm * 10000;
        setKnobs({ { "tempoSync", sw }, { "origTempo", bpm } });
    }
    if (const char* e = std::getenv("TEST_RATE")) {
        refCtl.lfoRate = std::atoi(e);
        setKnobs({ { "lfoRate", refCtl.lfoRate } });
    }
    if (const char* e = std::getenv("TEST_ATTACK")) {
        refCtl.oAttack = std::atoi(e);
        setKnobs({ { "oscEnvAttack", refCtl.oAttack } });
    }
    // TEST_HOST="bpm,ppq,moving": Clock Source HOST on a host's transport at that tempo, from that position, in quarter notes
    double hostBpm = 0.0, hostPpq = 0.0;
    int hostMoving = 1;
    if (const char* e = std::getenv("TEST_HOST")) {
        std::sscanf(e, "%lf,%lf,%d", &hostBpm, &hostPpq, &hostMoving);
        refCtl.clockSource = 1;
        setKnobs({ { "clockSource", 1 } });
    }
    // TEST_MOD="type,level,attack,release": the MOD on, Type 0 MIX, 1 RING, 2 FM, 3 ENV RING, 4 OSC SYNC
    if (const char* e = std::getenv("TEST_MOD")) {
        refCtl.mod.on = true;
        std::sscanf(e, "%d,%d,%d,%d", &refCtl.mod.type, &refCtl.mod.level, &refCtl.mod.attack, &refCtl.mod.release);
        setKnobs({ { "modSwitch", 1 }, { "modType", refCtl.mod.type }, { "modLevel", refCtl.mod.level }, { "modAttack", refCtl.mod.attack },
                   { "modRelease", refCtl.mod.release } });
    }
    // TEST_STRUCT=n: the Structure Type, 0…2
    if (const char* e = std::getenv("TEST_STRUCT")) {
        refCtl.structure = std::atoi(e);
        setKnobs({ { "structure", refCtl.structure } });
    }
    // TEST_MONO="mono,legato[,octave]": Mono/Poly (1 MONO), Legato Switch,
    // Octave Shift
    if (const char* e = std::getenv("TEST_MONO")) {
        int m = 0, l = 1;
        std::sscanf(e, "%d,%d,%d", &m, &l, &refCtl.octave);
        refCtl.mono = m != 0;
        refCtl.legato = l != 0;
        setKnobs({ { "monoPoly", refCtl.mono ? 0 : 1 }, { "legatoSwitch", l != 0 ? 1 : 0 }, { "octaveShift", refCtl.octave } });
    }
    // TEST_PITCH="shift,pitch,pitchKF,coarse,fine[,origKey]": Pitch Shift
    // (semitones), the OSC's Pitch knob, Pitch KF (%), Coarse, Fine (cents)
    // and the Original Key
    if (const char* e = std::getenv("TEST_PITCH")) {
        std::sscanf(e, "%f,%d,%d,%d,%d,%d", &refCtl.pitchSemis, &refCtl.oscPitch, &refCtl.pitchKF, &refCtl.oscCoarse, &refCtl.oscFine,
                    &refCtl.rootKey);
        auto* prm = p.apvts.getParameter("pitchShift");
        prm->setValueNotifyingHost(prm->convertTo0to1(refCtl.pitchSemis));
        setKnobs({ { "oscPitch", refCtl.oscPitch }, { "pitchKF", refCtl.pitchKF }, { "oscCoarse", refCtl.oscCoarse },
                   { "oscFine", refCtl.oscFine }, { "rootKey", refCtl.rootKey } });
    }
    // TEST_TUNE="patchCoarse,patchFine,random[,bendUp,bendDown]": the
    // patch's Coarse/Fine Tune, Random Pitch Depth (0..30) and Bend Range;
    // TEST_SCALE="c,c#,…,b": Scale Tune on with the 12 values (cents)
    if (const char* e = std::getenv("TEST_TUNE")) {
        std::sscanf(e, "%d,%d,%d,%d,%d", &refCtl.patchCoarse, &refCtl.patchFine, &refCtl.randomPitch, &refCtl.bendUp, &refCtl.bendDown);
        setKnobs({ { "patchCoarse", refCtl.patchCoarse }, { "patchFine", refCtl.patchFine }, { "randomPitch", refCtl.randomPitch },
                   { "bendRangeUp", refCtl.bendUp }, { "bendRangeDown", refCtl.bendDown } });
    }
    if (const char* e = std::getenv("TEST_SCALE")) {
        refCtl.scaleTune = true;
        int* t = refCtl.scale;
        std::sscanf(e, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", t, t + 1, t + 2, t + 3, t + 4, t + 5, t + 6, t + 7, t + 8, t + 9, t + 10, t + 11);
        static const char* const ids[12] = { "scaleTuneC", "scaleTuneCs", "scaleTuneD", "scaleTuneDs", "scaleTuneE", "scaleTuneF",
                                             "scaleTuneFs", "scaleTuneG", "scaleTuneGs", "scaleTuneA", "scaleTuneAs", "scaleTuneB" };
        setKnobs({ { "scaleTune", 1 } });
        for (int i = 0; i < 12; ++i) setKnobs({ { ids[i], t[i] } });
    }
    // TEST_VELOCITY=v: the note-on velocity (1..127, default 127)
    const int noteVel = std::getenv("TEST_VELOCITY") ? std::clamp(std::atoi(std::getenv("TEST_VELOCITY")), 1, 127) : 127;
    // OSC 2's wave first: the encodes run in turn, so OSC 1's status comes last
    std::shared_ptr<XtEncoded> E2;
    if (const char* e = std::getenv("TEST_OSC2_WAVE")) {
        const juce::String spec(e), path2 = spec.upToFirstOccurrenceOf(",", false, false), how = spec.fromFirstOccurrenceOf(",", false, false);
        const int idx2 = how == "backing" ? 2 : how == "ensemble" ? 3 : 1, type2 = how == "backing" ? 3 : how == "ensemble" ? 5 : 1;
        for (auto [id, v] : { std::pair<const char*, int>{ "oscSwitch", 1 }, { "oscType", 1 }, { "encodeType", idx2 } }) {
            auto* prm = p.apvts.getParameter(VSynthEmuProcessor::oscParamId(1, id));
            prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
        }
        refCtl.osc2.on = true;
        refCtl.osc2.oscType = 1;
        p.loadFile(juce::File(path2), 1);
        juce::AudioFormatManager fm2; fm2.registerBasicFormats(); fm2.registerFormat(new XtVpwFormat(), false);
        std::unique_ptr<juce::AudioFormatReader> rd2(fm2.createReaderFor(juce::File(path2)));
        if (!rd2) { std::fprintf(stderr, "TEST_OSC2_WAVE: cannot read %s\n", path2.toRawUTF8()); return 1; }
        juce::AudioBuffer<float> in2((int)rd2->numChannels, (int)rd2->lengthInSamples);
        rd2->read(&in2, 0, in2.getNumSamples(), 0, true, true);
        std::vector<std::vector<float>> ch2;
        for (int c = 0; c < std::min(2, in2.getNumChannels()); ++c) ch2.emplace_back(in2.getReadPointer(c), in2.getReadPointer(c) + in2.getNumSamples());
        XtWaveFile own2;
        juce::MemoryBlock b2;
        const bool isOwn2 = juce::File(path2).loadFileAsData(b2) && xtWaveRead(static_cast<const uint8_t*>(b2.getData()), b2.getSize(), own2) && own2.encodeType == type2;
        E2 = xtSynthEncode(ch2, rd2->sampleRate, type2, 64, nullptr, rd2->usesFloatingPointData ? 16 : (int)rd2->bitsPerSample, isOwn2 ? &own2.events : nullptr, isOwn2 ? &own2.vpdt : nullptr);
        if (!E2) { std::fprintf(stderr, "TEST_OSC2_WAVE: encode failed\n"); return 1; }
    }
    p.loadFile(juce::File(argv[2]));
    for (int i = 0; i < 600 && !p.getStatusText().startsWith("Encoded"); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));   // the encode runs on the pool thread
        if (p.getStatusText().startsWith("Encode failed")) break;
    }
    std::printf("status: %s\n", p.getStatusText().toRawUTF8());
    if (juce::File(argv[2]).hasFileExtension("vpw") && !p.getFormatManager().getWildcardForAllFormats().contains("*.vpw")) { std::fprintf(stderr, ".vpw isn't in the file chooser's list\n"); return 1; }
    for (const char* id : { "pitchShift", "patchCoarse", "patchFine", "randomPitch", "oscCoarse", "oscFine", "oscPitch", "pitchKF", "oscTime", "oscTimeOffset", "oscFormant", "energy",
                            "rootKey", "ampAttack", "ampDecay", "ampSustain", "ampRelease", "encodeDepth" })
        std::printf("  %s = %.9g\n", id, (double)p.apvts.getRawParameterValue(id)->load());
    if (!p.getStatusText().startsWith("Encoded")) return 1;

    // the same input straight through XtSynth
    juce::AudioFormatManager fm; fm.registerBasicFormats(); fm.registerFormat(new XtVpwFormat(), false);
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(juce::File(argv[2])));
    juce::AudioBuffer<float> in((int)rd->numChannels, (int)rd->lengthInSamples);
    rd->read(&in, 0, in.getNumSamples(), 0, true, true);
    std::vector<std::vector<float>> chans;
    for (int c = 0; c < std::min(2, in.getNumChannels()); ++c) chans.emplace_back(in.getReadPointer(c), in.getReadPointer(c) + in.getNumSamples());
    const juce::String st = p.getStatusText();
    // TEST_EXPECT_ENCODE=LITE (or SOLO, BACKING, ENSEMBLE): the run fails unless the wave was encoded as that
    if (const char* want = std::getenv("TEST_EXPECT_ENCODE"); want && !st.contains(juce::String("Encoded as ") + want)) {
        std::fprintf(stderr, "expected the wave encoded as %s; the status says: %s\n", want, st.toRawUTF8());
        return 1;
    }
    const int type = st.contains("ENSEMBLE") ? 5 : st.contains("BACKING") ? 3 : st.contains("LITE") ? 0 : 1;
    const int bits = rd->usesFloatingPointData ? 16 : (int)rd->bitsPerSample;
    // A V-Synth wave (.vpw, or a project's .wav) taken as its own type plays its own vpdt, or keeps its own events, as the processor does
    XtWaveFile ownFile;
    bool isOwnFile = false;
    {
        juce::MemoryBlock bytes;
        isOwnFile = juce::File(argv[2]).loadFileAsData(bytes) && xtWaveRead(static_cast<const uint8_t*>(bytes.getData()), bytes.getSize(), ownFile);
    }
    const std::vector<int32_t>* ownEvents = isOwnFile && ownFile.encodeType == type ? &ownFile.events : nullptr;
    const std::vector<uint8_t>* ownVpdt = isOwnFile && ownFile.encodeType == type ? &ownFile.vpdt : nullptr;
    auto E = xtSynthEncode(chans, rd->sampleRate, type, 64, nullptr, bits, ownEvents, ownVpdt);
    {   // the encode must be repeatable
        auto E2 = xtSynthEncode(chans, rd->sampleRate, type, 64, nullptr, bits, ownEvents, ownVpdt);
        bool same = E->vpdt == E2->vpdt && E->solo.segStart == E2->solo.segStart && E->solo.in == E2->solo.in
                 && E->solo.inR == E2->solo.inR;
        for (int k = 0; k < 6; ++k) same = same && E->bands.buf[k] == E2->bands.buf[k];
        std::printf("encode repeatable: %s\n", same ? "yes" : "NO");
    }
    if (ownVpdt) std::printf("the wave's own vpdt: %s\n", E->stored ? "played as it is" : "encoded again");
    // A V-Synth wave, a .vpw or a project's .wav, brings its Original Key, Tempo, loop, fine tuning and own list of events with it, as the
    // processor takes them (loadFile, reencode)
    if (isOwnFile) {
        xtWaveOntoControls(ownFile, refCtl);
        xtWaveOntoEncoded(ownFile, *E);
    }
    XtSynth ref; ref.prepare(rate); ref.setSample(E); ref.setControls(refCtl);
    if (E2) ref.setSample(E2, 1);
    // TEST_FX=1: the Effects switch on, so all this goes through the chip: the processor's, and one here on the reference's bus, each started the same way.
    // TEST_FX_KNOBS="fxRevOn=1,fxRevType=4": its parameters; TEST_FX_EDIT="0x80e=20,...": those by the firmware's ID.  TEST_FX_LIVE="frame:name=value,...": then, at
    // that frame's block, a parameter by name or ID, ticking both sides each block as the control thread does.
    std::unique_ptr<XtFxUnit> refFx;
    if (std::getenv("TEST_FX")) {
        p.fxManualTicks();
        p.apvts.getParameter("fxOn")->setValueNotifyingHost(1.0f);
        refFx = std::make_unique<XtFxUnit>(argv[1]);
        // Each of the plugin's effect parameters has the name, range and default the firmware gives its ID.
        Sh4 romImage; romImage.loadRom(argv[1]);
        for (auto* prmBase : p.getParameters())
            if (auto* prm = dynamic_cast<juce::RangedAudioParameter*>(prmBase); prm && VSynthEmuProcessor::fxKnobId(prm->paramID) >= 0) {
                const int id = VSynthEmuProcessor::fxKnobId(prm->paramID), def = (int)std::lround(prm->convertFrom0to1(prm->getDefaultValue()));
                juce::String firmwareName;
                for (uint32_t a = romImage.rd32(0x89790338 + 4 * (uint32_t)id); romImage.rd8(a); ++a) firmwareName += (char)romImage.rd8(a);
                firmwareName = firmwareName.upToFirstOccurrenceOf(" *", false, false).trim();
                if (prm->name != firmwareName) { std::fprintf(stderr, "%s is ID %x, which is %s\n", prm->name.toRawUTF8(), id, firmwareName.toRawUTF8()); return 1; }
                if (prm->convertFrom0to1(0.0f) != (float)refFx->control().lowest(id) || prm->convertFrom0to1(1.0f) != (float)refFx->control().highest(id) || def != refFx->control().get(id)) {
                    std::fprintf(stderr, "%s isn't as ID %x's descriptor: %g..%g, %d against %d..%d, %d\n", prm->paramID.toRawUTF8(), id, prm->convertFrom0to1(0.0f), prm->convertFrom0to1(1.0f), def, refFx->control().lowest(id), refFx->control().highest(id), refFx->control().get(id));
                    return 1;
                }
            }
        if (const char* e = std::getenv("TEST_FX_KNOBS"))
            for (const auto& pair : juce::StringArray::fromTokens(e, ",", "")) {
                const juce::String name = pair.upToFirstOccurrenceOf("=", false, false); const int v = pair.fromFirstOccurrenceOf("=", false, false).getIntValue();
                auto* prm = p.apvts.getParameter(name);
                if (!prm || VSynthEmuProcessor::fxKnobId(name) < 0) { std::fprintf(stderr, "no such effect parameter: %s\n", name.toRawUTF8()); return 1; }
                prm->setValueNotifyingHost(prm->convertTo0to1((float)v));
                refFx->control().set(VSynthEmuProcessor::fxKnobId(name), v);
            }
        if (const char* e = std::getenv("TEST_FX_EDIT"))
            for (const auto& pair : juce::StringArray::fromTokens(e, ",", "")) {
                const int id = (int)std::strtol(pair.upToFirstOccurrenceOf("=", false, false).toRawUTF8(), nullptr, 0), v = pair.fromFirstOccurrenceOf("=", false, false).getIntValue();
                p.setFxParam(id, v);
                refFx->control().set(id, v);
            }
        p.fxTick();
        if (!p.effectsRunning()) { std::fprintf(stderr, "the effects didn't start\n"); return 1; }
        refFx->control().set(0x11d, ref.fxPatchTempo());
        refFx->control().loadPatch();
        refFx->settle();
        {   // What a host's saved state brings back: the edits by ID, in a new processor.
            juce::MemoryBlock saved; p.getStateInformation(saved);
            VSynthEmuProcessor back; back.fxManualTicks(); back.setStateInformation(saved.getData(), (int)saved.getSize());
            if (const char* e = std::getenv("TEST_FX_EDIT"))
                for (const auto& pair : juce::StringArray::fromTokens(e, ",", ""))
                    if (back.fxParam((int)std::strtol(pair.upToFirstOccurrenceOf("=", false, false).toRawUTF8(), nullptr, 0), INT_MIN) != pair.fromFirstOccurrenceOf("=", false, false).getIntValue()) { std::fprintf(stderr, "a saved state lost %s\n", pair.toRawUTF8()); return 1; }
        }
        ref.setBusSink(refFx.get()); ref.useBusSink(true);
    }

    const int total = (int)(in.getNumSamples() * rate / rd->sampleRate);
    double maxDiff = 0, energy = 0, side = 0;
    long firstDiff = -1;
    juce::AudioBuffer<float> buf(2, block);
    std::vector<float> rl(block), rr(block);
    const int offBlock = total / 2 / block * block, offAt = 37;    // note-off halfway, inside a block
    int note2 = -1, at2 = 0;                                         // TEST_NOTE2="note,frame": a second note-on
    if (const char* e = std::getenv("TEST_NOTE2")) std::sscanf(e, "%d,%d", &note2, &at2);
    const int n2Block = note2 >= 0 ? at2 / block * block : -1, n2At = at2 % block;
    if (n2Block == 0 || n2Block == offBlock) { std::fprintf(stderr, "TEST_NOTE2 must fall in another block\n"); return 1; }
    int bendValue = -1, bendFrame = 0;                               // TEST_BEND="value,frame": the pitch wheel
    if (const char* e = std::getenv("TEST_BEND")) std::sscanf(e, "%d,%d", &bendValue, &bendFrame);
    const int bBlock = bendValue >= 0 ? bendFrame / block * block : -1, bAt = bendFrame % block;
    if (bendValue >= 0 && (bBlock == 0 || bBlock == offBlock || bBlock == n2Block)) { std::fprintf(stderr, "TEST_BEND must fall in another block\n"); return 1; }
    int ccNum = -1, ccValue = 0, ccFrame = 0;                        // TEST_CC="cc,value,frame": a controller
    if (const char* e = std::getenv("TEST_CC")) std::sscanf(e, "%d,%d,%d", &ccNum, &ccValue, &ccFrame);
    const int ccBlock = ccNum >= 0 ? ccFrame / block * block : -1, ccAt = ccFrame % block;
    if (ccNum >= 0 && (ccBlock == 0 || ccBlock == offBlock || ccBlock == n2Block || ccBlock == bBlock)) { std::fprintf(stderr, "TEST_CC must fall in another block\n"); return 1; }
    // TEST_TRIP="value,frame[,value,frame...]": the Time Trip knob at the start of the blocks holding these frames
    std::vector<std::pair<int, int>> trips;
    if (const char* e = std::getenv("TEST_TRIP")) {
        const juce::StringArray w = juce::StringArray::fromTokens(e, ",", "");
        for (int i = 0; i + 1 < w.size(); i += 2) trips.push_back({ w[i].getIntValue(), w[i + 1].getIntValue() / block * block });
    }
    struct Head : juce::AudioPlayHead {
        double bpm = 0.0, ppq = 0.0;
        bool moving = false;
        juce::Optional<PositionInfo> getPosition() const override {
            PositionInfo i;
            i.setBpm(bpm);
            i.setPpqPosition(ppq);
            i.setIsPlaying(moving);
            return i;
        }
    } head;
    if (hostBpm > 0.0) p.setPlayHead(&head);
    // TEST_CCS="cc,value,frame[,cc,value,frame...]": controllers at the start of the blocks holding these frames
    struct CcAt { int number, value, frame; };
    std::vector<CcAt> ccs;
    if (const char* e = std::getenv("TEST_CCS")) {
        const juce::StringArray w = juce::StringArray::fromTokens(e, ",", "");
        for (int i = 0; i + 2 < w.size(); i += 3) ccs.push_back({ w[i].getIntValue(), w[i + 1].getIntValue(), w[i + 2].getIntValue() / block * block });
    }
    struct FxAt { int frame; juce::String name; int value; };
    std::vector<FxAt> fxLive;
    if (const char* e = std::getenv("TEST_FX_LIVE"))
        for (const auto& t : juce::StringArray::fromTokens(e, ",", "")) {
            const juce::String kv = t.fromFirstOccurrenceOf(":", false, false);
            fxLive.push_back({ t.upToFirstOccurrenceOf(":", false, false).getIntValue() / block * block, kv.upToFirstOccurrenceOf("=", false, false), kv.fromFirstOccurrenceOf("=", false, false).getIntValue() });
        }
    double tail = 0.0, tailMean = 0.0;
    for (int done = 0; done < total; done += block) {
        std::map<int, int> fxNow;    // the last of a block's, as the processor sees them at its tick
        for (const FxAt& t : fxLive)
            if (t.frame == done) {
                if (VSynthEmuProcessor::fxKnobId(t.name) >= 0) {
                    auto* prm = p.apvts.getParameter(t.name);
                    prm->setValueNotifyingHost(prm->convertTo0to1((float)t.value));
                    fxNow[VSynthEmuProcessor::fxKnobId(t.name)] = t.value;
                } else {
                    const int id = (int)std::strtol(t.name.toRawUTF8(), nullptr, 0);
                    p.setFxParam(id, t.value);
                    fxNow[id] = t.value;
                }
            }
        if (refFx) {
            p.fxTick();
            XtFxControl& c = refFx->control();
            for (const auto& [id, value] : fxNow) c.set(id, value);
            c.set(0x11d, ref.fxPatchTempo());
            c.set(0x41, ref.fxClockSix() ? 1 : 0);
            c.tick();
            c.setMatrix(ref.fxMatrix(0), ref.fxMatrix(1), ref.fxMatrix(2));
            c.outsideClock(ref.fxClockSix());
            c.pump();
        }
        head.bpm = hostBpm;
        head.ppq = hostPpq + (double)done / rate * hostBpm / 60.0;
        head.moving = hostMoving != 0;
        for (const auto& t : trips)
            if (t.second == done) {
                setKnobs({ { "timeTrip", t.first } });
                ref.timeTripKnob(t.first);
            }
        juce::MidiBuffer midi;
        for (const CcAt& t : ccs)
            if (t.frame == done) midi.addEvent(juce::MidiMessage::controllerEvent(1, t.number, t.value), 0);
        if (done == 0) { midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)noteVel), 100); }
        if (done == offBlock) { midi.addEvent(juce::MidiMessage::noteOff(1, note, (juce::uint8)100), offAt); }
        if (done == n2Block) { midi.addEvent(juce::MidiMessage::noteOn(1, note2, (juce::uint8)noteVel), n2At); }
        if (done == bBlock) { midi.addEvent(juce::MidiMessage::pitchWheel(1, bendValue), bAt); }
        if (done == ccBlock) { midi.addEvent(juce::MidiMessage::controllerEvent(1, ccNum, ccValue), ccAt); }
        p.processBlock(buf, midi);
        juce::ScopedNoDenormals ftz;   // processBlock runs with denormals flushed (as the SH-4 does, FPSCR.DN = 1)
        if (std::getenv("TEST_CONTROLS")) ref.setControls(refCtl);   // as processBlock does each block
        if (hostBpm > 0.0) ref.hostClock(head.bpm, head.moving, head.ppq);
        for (const CcAt& t : ccs)
            if (t.frame == done) ref.controller(t.number, t.value);
        if (done == 0) { ref.render(rl.data(), rr.data(), 100); ref.noteOn(note, (float)noteVel * (1.0f / 127.0f)); ref.render(rl.data() + 100, rr.data() + 100, block - 100); }
        else if (done == offBlock) { ref.render(rl.data(), rr.data(), offAt); ref.noteOff(note, 100 / 127.0f); ref.render(rl.data() + offAt, rr.data() + offAt, block - offAt); }
        else if (done == n2Block) { ref.render(rl.data(), rr.data(), n2At); ref.noteOn(note2, (float)noteVel * (1.0f / 127.0f)); ref.render(rl.data() + n2At, rr.data() + n2At, block - n2At); }
        else if (done == bBlock) { ref.render(rl.data(), rr.data(), bAt); ref.pitchBend(bendValue); ref.render(rl.data() + bAt, rr.data() + bAt, block - bAt); }
        else if (done == ccBlock) { ref.render(rl.data(), rr.data(), ccAt); ref.controller(ccNum, ccValue); ref.render(rl.data() + ccAt, rr.data() + ccAt, block - ccAt); }
        else ref.render(rl.data(), rr.data(), block);
        for (int i = 0; i < block; ++i) {
            if (firstDiff < 0 && (buf.getSample(0, i) != rl[(size_t)i] || buf.getSample(1, i) != rr[(size_t)i])) firstDiff = done + i;
            maxDiff = std::max(maxDiff, (double)std::abs(buf.getSample(0, i) - rl[(size_t)i]));
            maxDiff = std::max(maxDiff, (double)std::abs(buf.getSample(1, i) - rr[(size_t)i]));
            energy += (double)buf.getSample(0, i) * buf.getSample(0, i);
            side += (double)(buf.getSample(0, i) - buf.getSample(1, i)) * (buf.getSample(0, i) - buf.getSample(1, i));
            if (done >= total - total / 8) { tail += (double)buf.getSample(0, i) * buf.getSample(0, i); tailMean += buf.getSample(0, i); }
        }
    }
    if (refFx) std::printf("the last eighth: rms %.5f, mean %.5f; MFX-PRM %d %d %d\n", std::sqrt(tail / (total / 8)), tailMean / (total / 8), ref.fxMatrix(0), ref.fxMatrix(1), ref.fxMatrix(2));
    std::printf("rendered %d frames at %.0f Hz: rms %.4f (L-R %.4f), max |plugin - XtSynth| = %g (first at frame %ld)\n",
                total, rate, std::sqrt(energy / total), std::sqrt(side / total), maxDiff, firstDiff);
    return maxDiff == 0 && energy > 0 ? 0 : 2;
}
