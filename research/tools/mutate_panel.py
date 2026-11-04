#!/usr/bin/env python3
# mutate_panel.py [name ...]: planted errors in the effects' panel and the processor's wiring of them, each built into PluginTest and run with the settings
# that should show it (VSynthEmuTest; about 30 s each, one at a time, and it edits the sources in place while it does, restoring them).  One line each: caught,
# MISSED, or ERROR.  Needs build/plugin configured, and analysis/test_files.
import os, subprocess, sys
REPO = os.path.abspath(os.path.dirname(os.path.abspath(__file__)) + '/../..')
os.chdir(REPO)
FP='plugin/Source/FxPanel.cpp'; PP='plugin/Source/PluginProcessor.cpp'
MUTS=[
 ('panel-no-set',FP,'processor_.setFxParam(row.id, (int)std::lround(row.value.getValue()));',';','panel'),
 ('panel-range',FP,'row.value.setRange(p.low, p.high, 1.0);','row.value.setRange(p.low, p.high + 1, 1.0);','panel'),
 ('panel-value',FP,'row->value.setValue(processor_.fxParam(row->id, row->def), juce::dontSendNotification);','row->value.setValue(row->def, juce::dontSendNotification);','panel'),
 ('panel-no-follow',FP,'row->value.setValue(processor_.fxParam(row->id, row->def), juce::dontSendNotification);',';','panel'),
 ('panel-name',FP,'row.name.setText(p.name, juce::dontSendNotification);','row.name.setText(p.name + " ", juce::dontSendNotification);','panel'),
 ('panel-stale-type',FP,'if (page != shownPage_ || type != shownType_) {','if (page != shownPage_) {','panel'),
 ('panel-stale-page',FP,'if (page != shownPage_ || type != shownType_) {','if (type != shownType_) {','panel'),
 ('panel-eq-page',FP,'page == 3 ? cat.eq :','page == 3 ? cat.mfx[1] :','panel'),
 ('panel-wrong-type',FP,'page == 1 ? chorus_.type.getSelectedItemIndex() : page == 2 ? reverb_.type.getSelectedItemIndex() : 0;','page == 1 ? reverb_.type.getSelectedItemIndex() : page == 2 ? chorus_.type.getSelectedItemIndex() : 0;','panel'),
 ('panel-extra-row',FP,'const bool shown = i < params.size();','const bool shown = true;','crash-or-panel'),
 ('panel-grid-height',FP,'grid_.setSize(view_.getWidth() - kScrollBarW, (int)params.size() * kParamH);','grid_.setSize(view_.getWidth() - kScrollBarW, 10);','panel'),
 ('knob-swap',PP,'{ "fxChoMaster", 0x1fd }','{ "fxChoMaster", 0x231 }','fx'),
 ('knob-id-gain',PP,'{ "fxGain", 0x76 }','{ "fxGain", 0x3e }','fx'),
 ('state-no-edits',PP,'state.setProperty("fxEdits", edits, nullptr);',';','fx-edit'),
 ('state-no-back',PP,'if (pair.contains("=")) setFxParam(','if (false) setFxParam(','fx-edit'),
 ('no-apply-knobs',PP,'for (const auto& k : kFxKnobs) c.set(k.id, (int)std::lround(apvts.getRawParameterValue(k.name)->load()));',';','fx-live'),
 ('no-apply-edits',PP,'if (v != kFxNoEdit) c.set(id, v); }\n}','}\n}','fx-live'),
 ('no-tick',PP,'    c.tick();\n',';\n','fx-live'),
 ('no-matrix',PP,'c.setMatrix(synth_.fxMatrix(0), synth_.fxMatrix(1), synth_.fxMatrix(2));',';','fx-matrix'),
 ('no-clock',PP,'c.outsideClock(synth_.fxClockSix());',';','fx-host'),
 ('no-clock-source',PP,'c.set(0x41, synth_.fxClockSix() ? 1 : 0);',';','fx-host'),
 ('no-tempo',PP,'c.set(0x11d, synth_.fxPatchTempo());',';','fx-tempo'),
 ('send-swap',PP,'c.chorusSend   = knob(Params::kTvaChoSend);','c.chorusSend   = knob(Params::kTvaRevSend);','fx-bus'),
 ('assign-off',PP,'c.outputAssign = knob(Params::kOutputAssign);',';','fx-assign'),
 ('mode-off',PP,'c.parallel     = knob(Params::kOutputMode) != 0;',';','fx-mode'),
]
ROM=os.environ.get('ROM', REPO + '/firmware/unpacked/V-SynthXT.rom.2.bin'); W='analysis/test_files/raw/chord_Cmaj_original.wav'
ENV={
 'panel':{'TEST_FX_PANEL':'1'},
 'crash-or-panel':{'TEST_FX_PANEL':'1'},
 'fx':{'TEST_FX':'1'},
 'fx-edit':{'TEST_FX':'1','TEST_FX_EDIT':'0x66=20'},
 'fx-live':{'TEST_FX':'1','TEST_FX_KNOBS':'fxRevOn=1,fxRevType=4','TEST_BUS':'0,100,0,1','TEST_FX_LIVE':'24000:fxRevType=8,36000:0x66=25,48000:fxMaster=90'},
 'fx-matrix':{'TEST_FX':'1','TEST_FX_KNOBS':'fxMfxOn=1,fxMfxType=23','TEST_MATRIX':'1,75,63,0,0','TEST_CC':'1,127,30000'},
 'fx-host':{'TEST_FX':'1','TEST_HOST':'170,0,1','TEST_FX_KNOBS':'fxRevOn=1,fxRevType=10','TEST_BUS':'0,100,0,1'},
 'fx-tempo':{'TEST_FX':'1','TEST_FX_KNOBS':'fxRevOn=1,fxRevType=10','TEST_BUS':'0,100,0,1','TEST_TEMPO':'170'},
 'fx-bus':{'TEST_FX':'1','TEST_BUS':'100,0,0,1','TEST_FX_KNOBS':'fxChoOn=1,fxChoType=1'},
 'fx-assign':{'TEST_FX':'1','TEST_BUS':'0,0,1,1'},
 'fx-mode':{'TEST_FX':'1','TEST_BUS':'0,0,2,0'},
}
SDK=os.environ.get('SDK','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk')
def build():
    r=subprocess.run(['cmake','--build','build/plugin','--target','VSynthEmuTest','-j','10'],capture_output=True,text=True,env=dict(os.environ,SDKROOT=SDK))
    return r.stdout if r.returncode==0 else 'failed: '+r.stderr[-300:]
if sys.argv[1:]: MUTS=[m for m in MUTS if m[0] in sys.argv[1:]]
def run(kind):
    env=dict(os.environ); env.update(ENV[kind])
    return subprocess.run(['build/plugin/VSynthEmuTest',ROM,W,'60','ensemble'],capture_output=True,text=True,env=env,timeout=300)
missed=0
# ENV and MUTS are above; a build first, so the first error's time isn't the source's own.
build()
for name,path,old,new,kind in MUTS:
    src=open(path).read()
    if old not in src: print('NOTEXT',name); missed+=1; continue
    open(path,'w').write(src.replace(old,new,1))
    try:
        b=build()
        if 'Built target VSynthEmuTest' not in b: print('ERROR ',name,b[:200]); missed+=1; continue
        r=run(kind)
        if r.returncode==0: print('MISSED',name); missed+=1
        else: print('caught',name,r.returncode,(r.stderr.strip().split('\n')[-1] if r.stderr.strip() else r.stdout.strip().split('\n')[-1])[:90])
    finally:
        open(path,'w').write(src)
print('planted',len(MUTS),'missed',missed)
build()
