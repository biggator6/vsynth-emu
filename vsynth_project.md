
  
  

What features would be required to emulate a Roland V-synth in a VST plugin

## Apr 3

Claude responded: The Roland V-Synth (2003) was a groundbreaking synthesizer.

The Roland V-Synth (2003) was a groundbreaking synthesizer. Here's a breakdown of the core features needed to emulate it in a VST:

## Sound Generation Engines

VariPhrase Technology — the V-Synth's signature feature. Requires:

•Time-stretching independent of pitch (and vice versa)

•Formant shifting independent of pitch

•Real-time audio manipulation of PCM samples without artifacts

•This is the hardest part to emulate faithfully — Roland's VariPhrase was proprietary DSP

## Elastic Audio Synthesis (EAS) — the core synthesis engine:

•Two oscillators, each switchable between PCM sample playback, analog modeling

oscillators, or external audio input

•Per-oscillator VariPhrase processing

•COSM (Composite Object Sound Modeling) — Roland's physical modeling layer:

•Acoustic resonance modeling (guitar body, violin body, etc.)

•Pickup simulation

•Amp/speaker modeling

## Modulation & Control

•Time-Variant Filter (TVF) — resonant multimode filter with envelope

•Time-Variant Amplifier (TVA) — amplitude envelope

•Two independent envelopes (ADSR + more stages)

•Two LFOs with syncable rates and multiple waveforms

•Step-time arpeggiator/phrase sequencer with time-based modulation

•Matrix modulation — flexible routing of any source to any destination

## Unique Interface Features

•D-Beam controller — infrared proximity sensor for gestural control (can be mapped to a mod wheel or expression input in a VST)

•Touch-sensitive ribbon controllers (two) — map to pitch bend / filter / any parameter

•Knob/slider automation — all parameters fully automatable

## Effects Chain

•Multi-effects processor (chorus, reverb, delay, distortion, EQ)

•Effects routed per-oscillator as well as globally

•MFX, chorus, reverb running in parallel

## Polyphony & Voice Architecture

•Up to 20-voice polyphony

•Split/layer modes for two independent patches

•Stereo voice output per partial

## The Hard Parts (Technical Challenges)

##
| Feature | Challenge |
|--|--|
| VariPhrase | Requires high-quality phase vocoder or similar time-domain stretching |
| COSM  | Complex modal/physical modeling — computationally expensive  |
| Ribbon controller|Needs MPE or custom MIDI mapping in a VST context |
| D-Beam|Can only be approximated via MIDI CC |
|Internal sample ROM | Licensing Roland's PCM content is not possible — custom samples needed|


## Recommended DSP Approaches for a VST Dev

•Time-stretching: élastique (zplane) or Rubber Band Library for VariPhrase-style processing

•Physical modeling: modal synthesis or waveguide synthesis for COSM

•Oscillators: bandlimited wavetable or BLEP/BLAMP for analog modeling

•Framework: JUCE is the standard VST framework that handles all the plugin scaffolding

The VariPhrase engine is really the soul of the V-Synth — a convincing emulation of that alone would be 80% of the battle. Several modern plugins (like VocalSynth 2 or Krotos Reformer) tackle adjacent territory but nothing has fully replicated the V-Synth's complete feature set.

You said: why is variphrase so difficult to replicate?

  

why is variphrase so difficult to replicate? what differentiates it from regular pitch shifting?

## Apr 3

Claude responded: Great question.

Great question. VariPhrase sits in a different category from regular pitch shifting — here's why:

## What Regular Pitch Shifting Does

A standard pitch shifter does one thing: resample the audio. Speed it up = higher pitch, slow it down = lower pitch. The relationship between time and pitch is locked together physically, just like playing a tape faster or slower.

Even more sophisticated "pitch shift without time change" algorithms (like those in most DAWs) work by chopping audio into small grains or windows and repositioning/overlapping them — but they're still fundamentally treating pitch, time, and formants as a single bundled thing that gets manipulated together, with artifacts being the cost of separating them.

What VariPhrase Does Differently




Roland's VariPhrase decouples three independent axes that are normally entangled:

##
| Parameter | Normal Pitch Shift |VariPhrase |
|--|--|--|
| Pitch|Changes together with speed |Fully independent|
 |Time /Duration |Locked to pitch|Fully independent|
 |Formants |Shift with pitch |Fully independent|

This means you can:

•Raise the pitch of a vocal without it sounding like a chipmunk (formants stay put)

•Slow a phrase down without it dropping in pitch or getting warbly

•Shift formants without changing pitch at all (change a male voice to female timbre while singing the same note)

•Do all three simultaneously, in real time, with low latency

## Why That's Hard

Formant preservation is the crux of it. Human voice and many acoustic instruments have resonant peaks called formants that define the character of the sound — the vowel shape of a voice, the body resonance of a cello. When you pitch shift naively, formants shift with the pitch, which is what creates the "chipmunk" or "demon voice" effect.

To separate formants from pitch you need to:

• Analyze the spectral envelope in real time — identify where the formant peaks are

• Extract the source excitation (the pitched part) separately from the resonant filter shape

• Manipulate them independently, then resynthesize

This is essentially a source-filter model — the same idea behind LPC (Linear Predictive Coding) used in vocoders — but Roland's implementation had to do this with very low latency, high quality, and in a way that worked on any audio input, not just voice.

## The Phase Vocoder Problem

Most modern time-stretching uses a phase vocoder — it converts audio to the frequency domain (via FFT), manipulates it, then converts back. Phase vocoders are good but have well-known failure modes:

•Phasiness / smearing on transients

•Metallic artifacts on polyphonic or complex material

•Phase coherence problems between harmonics, causing a swirling, unnatural sound

Roland's VariPhrase was widely noted for sounding cleaner than contemporary phase vocoder approaches, suggesting they were doing something more sophisticated — likely a hybrid approach combining sinusoidal modeling, transient detection, and residual noise handling separately.

What Makes it Hard to Replicate Today

•Roland never published the algorithm — it's a trade secret

•The clean real-time performance suggests custom DSP optimizations that aren't obvious from the output alone

•Modern libraries like Rubber Band or zplane élastique get close but have their own artifact signatures — they don't sound identical

•Reverse-engineering it requires having access to a V-Synth and doing extensive blind testing across edge cases (transients, polyphony, formant extremes)  

The irony is that in 2025, the raw compute needed is trivial — the barrier is purely algorithmic knowledge, not processing power. Roland essentially solved a hard DSP problem in 2003 and quietly kept the solution to themselves.

You said: if you had access to a V-synth to analyze output of a plug-in vs the real machine, would you be able to tailor an algorithm to get very close?

## Apr 3

Claude responded: Yes, in principle — this is essentially what blind reverse engineering via black-box testing looks like, and it's a legitimate and well-trodden approach.

Here's how you'd actually do it systematically:

## The Core Methodology

You'd treat the V-Synth as an oracle — you feed it known inputs, capture known outputs, and use the difference between your plugin's output and the oracle's output as a gradient to improve your algorithm. This is the same fundamental approach used to emulate analog circuits (like what Cytomic did with their filter models).

## Test Signal Battery

You'd design a suite of highly controlled inputs that stress-test specific aspects of VariPhrase independently:

For pitch/formant separation:

•Sustained sine waves at known frequencies

•Sawtooth waves with known harmonic series

•Sung vowels held steady ("aaaah") — formants are well-defined and measurable

•Then shift pitch up/down by known intervals and compare formant positions in the output spectrogram

For time-stretching quality:

•A single clean transient (click or drum hit) — stretched 2x, 4x, 0.5x

•Measure smearing, pre-ringing, artifact frequency content

•This reveals a lot about whether they're using windowed FFT, granular, or something else

For the source-filter model:

•Whispered speech (no pitch, pure formant/noise) vs. voiced speech

•See how VariPhrase handles unpitched material

•A glottal pulse approximation (very buzzy sawtooth into the input)

For polyphonic handling:

•Two simultaneous sine waves — does the algorithm track them independently or treat them as one signal?

•Piano chord stretches — where phase vocoders typically fall apart

## Analysis Toolkit

With both signals captured (V-Synth output vs. your plugin output):

•Spectrogram comparison (high resolution STFT) — reveals formant trajectories, artifact smearing, phase behavior

•LPC analysis on both — directly extracts and compares the formant filter model

•Cepstral analysis — separates the spectral envelope (formants) from the fine structure (pitch) mathematically

•Null testing — phase-invert one signal and sum with the other; what remains is only the difference, making subtle artifacts clearly audible and visible

•Group delay analysis — reveals latency and phase behavior of the algorithm

## What You Could Actually Deduce

##
|Observable|What It Tells You|
|--|--|
|Formant stability during pitch shift|Quality of spectral envelope estimation|
|Transient smearing amount|Window size and overlap in time-freq transform|
|Artifact frequency content|Whether they use sinusoidal + residual modeling|
|Polyphonic behavior|Whether pitch tracking is per-partial or global|
|Latency floor|Minimum FFT window size they're committed to|
|Noise floor artifacts|How they handle the residual/noise component|

## The Iterative Loop

You'd essentially be doing gradient descent by ear and measurement:

•Hypothesis: "they're using a sinusoidal model with residual noise handling"

•Implement it

•Null test against V-Synth output

•The residual reveals what your model is missing

•Refine hypothesis, repeat

Over many iterations this converges. It's slow but it works — this is roughly how Antares Auto-Tune, Melodyne, and zplane's algorithms were refined over years.

  

## The Honest Ceiling

You'd likely get very close but not identical, for a few reasons:

•If Roland is using a proprietary basis function or a non-standard transform, you can observe its behavior but can't know the exact implementation — two different algorithms can produce perceptually identical output while being architecturally different

•Some decisions may be non-deterministic or depend on internal state in ways that are hard to fully characterize from output alone

•The edge cases (extreme stretch ratios, unusual timbres) would be the last 5% that's hardest to nail But "very close" in this context means perceptually indistinguishable on most real-world material — which for a VST plugin is absolutely good enough, and arguably the right goal. Nobody's going to null-test your plugin against a V-Synth in a mix.

This Is Actually a Viable Project

The combination of modern compute, good analysis tools (Python + librosa + numpy), and a methodical black-box testing approach means someone with strong DSP knowledge could make serious progress in months rather than years. The V-Synth is old enough that a determined developer with the right skills and access to the hardware could produce something genuinely compelling.

You said: Could Claude Code do this type of coding and testing using test files generated from an actual V-synth?

## Apr 3

Claude responded: Yes — Claude Code is actually quite well suited for this type of project.

Here's a realistic picture of what it could and couldn't do:

## What Claude Code Could Do Well

Signal analysis pipeline

•Write the entire Python analysis stack — librosa, scipy, numpy, matplotlib — for spectrogram comparison, LPC analysis, cepstral analysis, null testing, group delay measurement

•Iterate on that code rapidly based on what the analysis reveals

•Generate visualizations of formant trajectories, spectral differences, artifact signatures

Algorithm implementation

•Implement candidate algorithms — phase vocoder, sinusoidal modeling, source-filter models, various time-stretching approaches

•Write these in Python first for rapid prototyping, then C++ for a real VST

•Refactor and restructure as the hypothesis evolves

The iterative test loop

•Given a folder of V-Synth recordings and your plugin's output, it could automate the entire comparison pipeline

•Run batch null tests across all test files, summarize where differences are largest

•This is exactly the kind of systematic, repeatable task Claude Code excels at

JUCE/VST boilerplate

•Set up the plugin architecture, parameter handling, preset system, UI scaffolding

•The non-algorithmic parts of VST development that are tedious but well-defined

## The Collaboration Model That Would Work Best

The most effective approach wouldn't be "Claude Code do everything" — it would be a tight loop between you and Claude Code where:

•You capture test recordings from the V-Synth, listen critically, and make judgment calls about what artifacts matter

•Claude Code writes the analysis tools, implements the algorithms, runs the comparisons, and proposes next steps based on the data

•You interpret the musical/perceptual meaning of what the analysis shows

•Claude Code translates that back into algorithmic adjustments

The perceptual judgment layer — "this sounds metallic in a bad way" or "the formants feel slightly wrong on vowel transitions" — genuinely needs a human. Claude Code can measure differences but can't reliably evaluate whether a given artifact is musically significant.

## Realistic Workflow

  

V-Synth recordings (WAV files)

## ↓

Claude Code: analysis pipeline

## ↓

Spectrogram / LPC / null test reports

## ↓

You: perceptual evaluation + hypothesis

## ↓

Claude Code: implement revised algorithm

## ↓

Plugin output WAVs

## ↓

Claude Code: automated comparison vs V-Synth

## ↓

repeat

Claude Code could own every box except the perceptual evaluation step, and could maintain the entire codebase across sessions if you keep a well-structured project with good documentation — which it can also help write.

## Practical Limitations

Context and memory — Claude Code has no memory between sessions by default. For a long project like this you'd want a ARCHITECTURE.md and RESEARCH_LOG.md that gets fed back in at the start of each session, summarizing what's been tried and what the current hypothesis is. Claude Code can help maintain these documents.

DSP depth — Claude Code knows DSP theory well and can implement standard algorithms correctly. Where it may need guidance is in genuinely novel algorithmic territory — if the right answer turns out to be something non-standard, you may need to bring in a DSP specialist to unstick it.

C++ / JUCE complexity — it can write JUCE code but complex real-time audio C++ with
careful memory management will need review. The analysis/prototyping phase in Python would be smoother.

It can't listen — all perceptual evaluation has to be you. It can tell you the null test residual is -42dB at 2-4kHz but it can't tell you if that matters musically.

## Bottom Line

For a project like this, Claude Code would function as a highly capable DSP engineering
collaborator — one that can implement, test, measure, and iterate quickly, but needs a human in the loop for the perceptual and musical judgment calls. The analysis and automation side is where it would genuinely accelerate the work, potentially compressing months of tedious measurement work into weeks.

It's a legitimate use case and honestly a pretty exciting one — the kind of long-running, iterative, measurement-driven project that suits Claude Code's strengths well.

Recommended apps and extensions

## Claude Code

Build, debug, and ship from your terminal.

Claude Code for VS Code

Ask Claude to explain, edit, or fix code right in VS Code.

If you're serious about pursuing this project, Claude Code via the terminal or VS Code extension would be the right environment — it can maintain context across your entire codebase, run the analysis scripts directly, and iterate on the algorithm without you having to copy-paste code back and forth.

You said: if I have claude code integrated into VScode, as well as a JUCE framework to build the plugin - what would be the best way to begin this project if I have claude code integrated into VScode, as well as a JUCE framework to build the plugin - what would be the best way to begin this project

## Apr 13

Claude responded: Great starting point.

Great starting point. Here's how I'd structure the kickoff:

Phase 1: Project Foundation First (Before Any DSP)

The temptation is to dive into algorithms immediately — resist it. The scaffolding you set up now will determine how smoothly Claude Code can help you throughout the project.


Start by having Claude Code generate:

vsynth-emu/

├── ARCHITECTURE.md # living document of design decisions

├── RESEARCH_LOG.md # what's been tried, what the results were

├── analysis/ # Python analysis pipeline (separate from plugin)

│ ├── compare.py # null testing, spectrogram diff

│ ├── lpc.py # formant extraction

│ ├── batch_test.py # automated comparison runner

│ └── test_files/ # your V-Synth WAV recordings go here

├── plugin/ # JUCE project lives here

│ ├── CMakeLists.txt

## │ ├── Source/

│ │ ├── PluginProcessor.cpp

│ │ ├── VariphraseEngine.cpp # isolated, testable

## │ │ └── ...

└── research/ # notes, papers, spectrogram images

The key architectural decision is keeping the VariPhrase engine as a completely isolated, independently testable module from day one. This lets Python call into it for testing without the full VST host overhead.

Phase 2: Build the Analysis Pipeline Before the Plugin

Before writing a single line of DSP, you want your measurement tools working. Have Claude Code build:

compare.py — given two WAV files, outputs:

•Side-by-side spectrograms

•Null test residual with dB level

•Formant trajectory comparison via LPC

•A single "similarity score" you can track over time

batch_test.py — runs compare.py across your entire test file folder and produces a summary report. This becomes your automated regression test suite — every time you change the algorithm, you run this and see if things got better or worse across all test cases.

Get this working and validated before you record your V-Synth test files, so you know the tooling is solid.

Phase 3: Record Your V-Synth Test Battery Systematically

With your analysis tools ready, record deliberately:

test_files/

├── sustained/

│ ├── sine_440_pitchup_major3rd.wav

│ ├── sine_440_pitchdown_octave.wav

│ ├── sawtooth_220_stretch_2x.wav

│ └── vocal_aah_formant_shift_up.wav

├── transients/

│ ├── drum_hit_stretch_2x.wav

│ ├── drum_hit_stretch_4x.wav

│ └── pluck_stretch_halfspeed.wav

├── polyphonic/

│ ├── piano_chord_stretch_2x.wav

│ └── two_sines_pitchup.wav

└── edge_cases/

├── whisper_pitchshift.wav

└── noise_stretch.wav

Document the exact V-Synth settings used for each recording — patch, VariPhrase parameters, input level. This metadata is critical so results are reproducible.

Phase 4: Minimal JUCE Shell

Now build the plugin — but keep it minimal at first. Have Claude Code generate a JUCE plugin that:

•Accepts audio input

•Has placeholder parameters matching V-Synth's VariPhrase controls (pitch shift, time stretch, formant shift)

•Routes audio through a stub VariphraseEngine that initially just passes audio through unchanged

•Can render audio offline to a WAV file for testing (this is crucial — you don't want to rely on real-time playback for measurement)

The offline render capability means you can close the loop: feed test audio in, get WAV out, pipe it into batch_test.py automatically.

How to Work With Claude Code Day to Day

A few habits that will make the collaboration much more effective:

Start every session by feeding it context:

"Read ARCHITECTURE.md and RESEARCH_LOG.md before we begin.

We're building a VariPhrase emulator. Last session we found that our phase vocoder has excessive smearing on transients — today we're trying a sinusoidal + residual model instead."

End every session by updating the log: Ask Claude Code to append a summary to

RESEARCH_LOG.md — what was tried, what the batch test scores were, what the next hypothesis is. This becomes invaluable after 20+ sessions.

Let it write the tests, not just the code: For every algorithm Claude Code implements, ask it to also write the corresponding test in batch_test.py. The test suite becomes your safety net.

## The First Concrete Thing To Do

Open VS Code, start a Claude Code session, and give it this prompt:

"I'm starting a project to reverse-engineer Roland's VariPhrase algorithm by black-box testing against a real V-Synth. Set up the project structure above, create a JUCE CMakeLists.txt for a basic audio effect plugin, and build a Python analysis script that takes two WAV files and outputs a null test result, a spectrogram comparison image, and an LPC formant comparison. Use librosa and scipy."

That one prompt gives you the entire foundation and you'll have something real to work with in the first session.

You said: would sonnet or opus be the better model for this  project?

## Apr 20

Claude responded: For this specific project, the answer shifts depending on the phase you're in:

For this specific project, the answer shifts depending on the phase you're in:

Sonnet for the Majority of the Work

Most of what this project involves — writing Python analysis scripts, implementing JUCE boilerplate, running batch tests, refactoring code, maintaining the research log — is well within

Sonnet's capabilities. Claude Code defaults to Sonnet for good reason: it's faster and cheaper per token, which matters a lot in an agentic coding context where it might make dozens of file edits and tool calls in a single session. You don't want to burn Opus tokens on writing a

CMakeLists.txt.

Opus for Specific High-Value Moments

Where Opus earns its cost in this project is the hard reasoning steps — the moments where you're staring at a null test result and trying to figure out what it's telling you about Roland's algorithm. Specifically:

•Interpreting ambiguous analysis results — "here's our spectrogram diff and LPC comparison after 3 iterations, what does this pattern suggest about their source-filter model?"

•Algorithmic dead ends — when Sonnet has tried a few approaches and none are converging, Opus is better at stepping back and reconsidering the hypothesis

•Architecture decisions — the early decisions about how to structure VariphraseEngine that will be painful to undo later

•Reading DSP literature — if you feed it academic papers on phase vocoders or sinusoidal modeling and ask it to propose an implementation strategy

## Practical Approach

A good rhythm would be:

Opus → session planning, hypothesis formation, interpreting results, major architecture decisions

Sonnet → everything else (which is 90% of the work)

You can switch models mid-project without any issue — they'll both read your ARCHITECTURE.md and RESEARCH_LOG.md equally well. The living documentation you maintain is what provides continuity, not the model choice.

## One Caveat

Claude Code in VS Code uses Sonnet by default and model switching depends on your plan and how you've configured it. Worth checking your current setup — if you're on a Pro plan you have access to both, and you can select Opus for specific conversations when you hit one of those high-reasoning moments.

The honest answer is that Sonnet will surprise you with how far it gets on the DSP work. Opus is the escalation path, not the default.  
  

