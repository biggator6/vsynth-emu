// End-to-end check of the plugin processor's XT path: ROM, file load,
// background encode, sample-accurate MIDI, render — against XtSynth driven
// directly on the same input.  Build: cmake --build <dir> --target VSynthEmuTest
// Run:   VSynthEmuTest <rom.2.bin> <file.wav> [note]
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <string>
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
    // TEST_SNAPSHOT=file.png: the editor drawn offscreen, for checking the
    // layout (TEST_SNAPSHOT_COSM2=1: with the COSM rows on COSM 2;
    // TEST_SNAPSHOT_TYPE=n: COSM 1's type, e.g. 6 for the 2nd Side Band rows)
    if (const char* snap = std::getenv("TEST_SNAPSHOT")) {
        if (const char* t = std::getenv("TEST_SNAPSHOT_TYPE")) {
            auto* prm = p.apvts.getParameter("cosmType");
            prm->setValueNotifyingHost(prm->convertTo0to1((float)std::atoi(t)));
        }
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        if (std::getenv("TEST_SNAPSHOT_COSM2"))      // the COSM rows switched to COSM 2 (the selector re-attaches them)
            for (juce::Component* ch : ed->getChildren())
                if (auto* cb = dynamic_cast<juce::ComboBox*>(ch); cb && cb->getItemText(0) == "COSM 1") cb->setSelectedId(2, juce::sendNotificationSync);
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
    p.loadFile(juce::File(argv[2]));
    for (int i = 0; i < 600 && !p.getStatusText().startsWith("Encoded"); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));   // the encode runs on the pool thread
        if (p.getStatusText().startsWith("Encode failed")) break;
    }
    std::printf("status: %s\n", p.getStatusText().toRawUTF8());
    for (const char* id : { "pitchShift", "patchCoarse", "patchFine", "randomPitch", "oscCoarse", "oscFine", "oscPitch", "pitchKF", "oscTime", "oscTimeOffset", "oscFormant", "energy",
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
    int note2 = -1, at2 = 0;                                         // TEST_NOTE2="note,frame": a second note-on
    if (const char* e = std::getenv("TEST_NOTE2")) std::sscanf(e, "%d,%d", &note2, &at2);
    const int n2Block = note2 >= 0 ? at2 / block * block : -1, n2At = at2 % block;
    if (n2Block == 0 || n2Block == offBlock) { std::fprintf(stderr, "TEST_NOTE2 must fall in another block\n"); return 1; }
    int bendValue = -1, bendFrame = 0;                               // TEST_BEND="value,frame": the pitch wheel
    if (const char* e = std::getenv("TEST_BEND")) std::sscanf(e, "%d,%d", &bendValue, &bendFrame);
    const int bBlock = bendValue >= 0 ? bendFrame / block * block : -1, bAt = bendFrame % block;
    if (bendValue >= 0 && (bBlock == 0 || bBlock == offBlock || bBlock == n2Block)) { std::fprintf(stderr, "TEST_BEND must fall in another block\n"); return 1; }
    for (int done = 0; done < total; done += block) {
        juce::MidiBuffer midi;
        if (done == 0) { midi.addEvent(juce::MidiMessage::noteOn(1, note, (juce::uint8)noteVel), 100); }
        if (done == offBlock) { midi.addEvent(juce::MidiMessage::noteOff(1, note, (juce::uint8)100), offAt); }
        if (done == n2Block) { midi.addEvent(juce::MidiMessage::noteOn(1, note2, (juce::uint8)noteVel), n2At); }
        if (done == bBlock) { midi.addEvent(juce::MidiMessage::pitchWheel(1, bendValue), bAt); }
        p.processBlock(buf, midi);
        juce::ScopedNoDenormals ftz;   // processBlock runs with denormals flushed (as the SH-4 does, FPSCR.DN = 1)
        if (std::getenv("TEST_CONTROLS")) ref.setControls(refCtl);   // as processBlock does each block
        if (done == 0) { ref.render(rl.data(), rr.data(), 100); ref.noteOn(note, (float)noteVel * (1.0f / 127.0f)); ref.render(rl.data() + 100, rr.data() + 100, block - 100); }
        else if (done == offBlock) { ref.render(rl.data(), rr.data(), offAt); ref.noteOff(note, 100 / 127.0f); ref.render(rl.data() + offAt, rr.data() + offAt, block - offAt); }
        else if (done == n2Block) { ref.render(rl.data(), rr.data(), n2At); ref.noteOn(note2, (float)noteVel * (1.0f / 127.0f)); ref.render(rl.data() + n2At, rr.data() + n2At, block - n2At); }
        else if (done == bBlock) { ref.render(rl.data(), rr.data(), bAt); ref.pitchBend(bendValue); ref.render(rl.data() + bAt, rr.data() + bAt, block - bAt); }
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
