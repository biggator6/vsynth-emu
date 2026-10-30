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
    p.loadFile(juce::File(argv[2]));
    for (int i = 0; i < 600 && !p.getStatusText().startsWith("Encoded"); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));   // the encode runs on the pool thread
        if (p.getStatusText().startsWith("Encode failed")) break;
    }
    std::printf("status: %s\n", p.getStatusText().toRawUTF8());
    for (const char* id : { "pitchShift", "timeStretch", "formantShift", "energy", "rootKey", "ampAttack", "ampDecay", "ampSustain", "ampRelease", "encodeDepth" })
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
        if (done == 0) { midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)127), 100); }
        if (done == offBlock) { midi.addEvent(juce::MidiMessage::noteOff(1, note, (juce::uint8)100), offAt); }
        p.processBlock(buf, midi);
        juce::ScopedNoDenormals ftz;   // processBlock runs with denormals flushed (as the SH-4 does, FPSCR.DN = 1)
        if (std::getenv("TEST_CONTROLS")) ref.setControls(refCtl);   // as processBlock does each block
        if (done == 0) { ref.render(rl.data(), rr.data(), 100); ref.noteOn(note, 1.0f); ref.render(rl.data() + 100, rr.data() + 100, block - 100); }
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
