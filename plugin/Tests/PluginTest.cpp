// End-to-end check of the plugin processor's XT path: ROM, file load,
// background encode, sample-accurate MIDI, render — against XtSynth driven
// directly on the same input.  Build: cmake --build <dir> --target VSynthEmuTest
// Run:   VSynthEmuTest <rom.2.bin> <file.wav> [note]
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <thread>

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s rom.2.bin file.wav [note] [auto|solo|backing|ensemble]\n", argv[0]); return 1; }
    juce::ScopedJuceInitialiser_GUI init;
    const int note = argc > 3 ? std::atoi(argv[3]) : 60;
    const double rate = 48000.0;
    const int block = 512;

    VSynthEmuProcessor p;
    if (!p.setRomFile(juce::File(argv[1]))) { std::fprintf(stderr, "ROM rejected: %s\n", p.getStatusText().toRawUTF8()); return 1; }
    p.prepareToPlay(rate, block);
    // TEST_SNAPSHOT=file.png: the editor drawn offscreen, for checking the layout
    if (const char* snap = std::getenv("TEST_SNAPSHOT")) {
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        const juce::Image img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 2.0f);
        juce::File f(juce::File::getCurrentWorkingDirectory().getChildFile(snap));
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat png;
        if (!os.openedOk() || !png.writeImageToStream(img, os)) { std::fprintf(stderr, "snapshot failed\n"); return 1; }
        std::printf("snapshot: %s (%dx%d)\n", f.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    }
    if (argc > 4) {   // Encode Type (choice index 0 Auto, 1 SOLO, 2 BACKING, 3 ENSEMBLE)
        const juce::String t(argv[4]);
        const int idx = t == "solo" ? 1 : t == "backing" ? 2 : t == "ensemble" ? 3 : 0;
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
        refCtl.loopStart = std::round(p.apvts.getRawParameterValue("loopStart")->load() / 0.01f) * 0.01f / 100.0f;
        refCtl.loopEnd = std::round(p.apvts.getRawParameterValue("loopEnd")->load() / 0.01f) * 0.01f / 100.0f;
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
    // TEST_VELOCITY=v: the note-on velocity (1..127, default 127)
    const int noteVel = std::getenv("TEST_VELOCITY") ? std::clamp(std::atoi(std::getenv("TEST_VELOCITY")), 1, 127) : 127;
    p.loadFile(juce::File(argv[2]));
    for (int i = 0; i < 600 && !p.getStatusText().startsWith("Encoded"); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));   // the encode runs on the pool thread
        if (p.getStatusText().startsWith("Encode failed")) break;
    }
    std::printf("status: %s\n", p.getStatusText().toRawUTF8());
    for (const char* id : { "pitchShift", "oscCoarse", "oscFine", "oscPitch", "pitchKF", "oscTime", "oscTimeOffset", "oscFormant", "energy",
                            "rootKey", "ampAttack", "ampDecay", "ampSustain", "ampRelease", "encodeDepth" })
        std::printf("  %s = %.9g\n", id, (double)p.apvts.getRawParameterValue(id)->load());
    if (!p.getStatusText().startsWith("Encoded")) return 1;

    // the same input straight through XtSynth
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd(fm.createReaderFor(juce::File(argv[2])));
    juce::AudioBuffer<float> in((int)rd->numChannels, (int)rd->lengthInSamples);
    rd->read(&in, 0, in.getNumSamples(), 0, true, true);
    std::vector<std::vector<float>> chans;
    for (int c = 0; c < std::min(2, in.getNumChannels()); ++c) chans.emplace_back(in.getReadPointer(c), in.getReadPointer(c) + in.getNumSamples());
    const juce::String st = p.getStatusText();
    const int type = st.contains("ENSEMBLE") ? 5 : st.contains("BACKING") ? 3 : 1;
    const int bits = rd->usesFloatingPointData ? 16 : (int)rd->bitsPerSample;
    auto E = xtSynthEncode(chans, rd->sampleRate, type, 64, nullptr, bits);
    {   // the encode must be repeatable
        auto E2 = xtSynthEncode(chans, rd->sampleRate, type, 64, nullptr, bits);
        bool same = E->vpdt == E2->vpdt && E->solo.segStart == E2->solo.segStart && E->solo.in == E2->solo.in
                 && E->solo.inR == E2->solo.inR;
        for (int k = 0; k < 6; ++k) same = same && E->bands.buf[k] == E2->bands.buf[k];
        std::printf("encode repeatable: %s\n", same ? "yes" : "NO");
    }
    XtSynth ref; ref.prepare(rate); ref.setSample(E); ref.setControls(refCtl);

    const int total = (int)(in.getNumSamples() * rate / rd->sampleRate);
    double maxDiff = 0, energy = 0, side = 0;
    long firstDiff = -1;
    juce::AudioBuffer<float> buf(2, block);
    std::vector<float> rl(block), rr(block);
    const int offBlock = total / 2 / block * block, offAt = 37;    // note-off halfway, inside a block
    for (int done = 0; done < total; done += block) {
        juce::MidiBuffer midi;
        if (done == 0) { midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)noteVel), 100); }
        if (done == offBlock) { midi.addEvent(juce::MidiMessage::noteOff(1, note, (juce::uint8)100), offAt); }
        p.processBlock(buf, midi);
        juce::ScopedNoDenormals ftz;   // processBlock runs with denormals flushed (as the SH-4 does, FPSCR.DN = 1)
        if (std::getenv("TEST_CONTROLS")) ref.setControls(refCtl);   // as processBlock does each block
        if (done == 0) { ref.render(rl.data(), rr.data(), 100); ref.noteOn(note, (float)noteVel * (1.0f / 127.0f)); ref.render(rl.data() + 100, rr.data() + 100, block - 100); }
        else if (done == offBlock) { ref.render(rl.data(), rr.data(), offAt); ref.noteOff(note, 100 / 127.0f); ref.render(rl.data() + offAt, rr.data() + offAt, block - offAt); }
        else ref.render(rl.data(), rr.data(), block);
        for (int i = 0; i < block; ++i) {
            if (firstDiff < 0 && (buf.getSample(0, i) != rl[(size_t)i] || buf.getSample(1, i) != rr[(size_t)i])) firstDiff = done + i;
            maxDiff = std::max(maxDiff, (double)std::abs(buf.getSample(0, i) - rl[(size_t)i]));
            maxDiff = std::max(maxDiff, (double)std::abs(buf.getSample(1, i) - rr[(size_t)i]));
            energy += (double)buf.getSample(0, i) * buf.getSample(0, i);
            side += (double)(buf.getSample(0, i) - buf.getSample(1, i)) * (buf.getSample(0, i) - buf.getSample(1, i));
        }
    }
    std::printf("rendered %d frames at %.0f Hz: rms %.4f (L-R %.4f), max |plugin - XtSynth| = %g (first at frame %ld)\n",
                total, rate, std::sqrt(energy / total), std::sqrt(side / total), maxDiff, firstDiff);
    return maxDiff == 0 && energy > 0 ? 0 : 2;
}
