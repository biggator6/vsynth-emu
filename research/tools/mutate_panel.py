#!/usr/bin/env python3
# mutate_panel.py [--editor] [name ...]: planted errors in the effects' panel, the values' text, the processor's wiring of them and the editor's layout (--editor: those alone), each built into PluginTest and run with the settings
# that should show it (VSynthEmuTest; about 30 s each, one at a time, and it edits the sources in place while it does, restoring them).  One line each: caught,
# MISSED, or ERROR.  Needs build/plugin configured, and analysis/test_files.
import os, subprocess, sys
REPO = os.path.abspath(os.path.dirname(os.path.abspath(__file__)) + '/../..')
os.chdir(REPO)
FP='plugin/Source/FxPanel.cpp'; PP='plugin/Source/PluginProcessor.cpp'; XV='plugin/Source/XtFxControl.cpp'; EP='plugin/Source/PluginEditor.cpp'
TEXT='row.value.textFromValueFunction = [values, id = p.id](double v) { return juce::String(values->text(id, (int)std::lround(v))); };\n        '
TYPE='row.value.valueFromTextFunction = [values, &row, id = p.id, low = p.low, high = p.high](const juce::String& typed) { return (double)values->value(id, low, high, typed.toStdString(), (int)std::lround(row.value.getValue())); };\n        '
RANGE='row.value.setRange(p.low, p.high, 1.0);'
MUTS=[
 # The editor's layout (TEST_UI_LAYOUT, TEST_FX_PANEL)
 ('editor-no-panel',EP,'&fxOnBtn_, fxPanel_.get() };','&fxOnBtn_ };','panel'),
 ('editor-no-switch',EP,'&fxOnBtn_, fxPanel_.get() };','fxPanel_.get() };','layout'),
 ('editor-panel-always',EP,'fxPanel_->setVisible(tab_ == kEffectsTab);','fxPanel_->setVisible(true);','layout'),
 ('editor-panel-never',EP,'fxPanel_->setVisible(tab_ == kEffectsTab);','fxPanel_->setVisible(false);','layout'),
 ('editor-page-stuck',EP,'pages_[i]->setVisible(i == tab_);','pages_[i]->setVisible(i == 0);','layout'),
 ('editor-no-row',EP,'for (int i : { 8, 9, 10, 11 }) level.rows.push_back(rowOfLevel(i));','for (int i : { 8, 9, 10 }) level.rows.push_back(rowOfLevel(i));','layout'),
 ('editor-no-card',EP,'p.add(tempo);',';','layout'),
 ('editor-cosm-rows',EP,'addAll(tbLabels_, tbSliders_, kTbKnobs);',';','layout'),
 ('editor-row-gap',EP,'rowGap = 4','rowGap = -8','layout'),
 ('editor-cosm-rows-close',EP,'return area.withTrimmedTop(30 * r).withHeight(24);','return area.withTrimmedTop(20 * r).withHeight(24);','layout'),
 ('editor-narrow-env',EP,'v->setBounds(a.removeFromLeft(104).withTrimmedBottom(2));','v->setBounds(a.removeFromLeft(204).withTrimmedBottom(2));','layout'),
 ('editor-env-border',EP,'l->setBorderSize({ 1, 0, 1, 0 });            // a narrow cell',';                                            // a narrow cell','layout'),
 ('editor-toggle-narrow',EP,'osc.head = { { &lfoSyncBtn_, 84 } };','osc.head = { { &lfoSyncBtn_, 40 } };','layout'),
 ('editor-min-width',EP,'setResizeLimits(900, 600, 1400, 900);','setResizeLimits(700, 600, 1400, 900);','layout'),
 ('editor-min-height',EP,'setResizeLimits(900, 600, 1400, 900);','setResizeLimits(900, 400, 1400, 900);','layout'),
 ('editor-no-tab-name',EP,'"Matrix", "Effects" }','"Matrix" }','layout'),
 ('editor-tab-click',EP,'if (tabBounds(i).contains(e.getPosition())) { if (onChange) onChange(i); return; }','if (tabBounds(i).contains(e.getPosition())) { if (onChange) onChange(0); return; }','layout'),
 ('editor-seg-osc',EP,'oscSeg_->onChange = [this](int i) { oscSelBox_.setSelectedId(i + 1, juce::sendNotificationSync); };',';','layout'),
 ('editor-seg-follow',EP,'attachCosm(cosmSelBox_.getSelectedId() == 2 ? 1 : 0); cosmSeg_->setIndex(cosmK_); };','attachCosm(cosmSelBox_.getSelectedId() == 2 ? 1 : 0); };','layout'),
 ('editor-seg-mono',EP,'monoSeg_->onChange = [this](int i) { monoBox_.setSelectedItemIndex(i, juce::sendNotificationSync); };',';','layout'),
 ('editor-mono-follow',EP,'monoBox_.onChange = [this] { monoSeg_->setIndex(monoBox_.getSelectedItemIndex()); };',';','layout'),
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
 # The text in the box: the panel's side
 ('box-no-text',FP,'row.value.textFromValueFunction = [values, id = p.id](double v) { return juce::String(values->text(id, (int)std::lround(v))); };',';','panel'),
 ('box-wrong-id',FP,'[values, id = p.id](double v) { return juce::String(values->text(id, (int)std::lround(v))); };','[values, id = p.id](double v) { return juce::String(values->text(id + 1, (int)std::lround(v))); };','panel'),
 ('box-order',FP,TEXT+TYPE+RANGE,RANGE+'\n        '+TEXT+TYPE.rstrip(),'panel'),
 ('box-no-typing',FP,'row.value.valueFromTextFunction = [values, &row, id = p.id, low = p.low, high = p.high](const juce::String& typed) { return (double)values->value(id, low, high, typed.toStdString(), (int)std::lround(row.value.getValue())); };',';','panel'),
 ('box-typing-lost',FP,'(double)values->value(id, low, high, typed.toStdString(),','(double)values->value(id, low, high, std::string(),','panel'),
 ('box-typing-range',FP,'(double)values->value(id, low, high, typed.toStdString(),','(double)values->value(id, low, high - 1, typed.toStdString(),','panel'),
 ('gain-no-text',FP,'gain.textFromValueFunction = [values = cat.values](double v) { return juce::String(values->text(kOutputGain, (int)std::lround(v))); };',';','panel'),
 ('gain-wrong-id',FP,'constexpr int kOutputGain = 0x76;','constexpr int kOutputGain = 0x3e;','panel'),
 ('gain-no-update',FP,'gain.updateText();',';','panel'),
 # The text itself
 ('text-no-values',XV,'cat.values = std::make_shared<XtFxValueText>(romPath);',';','crash-or-panel'),
 ('text-with-unit',XV,'(uint32_t)value, 0 }, kSp, &at, kTextSteps','(uint32_t)value, 1 }, kSp, &at, kTextSteps','panel'),
 ('text-value',XV,'(uint32_t)value, 0 }, kSp, &at, kTextSteps','(uint32_t)value + 1, 0 }, kSp, &at, kTextSteps','crash-or-panel'),
 ('text-id',XV,'{ (uint32_t)formatOf(id), (uint32_t)value, 0 }, kSp, &at, kTextSteps','{ (uint32_t)formatOf(id) + 1, (uint32_t)value, 0 }, kSp, &at, kTextSteps','panel'),
 ('time-room1',XV,'case 0x814: case 0x815:','case 0x815:','panel'),
 ('time-room1-hi',XV,'case 0x814: case 0x815:','case 0x814:','panel'),
 ('time-room2',XV,'        case 0x828:                                                                     // Room 2\'s\n','','panel'),
 ('time-room3',XV,'        case 0x839:                                                                     // Room 3\'s\n','','panel'),
 ('time-hall1',XV,'case 0x84b: case 0x84c:','case 0x84c:','panel'),
 ('time-hall1-hi',XV,'case 0x84b: case 0x84c:','case 0x84b:','panel'),
 ('time-hall2',XV,'        case 0x861:                                                                     // Hall 2\'s\n','','panel'),
 ('time-hall3',XV,'        case 0x872:                                                                     // Hall 3\'s\n','','panel'),
 ('time-garage',XV,'        case 0x884:                                                                     // Garage\'s\n','','panel'),
 ('time-plate',XV,'case 0x896: case 0x897:','case 0x897:','panel'),
 ('time-plate-hi',XV,'case 0x896: case 0x897:','case 0x896:','panel'),
 ('time-short-table',XV,'kReverbTime = 0x894b2bf0;','kReverbTime = 0x894b2ba0;','panel'),
 ('time-truncated',XV,'(at * 100 + 22050) / 44100','(at * 100) / 44100','panel'),
 ('time-rate',XV,'(at * 100 + 22050) / 44100','(at * 100 + 24000) / 48000','panel'),
 ('time-digit',XV,'(hundredths % 100 < 10 ? "0" : "")','""','panel'),
 ('time-unit',XV,'+ std::to_string(hundredths % 100) + " " + unit(id);','+ std::to_string(hundredths % 100);','panel'),
 ('lent-hall2-size',XV,'case 0x864: return 0x82b;','','panel'),
 ('lent-hall3-size',XV,'case 0x875: case 0x887: return 0x83c;','case 0x887: return 0x83c;','panel'),
 ('lent-garage-size',XV,'case 0x875: case 0x887: return 0x83c;','case 0x875: return 0x83c;','panel'),
 ('lent-xover',XV,'case 0x898: return 0x84d;','','panel'),
 ('text-by-value',XV,'(uint32_t)id << 16 | (uint16_t)value','(uint16_t)value','panel'),
 ('text-by-id',XV,'(uint32_t)id << 16 | (uint16_t)value','(uint32_t)id << 16','panel'),
 ('text-spaces',XV,'t = from == std::string::npos ? std::string() : t.substr(from, to - from + 1);',';','panel'),
 ('unit-none',XV,'if (!u.empty()) t += " " + u;',';','panel'),
 ('unit-on-names',XV,'if (leadingNumber(t, number, rest) && rest == t.size()) {','{','panel'),
 ('unit-twice',XV,'if (leadingNumber(t, number, rest) && rest == t.size()) {','if (leadingNumber(t, number, rest)) {','panel'),
 ('unit-brackets',XV,"if (ch != '[' && ch != ']') out += (char)ch;",'out += (char)ch;','panel'),
 ('unit-no-space',XV,'t += " " + u;','t += u;','panel'),
 ('unit-row-time',XV,'id == 0x8b6 ? 0x8b2 : ','','panel'),
 ('unit-row-level',XV,'id == 0x8b7 ? 0x8b6 : ','','panel'),
 ('unit-row-hc',XV,'id == 0x8b8 ? 0x8b7 : ','','panel'),
 ('unit-table-low',XV,'c.rd32(kValueStrings + 4 * (uint32_t)row)','c.rd32(kValueStringsHigh + 4 * (uint32_t)row)','panel'),
 ('unit-table-high',XV,'c.rd32(kValueStringsHigh + 4 * (uint32_t)(row - kHighIds))','c.rd32(kValueStringsHigh + 4 * (uint32_t)(row - kHighIds + 1))','panel'),
 ('note-1-16',XV,'case 0x11: return "1/16";','case 0x11: return "1/32";','panel'),
 ('note-1-8',XV,'case 0x12: return "1/8";','case 0x12: return "1/4";','panel'),
 ('note-1-4',XV,'case 0x18: return "1/4";','case 0x18: return "1/2";','panel'),
 ('note-1-2',XV,'case 0x13: return "1/2";','case 0x13: return "1/4";','panel'),
 ('note-1-1',XV,'case 0x14: return "1/1";','case 0x14: return "2/1";','panel'),
 ('note-2-1',XV,'case 0x15: return "2/1";','case 0x15: return "1/1";','panel'),
 ('note-unnamed',XV,'case 0x15: return "2/1";','','panel'),
 ('note-triplet',XV,'t[1] == 0x17 ? "T"','t[1] == 0x17 ? ""','panel'),
 ('note-dot',XV,"t[1] == '.' ? \".\"","t[1] == '.' ? \"\"",'panel'),
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
 'layout':{'TEST_UI_LAYOUT':'1'},
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
if sys.argv[1:]: MUTS=[m for m in MUTS if m[0] in sys.argv[1:] or ('--editor' in sys.argv and m[0].startswith('editor-'))]
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
