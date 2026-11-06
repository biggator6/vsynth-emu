#!/usr/bin/env python3
# mutate_panel.py [--editor] [name ...]: planted errors in the effects' panel, the values' text, the processor's wiring of them and the editor's layout (--editor: those alone), each built into PluginTest and run with the settings
# that should show it (VSynthEmuTest; about 30 s each, one at a time, and it edits the sources in place while it does, restoring them).  One line each: caught,
# MISSED, or ERROR.  Needs build/plugin configured, and analysis/test_files.
import os, subprocess, sys
REPO = os.path.abspath(os.path.dirname(os.path.abspath(__file__)) + '/../..')
os.chdir(REPO)
FP='plugin/Source/FxPanel.cpp'; PP='plugin/Source/PluginProcessor.cpp'; XV='plugin/Source/XtFxControl.cpp'; EP='plugin/Source/PluginEditor.cpp'; UP='plugin/Source/UiParts.h'
TEXT='row.value.textFromValueFunction = [values, id = p.id](double v) { return juce::String(values->text(id, (int)std::lround(v))); };\n        '
TYPE='row.value.valueFromTextFunction = [values, &row, id = p.id, low = p.low, high = p.high](const juce::String& typed) { return (double)values->value(id, low, high, typed.toStdString(), (int)std::lround(row.value.getValue())); };\n        '
RANGE='row.value.setRange(p.low, p.high, 1.0);'
MUTS=[
 # The editor's layout (TEST_UI_LAYOUT, TEST_FX_PANEL)
 ('editor-vari-unplaced',EP,'        wave.head = { { &variBtn_, 60 }, { &playbackBox_, 110 } };\n','        wave.head = { { &playbackBox_, 110 } };\n','layout'),
 ('editor-vari-hide',EP,'(juce::Component*)&loopBtn_, (juce::Component*)&variBtn_,','(juce::Component*)&loopBtn_,','layout'),
 ('editor-vari-osc',EP,'variAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, id("vari"), variBtn_);','variAtt_       = std::make_unique<ButtonAtt>(processor_.apvts, "vari", variBtn_);','layout'),
 ('patch-vari-read',PP,'    c.vari         = apvts.getRawParameterValue(Params::kVari)->load() > 0.5f;','    c.vari         = true;','patch'),
 ('patch-vari2-read',PP,'        o.vari = apvts.getRawParameterValue(oscId(1, Params::kVari))->load() > 0.5f;','        o.vari = true;','patch'),
 ('patch-vari-write',PP,'    set(Params::kVari, c.vari ? 1 : 0);','    ;','patch'),
 ('editor-start-unplaced',EP,', lr(startLabel_, startSlider_) };',' };','layout'),
 ('editor-start-osc',EP,'startAtt_      = std::make_unique<SliderAtt>(processor_.apvts, id("startOffset"), startSlider_);','startAtt_      = std::make_unique<SliderAtt>(processor_.apvts, "startOffset", startSlider_);','layout'),
 ('lite-own',PP,'own = w.ownType == 0 || w.ownType == 1','own = w.ownType == 1','lite'),
 ('lite-choice',PP,'choice == 3 ? 5 : choice == 4 ? 0 : own','choice == 3 ? 5 : own','lite-pick'),
 ('lite-riff',PP,' || !std::memcmp(head + i, "vpsc", 4)) return true;',') return true;','lite'),
 ('patch-start-read',PP,'    c.startOffset  = knob(Params::kStartOffset);','    c.startOffset  = 0;','patch'),
 ('patch-start2-read',PP,'        o.startOffset = knob2(Params::kStartOffset);','        o.startOffset = 0;','patch'),
 ('patch-start-write',PP,'    set(Params::kStartOffset, (float)startOffsetParam(c.startOffset));','    ;','patch'),
 ('patch-start2-write',PP,'        set(oscId(1, Params::kStartOffset), (float)startOffsetParam(c.osc2.startOffset));','        ;','patch'),
 ('editor-no-build',EP,'chrome { &buildLabel_, &algoLabel_,','chrome { &algoLabel_,','layout'),
 ('editor-build-unplaced',EP,'buildLabel_.setBounds(16, Ui::headerH - 17, hdr.getRight() - 16, 13);',';','layout'),
 ('editor-build-narrow',EP,'buildLabel_.setBounds(16, Ui::headerH - 17, hdr.getRight() - 16, 13);','buildLabel_.setBounds(16, Ui::headerH - 17, 40, 13);','layout'),
 ('editor-build-text',EP,'buildLabel_.setText(juce::String(xtBuildId()) + "  " + xtBuildTime(), juce::dontSendNotification);','buildLabel_.setText("v150", juce::dontSendNotification);','layout'),
 ('editor-no-patch-button',EP,'&romButton_, &loadButton_, &patchButton_, &fileNameLabel_,','&romButton_, &loadButton_, &fileNameLabel_,','layout'),
 ('editor-patch-unplaced',EP,'patchButton_.setBounds(src.removeFromLeft(100));',';','layout'),
 ('editor-no-panel',EP,'&fxOnBtn_, fxPanel_.get() };','&fxOnBtn_ };','panel'),
 ('editor-no-switch',EP,'&fxOnBtn_, fxPanel_.get() };','fxPanel_.get() };','layout'),
 ('editor-panel-always',EP,'fxPanel_->setVisible(tab_ == kEffectsTab);','fxPanel_->setVisible(true);','layout'),
 ('editor-panel-never',EP,'fxPanel_->setVisible(tab_ == kEffectsTab);','fxPanel_->setVisible(false);','layout'),
 ('editor-page-stuck',EP,'pages_[i]->setVisible(i == tab_);','pages_[i]->setVisible(i == 0);','layout'),
 ('editor-no-row',EP,'for (int i : { 8, 9, 10, 11 }) level.rows.push_back(rowOfLevel(i));','for (int i : { 8, 9, 10 }) level.rows.push_back(rowOfLevel(i));','layout'),
 ('editor-no-card',EP,'p.add(tempo);',';','layout'),
 ('editor-cosm-rows',EP,'addAll(tbLabels_, tbSliders_, kTbKnobs);',';','layout'),
 ('editor-row-gap',UP,'rowGap = 4','rowGap = -8','layout'),
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
 # The Effects tab's cards
 ('editor-fx-seg',FP,'pageSeg_.onChange = [this](int i) { showPage(i); };',';','layout'),
 ('editor-fx-seg-follow',FP,'pageSeg_.setIndex(page_);',';','layout'),
 ('editor-fx-always',FP,'show(chorus_, page_ == 1);','show(chorus_, true);','layout'),
 ('editor-fx-never',FP,'show(reverb_, page_ == 2);','show(reverb_, false);','layout'),
 ('editor-fx-system-never',FP,'const bool sys = page_ == 3;','const bool sys = false;','layout'),
 ('editor-fx-no-levels',FP,'for (auto& l : b.levels) c.rows.push_back(row(l->label, l->slider));',';','layout'),
 ('editor-fx-names',FP,'kNameW = 130','kNameW = 30','layout'),
 ('editor-fx-left',FP,'kLeftW = 250','kLeftW = 100','layout'),
 ('editor-fx-fill',UP,'const int h = c.fill ? juce::jmax(0, area.getBottom() - y[juce::jmax(0, c.col)]) : heightOf(c);','const int h = heightOf(c);','layout'),
 ('editor-fx-same-place',FP,'if (page > 0) c.sameAs = firstCard;',';','layout'),
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
 ('engine-level',PP,'buffer.applyGain(0.125f);',';','rom'),
 ('rom-not-beside',PP,'cand.add(juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile(kRomName).getFullPathName());',';','rom'),
 ('rom-order',PP,'cand.add(appDir().getChildFile(kRomName).getFullPathName());\n    cand.add(appSettings()->getValue(kRomKey));','cand.add(appSettings()->getValue(kRomKey));\n    cand.add(appDir().getChildFile(kRomName).getFullPathName());','rom'),
 ('rom-env-last',PP,'if (const char* p = std::getenv("VSE_XT_ROM")) cand.add(juce::String::fromUTF8(p));\n','','rom'),
 ('rom-not-kept',PP,'settings->setValue(kRomKey, file.getFullPathName());',';','rom'),
 ('rom-project-kept',PP,'        openRom(juce::File(rom));','        setRomFile(juce::File(rom));','rom'),
 ('rom-read-again',PP,'if (path == loaded && xtSynthRomReady()) return true;',';','rom'),
 ('patch-writer',PP,'set(Params::kOscCoarse, c.oscCoarse);','set(Params::kOscCoarse, c.oscFine);','patch'),
 ('patch-writer-osc2',PP,'set(oscId(1, Params::kOscPitch), o.oscPitch);','set(oscId(1, Params::kOscPitch), c.oscPitch);','patch'),
 ('patch-writer-kf',PP,'set(Params::kAnaPwKF, c.ana.pwKf * 10);','set(Params::kAnaPwKF, c.ana.pwKf);','patch'),
 ('patch-writer-mono',PP,'set(Params::kMonoPoly, c.mono ? 0 : 1);','set(Params::kMonoPoly, c.mono ? 1 : 0);','patch'),
 ('patch-writer-cosm2',PP,'set(cosmId(k, "Cutoff"), m.cutoff);','set(cosmId(0, "Cutoff"), m.cutoff);','patch'),
 ('patch-writer-velocity',PP,'set(oscId(k, kEnvs[e] + "VelSens"), v[e]->sens);',';','patch'),
 ('patch-reader-velocity',PP,'v[e]->timeKF = kf(Params::oscId(k, kEnvs[e] + "TimeKF"));',';','patch'),
 ('patch-scale-range',PP,'juce::String("Scale Tune ") + names[i], -100, 100, 0)','juce::String("Scale Tune ") + names[i], -64, 63, 0)','patch'),
 ('patch-fx-params',PP,'if (!knob) { setFxParam(fx.id, fx.value); continue; }','if (!knob) continue;','patch'),
 ('patch-fx-knobs',PP,'if (auto* p = apvts.getParameter(knob)) p->setValueNotifyingHost(p->convertTo0to1((float)fx.value));',';','patch'),
 ('patch-wave-folder',PP,'file.getParentDirectory().getSiblingFile("Wave")','file.getParentDirectory().getChildFile("Wave")','patch'),
 ('patch-wave-number',PP,'xtPatchWaveNumber(store, zone, k);','xtPatchWaveNumber(store, zone, 0) + 1;','patch'),
 ('patch-wave-copy',PP,'f.getFileName().length() < best.getFileName().length()','f.getFileName().length() > best.getFileName().length()','patch'),
 ('patch-wave-settings',PP,'if (files[k].existsAsFile() && readVSynthWave(files[k], waves[k], &why)) { found[k] = true; xtWaveOntoControls(waves[k], o); }','if (files[k].existsAsFile() && readVSynthWave(files[k], waves[k], &why)) { found[k] = true; }','patch'),
 ('patch-wave-tuning',PP,'if (isOwn) xtWaveOntoEncoded(ownWave, *e);',';','patch'),
 ('patch-wave-left',PP,'else unloadWave(k);',';','patch'),
 ('patch-wave-note',PP,'else notes.add("no wave " + juce::String(number));',';','patch'),
 ('patch-encode-auto',PP,'p->setValueNotifyingHost(p->convertTo0to1(0.0f));   // Auto',';   // Auto','patch'),
 ('patch-restore-settings',PP,'loadFile(f, k, false);','loadFile(f, k);','patch'),
 ('patch-restore-name',PP,'patchName_ = apvts.state.getProperty("patchName").toString();',';','patch'),
 ('patch-loop-step',PP,'Params::kLoopStart, "Loop Start", juce::NormalisableRange<float>(0.0f, 100.0f)','Params::kLoopStart, "Loop Start", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f)','patch'),
 ('patch-tempo-step',PP,'Params::kOrigTempo, "Original Tempo", juce::NormalisableRange<float>(20.0f, 300.0f, 0.0001f)','Params::kOrigTempo, "Original Tempo", juce::NormalisableRange<float>(20.0f, 300.0f, 0.01f)','patch'),
 ('patch-folder-kept',PP,'settings->setValue(kPatchFolderKey, file.getParentDirectory().getFullPathName());',';','patch'),
 ('zones-store',PP,'apvts.state.setProperty("patchStore", zones > 1 ? juce::Base64::toBase64(store.data(), store.size()) : juce::String(), nullptr);',';','patch'),
 ('zones-restore',PP,'        loadZones(store, juce::File(apvts.state.getProperty("patchWaves").toString()), unused);','        ;','patch'),
 ('zones-set',PP,'        synth_.setZones(withWaves);','        ;','patch'),
 ('zones-wave-settings',PP,'            xtWaveOntoControls(w, o);\n            want.push_back({ z, k, number });','            want.push_back({ z, k, number });','patch'),
 ('zones-wave-tuning',PP,'            xtWaveOntoEncoded(w, *e);\n            enc[number] = std::move(e);','            enc[number] = std::move(e);','patch'),
 ('zones-one',PP,'if (zones < 2) { synth_.setZones(nullptr); zonesReady_.store(true); return; }','if (zones < 2) { zonesReady_.store(true); return; }','patch'),
 ('zones-note',PP,'    notes.add(juce::String(zones) + " zones, zone 1 on the panel");','    ;','patch'),
 ('zones-wave-number',PP,'            const int number = xtPatchWaveNumber(store, z, k);','            const int number = xtPatchWaveNumber(store, z, 0);','patch'),
 ('zones-wave-zone',PP,'            const int number = xtPatchWaveNumber(store, z, k);','            const int number = xtPatchWaveNumber(store, 0, k);','patch'),
 ('rom-bad-drops',PP,'if (loaded.isNotEmpty()) xtSynthSetRom(loaded.toStdString());',';','rom'),
 ('patch-playback-read',PP,'    c.playbackMode = knob(Params::kPlaybackMode);','    c.playbackMode = 0;','patch'),
 ('patch-playback2-read',PP,'        o.playbackMode = knob2(Params::kPlaybackMode);','        o.playbackMode = 0;','patch'),
 ('patch-playback-write',PP,'    set(Params::kPlaybackMode, c.playbackMode);','    ;','patch'),
 ('patch-playback2-write',PP,'        set(oscId(1, Params::kPlaybackMode), o.playbackMode);','        ;','patch'),
 ('patch-own-events-osc',PP,'isOwn && type == own ? &ownWave.events : nullptr','nullptr','solo-own'),
 ('patch-own-events-zone',PP,'auto e = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16, &w.events);','auto e = xtSynthEncode(chans, 44100.0, type, 64, nullptr, 16);','patch'),
 ('patch-playback-vpno','plugin/Source/XtPatch.cpp',"    if (w.vpnoCount >= 0) { e.vpno = w.vpno; e.vpnoCount = w.vpnoCount; }   // its own list in place of the encode's\n",'','patch'),
 ('editor-playback-osc',EP,'    playbackAtt_   = std::make_unique<ComboAtt>(processor_.apvts, id("playbackMode"), playbackBox_);','    playbackAtt_   = std::make_unique<ComboAtt>(processor_.apvts, "playbackMode", playbackBox_);','layout'),
 ('editor-playback-card',EP,'        wave.head = { { &variBtn_, 60 }, { &playbackBox_, 110 } };','        wave.head = { { &variBtn_, 60 } };','layout'),
 ('editor-playback-hide',EP,'(juce::Component*)&startSlider_, (juce::Component*)&playbackBox_, (juce::Component*)&offsetLabel_,','(juce::Component*)&startSlider_, (juce::Component*)&offsetLabel_,','layout'),
]
ROM=os.environ.get('ROM', REPO + '/firmware/unpacked/V-SynthXT.rom.2.bin'); W='analysis/test_files/raw/chord_Cmaj_original.wav'
ENV={
 'layout':{'TEST_UI_LAYOUT':'1'},
 'rom':{'TEST_ROM_SEARCH':'1'},
 'patch':{'TEST_PATCH_APPLY':'1'},
 'solo-own':{'TEST_PLAYBACK':'3','TEST_NOTE2':'64,30000','_file':'analysis/projects/Emu_test.prj/Wave/001 ITS-3AM.wav','_mode':'auto'},
 'lite':{'TEST_EXPECT_ENCODE':'LITE','_file':'analysis/projects/Emu_test.prj/Wave/005 sine_400hz_r.wav','_mode':'auto'},
 'lite-pick':{'TEST_EXPECT_ENCODE':'LITE','_mode':'lite'},
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
    e=dict(ENV[kind]); f=e.pop('_file',W); m=e.pop('_mode','ensemble')   # a kind's own wave and Encode Type, or the chord as ENSEMBLE
    env=dict(os.environ); env.update(e)
    return subprocess.run(['build/plugin/VSynthEmuTest',ROM,f,'60',m],capture_output=True,text=True,env=env,timeout=300)
missed=0
# ENV and MUTS are above; a build first, so the first error's time isn't the source's own.
build()
# Every run must fail for the plant's reason: with nothing planted, each kind of run has to pass first.
for kind in sorted({m[4] for m in MUTS}):
    r=run(kind)
    if r.returncode!=0: sys.exit('the unplanted run '+kind+' fails: '+(r.stderr.strip().split('\n')[-1] if r.stderr.strip() else '')[:120])
for name,path,old,new,kind in MUTS:
    src=open(path).read()
    if old not in src: print('NOTEXT',name); missed+=1; continue
    open(path,'w').write(src.replace(old,new,1))
    try:
        b=build()
        if 'Built target VSynthEmuTest' not in b: print('ERROR ',name,b[:200]); missed+=1; continue
        r=run(kind)
        if 'ROM rejected' in r.stderr: print('ERROR ',name,'the ROM was rejected, so this proves nothing'); missed+=1
        elif r.returncode==0: print('MISSED',name); missed+=1
        else: print('caught',name,r.returncode,(r.stderr.strip().split('\n')[-1] if r.stderr.strip() else r.stdout.strip().split('\n')[-1])[:90])
    finally:
        open(path,'w').write(src)
print('planted',len(MUTS),'missed',missed)
build()
