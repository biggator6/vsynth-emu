---
author: Mark Borrow
operator: Mark Borrow
---

What features would be required to emulate a Roland V-synth in a VST plugin

**[Apr 3]{.smallcaps}**

**[Claude responded: The Roland V-Synth (2003) was a groundbreaking synthesizer.]{.smallcaps}**

**[The Roland V-Synth (2003) was a groundbreaking synthesizer. Here\'s a breakdown of the core features needed to emulate it in a VST:]{.smallcaps}**

**[Sound Generation Engines]{.smallcaps}**

**[VariPhrase Technology]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ the V-Synth\'s signature feature. Requires:]{.smallcaps}**

- **[Time-stretching independent of pitch (and vice versa)]{.smallcaps}**

- **[Formant shifting independent of pitch]{.smallcaps}**

- **[Real-time audio manipulation of PCM samples without artifacts]{.smallcaps}**

- **[This is the hardest part to emulate faithfully ]{.smallcaps}[---]{.smallcaps}[ Roland\'s VariPhrase was proprietary DSP]{.smallcaps}**

**[Elastic Audio Synthesis (EAS)]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ the core synthesis engine:]{.smallcaps}**

- **[Two oscillators, each switchable between PCM sample playback, analog modeling oscillators, or external audio input]{.smallcaps}**

- **[Per-oscillator VariPhrase processing]{.smallcaps}**

**[COSM (Composite Object Sound Modeling)]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ Roland\'s physical modeling layer:]{.smallcaps}**

- **[Acoustic resonance modeling (guitar body, violin body, etc.)]{.smallcaps}**

- **[Pickup simulation]{.smallcaps}**

- **[Amp/speaker modeling]{.smallcaps}**

**[Modulation & Control]{.smallcaps}**

- **[Time-Variant Filter (TVF)]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ resonant multimode filter with envelope]{.smallcaps}**

- **[Time-Variant Amplifier (TVA)]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ amplitude envelope]{.smallcaps}**

- **[Two independent envelopes]{.smallcaps}[ (ADSR + more stages)]{.smallcaps}**

- **[Two LFOs]{.smallcaps}[ with syncable rates and multiple waveforms]{.smallcaps}**

- **[Step-time arpeggiator/phrase sequencer]{.smallcaps}[ with time-based modulation]{.smallcaps}**

- **[Matrix modulation]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ flexible routing of any source to any destination]{.smallcaps}**

**[Unique Interface Features]{.smallcaps}**

- **[D-Beam controller]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ infrared proximity sensor for gestural control (can be mapped to a mod wheel or expression input in a VST)]{.smallcaps}**

- **[Touch-sensitive ribbon controllers]{.smallcaps}[ (two) ]{.smallcaps}[---]{.smallcaps}[ map to pitch bend / filter / any parameter]{.smallcaps}**

- **[Knob/slider automation]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ all parameters fully automatable]{.smallcaps}**

**[Effects Chain]{.smallcaps}**

- **[Multi-effects processor (chorus, reverb, delay, distortion, EQ)]{.smallcaps}**

- **[Effects routed ]{.smallcaps}[per-oscillator]{.smallcaps}[ as well as globally]{.smallcaps}**

- **[MFX, chorus, reverb running in parallel]{.smallcaps}**

**[Polyphony & Voice Architecture]{.smallcaps}**

- **[Up to ]{.smallcaps}[20-voice polyphony]{.smallcaps}**

- **[Split/layer]{.smallcaps}[ modes for two independent patches]{.smallcaps}**

- **[Stereo voice output per partial]{.smallcaps}**

**[The Hard Parts (Technical Challenges)]{.smallcaps}**

|                                       |                                                                                                                         |
|---------------------------------------|-------------------------------------------------------------------------------------------------------------------------|
| **[Feature]{.smallcaps}**             | **[Challenge]{.smallcaps}**                                                                                             |
|                                       |                                                                                                                         |
| **[VariPhrase]{.smallcaps}**          | **[Requires high-quality phase vocoder or similar time-domain stretching]{.smallcaps}**                                 |
| **[COSM]{.smallcaps}**                | **[Complex modal/physical modeling ]{.smallcaps}[---]{.smallcaps}[ computationally expensive]{.smallcaps}**             |
| **[Ribbon controller]{.smallcaps}**   | **[Needs MPE or custom MIDI mapping in a VST context]{.smallcaps}**                                                     |
| **[D-Beam]{.smallcaps}**              | **[Can only be approximated via MIDI CC]{.smallcaps}**                                                                  |
|                                       |                                                                                                                         |
| **[Internal sample ROM]{.smallcaps}** | **[Licensing Roland\'s PCM content is not possible ]{.smallcaps}[---]{.smallcaps}[ custom samples needed]{.smallcaps}** |
|                                       |                                                                                                                         |

**[Recommended DSP Approaches for a VST Dev]{.smallcaps}**

- **[Time-stretching]{.smallcaps}[: ]{.smallcaps}[é]{.smallcaps}[lastique (zplane) or Rubber Band Library for VariPhrase-style processing]{.smallcaps}**

- **[Physical modeling]{.smallcaps}[: modal synthesis or waveguide synthesis for COSM]{.smallcaps}**

- **[Oscillators]{.smallcaps}[: bandlimited wavetable or BLEP/BLAMP for analog modeling]{.smallcaps}**

- **[Framework]{.smallcaps}[: JUCE is the standard VST framework that handles all the plugin scaffolding]{.smallcaps}**

**[The VariPhrase engine is really the soul of the V-Synth ]{.smallcaps}[---]{.smallcaps}[ a convincing emulation of that alone would be 80% of the battle. Several modern plugins (like VocalSynth 2 or Krotos Reformer) tackle adjacent territory but nothing has fully replicated the V-Synth\'s complete feature set.]{.smallcaps}**

**[You said: why is variphrase so difficult to replicate?]{.smallcaps}**

**[why is variphrase so difficult to replicate?]{.smallcaps}[ ]{.smallcaps}[ what differentiates it from regular pitch shifting?]{.smallcaps}**

**[Apr 3]{.smallcaps}**

**[Claude responded: Great question.]{.smallcaps}**

**[Great question. VariPhrase sits in a different category from regular pitch shifting ]{.smallcaps}[---]{.smallcaps}[ here\'s why:]{.smallcaps}**

**[What Regular Pitch Shifting Does]{.smallcaps}**

**[A standard pitch shifter does one thing: ]{.smallcaps}[resample the audio]{.smallcaps}[. Speed it up = higher pitch, slow it down = lower pitch. The relationship between time and pitch is locked together physically, just like playing a tape faster or slower.]{.smallcaps}**

**[Even more sophisticated \"pitch shift without time change\" algorithms (like those in most DAWs) work by chopping audio into small grains or windows and repositioning/overlapping them ]{.smallcaps}[---]{.smallcaps}[ but they\'re still fundamentally treating pitch, time, and formants as a ]{.smallcaps}[single bundled thing]{.smallcaps}[ that gets manipulated together, with artifacts being the cost of separating them.]{.smallcaps}**

**[What VariPhrase Does Differently]{.smallcaps}**

**[Roland\'s VariPhrase decouples ]{.smallcaps}[three independent axes]{.smallcaps}[ that are normally entangled:]{.smallcaps}**

|                                   |                                               |                                     |
|-----------------------------------|-----------------------------------------------|-------------------------------------|
| **[Parameter]{.smallcaps}**       | **[Normal Pitch Shift]{.smallcaps}**          | **[VariPhrase]{.smallcaps}**        |
|                                   |                                               |                                     |
| **[Pitch]{.smallcaps}**           | **[Changes together with speed]{.smallcaps}** | **[Fully independent]{.smallcaps}** |
| **[Time / Duration]{.smallcaps}** | **[Locked to pitch]{.smallcaps}**             | **[Fully independent]{.smallcaps}** |
|                                   |                                               |                                     |
| **[Formants]{.smallcaps}**        | **[Shift with pitch]{.smallcaps}**            | **[Fully independent]{.smallcaps}** |
|                                   |                                               |                                     |

**[This means you can:]{.smallcaps}**

- **[Raise the pitch of a vocal without it sounding like a chipmunk (formants stay put)]{.smallcaps}**

- **[Slow a phrase down without it dropping in pitch or getting warbly]{.smallcaps}**

- **[Shift formants without changing pitch at all (change a male voice to female timbre while singing the same note)]{.smallcaps}**

- **[Do all three simultaneously, in real time, with low latency]{.smallcaps}**

**[Why That\'s Hard]{.smallcaps}**

**[Formant preservation is the crux of it.]{.smallcaps}[ Human voice and many acoustic instruments have resonant peaks called formants that define the ]{.smallcaps}*[character]{.smallcaps}*[ of the sound ]{.smallcaps}[---]{.smallcaps}[ the vowel shape of a voice, the body resonance of a cello. When you pitch shift naively, formants shift with the pitch, which is what creates the \"chipmunk\" or \"demon voice\" effect.]{.smallcaps}**

**[To separate formants from pitch you need to:]{.smallcaps}**

1.  **[Analyze the spectral envelope]{.smallcaps}[ in real time ]{.smallcaps}[---]{.smallcaps}[ identify where the formant peaks are]{.smallcaps}**

2.  **[Extract the source excitation]{.smallcaps}[ (the pitched part) separately from the resonant filter shape]{.smallcaps}**

3.  **[Manipulate them independently]{.smallcaps}[, then resynthesize]{.smallcaps}**

**[This is essentially a ]{.smallcaps}[source-filter model]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ the same idea behind LPC (Linear Predictive Coding) used in vocoders ]{.smallcaps}[---]{.smallcaps}[ but Roland\'s implementation had to do this with very low latency, high quality, and in a way that worked on ]{.smallcaps}*[any]{.smallcaps}*[ audio input, not just voice.]{.smallcaps}**

**[The Phase Vocoder Problem]{.smallcaps}**

**[Most modern time-stretching uses a ]{.smallcaps}[phase vocoder]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ it converts audio to the frequency domain (via FFT), manipulates it, then converts back. Phase vocoders are good but have well-known failure modes:]{.smallcaps}**

- **[Phasiness / smearing]{.smallcaps}[ on transients]{.smallcaps}**

- **[Metallic artifacts]{.smallcaps}[ on polyphonic or complex material]{.smallcaps}**

- **[Phase coherence problems]{.smallcaps}[ between harmonics, causing a swirling, unnatural sound]{.smallcaps}**

**[Roland\'s VariPhrase was widely noted for sounding ]{.smallcaps}*[cleaner]{.smallcaps}*[ than contemporary phase vocoder approaches, suggesting they were doing something more sophisticated ]{.smallcaps}[---]{.smallcaps}[ likely a ]{.smallcaps}[hybrid approach]{.smallcaps}[ combining sinusoidal modeling, transient detection, and residual noise handling separately.]{.smallcaps}**

**[What Makes it Hard to Replicate Today]{.smallcaps}**

- **[Roland never published the algorithm ]{.smallcaps}[---]{.smallcaps}[ it\'s a ]{.smallcaps}[trade secret]{.smallcaps}**

- **[The clean real-time performance suggests custom DSP optimizations that aren\'t obvious from the output alone]{.smallcaps}**

- **[Modern libraries like Rubber Band or zplane ]{.smallcaps}[é]{.smallcaps}[lastique get close but have their own artifact signatures ]{.smallcaps}[---]{.smallcaps}[ they don\'t sound identical]{.smallcaps}**

- **[Reverse-engineering it requires having access to a V-Synth and doing ]{.smallcaps}[extensive blind testing]{.smallcaps}[ across edge cases (transients, polyphony, formant extremes)]{.smallcaps}**

**[The irony is that in 2025, the raw compute needed is trivial ]{.smallcaps}[---]{.smallcaps}[ the barrier is purely ]{.smallcaps}[algorithmic knowledge]{.smallcaps}[, not processing power. Roland essentially solved a hard DSP problem in 2003 and quietly kept the solution to themselves.]{.smallcaps}**

**[You said: if you had access to a V-synth to analyze output of a plug-in vs the real machine, would you be able to tailor an algorithm to get very close?]{.smallcaps}**

**[if you had access to a V-synth to analyze output of a plug-in vs the real machine, would you be able to tailor an algorithm to get very close?]{.smallcaps}**

**[Apr 3]{.smallcaps}**

**[Claude responded: Yes, in principle ]{.smallcaps}[---]{.smallcaps}[ this is essentially what blind reverse engineering via black-box testing looks like, and it\'s a legitimate and well-trodden approach.]{.smallcaps}**

**[Yes, in principle ]{.smallcaps}[---]{.smallcaps}[ this is essentially what ]{.smallcaps}[blind reverse engineering via black-box testing]{.smallcaps}[ looks like, and it\'s a legitimate and well-trodden approach. Here\'s how you\'d actually do it systematically:]{.smallcaps}**

**[The Core Methodology]{.smallcaps}**

**[You\'d treat the V-Synth as an ]{.smallcaps}[oracle]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ you feed it known inputs, capture known outputs, and use the difference between your plugin\'s output and the oracle\'s output as a gradient to improve your algorithm. This is the same fundamental approach used to emulate analog circuits (like what Cytomic did with their filter models).]{.smallcaps}**

**[Test Signal Battery]{.smallcaps}**

**[You\'d design a suite of highly controlled inputs that ]{.smallcaps}[stress-test specific aspects]{.smallcaps}[ of VariPhrase independently:]{.smallcaps}**

**[For pitch/formant separation:]{.smallcaps}**

- **[Sustained sine waves at known frequencies]{.smallcaps}**

- **[Sawtooth waves with known harmonic series]{.smallcaps}**

- **[Sung vowels held steady (\"aaaah\") ]{.smallcaps}[---]{.smallcaps}[ formants are well-defined and measurable]{.smallcaps}**

- **[Then shift pitch up/down by known intervals and compare formant positions in the output spectrogram]{.smallcaps}**

**[For time-stretching quality:]{.smallcaps}**

- **[A single clean transient (click or drum hit) ]{.smallcaps}[---]{.smallcaps}[ stretched 2x, 4x, 0.5x]{.smallcaps}**

- **[Measure smearing, pre-ringing, artifact frequency content]{.smallcaps}**

- **[This reveals a lot about whether they\'re using windowed FFT, granular, or something else]{.smallcaps}**

**[For the source-filter model:]{.smallcaps}**

- **[Whispered speech (no pitch, pure formant/noise) vs. voiced speech]{.smallcaps}**

- **[See how VariPhrase handles unpitched material]{.smallcaps}**

- **[A glottal pulse approximation (very buzzy sawtooth into the input)]{.smallcaps}**

**[For polyphonic handling:]{.smallcaps}**

- **[Two simultaneous sine waves ]{.smallcaps}[---]{.smallcaps}[ does the algorithm track them independently or treat them as one signal?]{.smallcaps}**

- **[Piano chord stretches ]{.smallcaps}[---]{.smallcaps}[ where phase vocoders typically fall apart]{.smallcaps}**

**[Analysis Toolkit]{.smallcaps}**

**[With both signals captured (V-Synth output vs. your plugin output):]{.smallcaps}**

- **[Spectrogram comparison]{.smallcaps}[ (high resolution STFT) ]{.smallcaps}[---]{.smallcaps}[ reveals formant trajectories, artifact smearing, phase behavior]{.smallcaps}**

- **[LPC analysis]{.smallcaps}[ on both ]{.smallcaps}[---]{.smallcaps}[ directly extracts and compares the formant filter model]{.smallcaps}**

- **[Cepstral analysis]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ separates the spectral envelope (formants) from the fine structure (pitch) mathematically]{.smallcaps}**

- **[Null testing]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ phase-invert one signal and sum with the other; what remains is ]{.smallcaps}*[only]{.smallcaps}*[ the difference, making subtle artifacts clearly audible and visible]{.smallcaps}**

- **[Group delay analysis]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ reveals latency and phase behavior of the algorithm]{.smallcaps}**

**[What You Could Actually Deduce]{.smallcaps}**

|                                                        |                                                                   |
|--------------------------------------------------------|-------------------------------------------------------------------|
| **[Observable]{.smallcaps}**                           | **[What It Tells You]{.smallcaps}**                               |
|                                                        |                                                                   |
| **[Formant stability during pitch shift]{.smallcaps}** | **[Quality of spectral envelope estimation]{.smallcaps}**         |
| **[Transient smearing amount]{.smallcaps}**            | **[Window size and overlap in time-freq transform]{.smallcaps}**  |
| **[Artifact frequency content]{.smallcaps}**           | **[Whether they use sinusoidal + residual modeling]{.smallcaps}** |
| **[Polyphonic behavior]{.smallcaps}**                  | **[Whether pitch tracking is per-partial or global]{.smallcaps}** |
| **[Latency floor]{.smallcaps}**                        | **[Minimum FFT window size they\'re committed to]{.smallcaps}**   |
|                                                        |                                                                   |
| **[Noise floor artifacts]{.smallcaps}**                | **[How they handle the residual/noise component]{.smallcaps}**    |
|                                                        |                                                                   |

**[The Iterative Loop]{.smallcaps}**

**[You\'d essentially be doing ]{.smallcaps}[gradient descent by ear and measurement]{.smallcaps}[:]{.smallcaps}**

1.  **[Hypothesis: \"they\'re using a sinusoidal model with residual noise handling\"]{.smallcaps}**

2.  **[Implement it]{.smallcaps}**

3.  **[Null test against V-Synth output]{.smallcaps}**

4.  **[The residual reveals what your model is missing]{.smallcaps}**

5.  **[Refine hypothesis, repeat]{.smallcaps}**

**[Over many iterations this converges. It\'s slow but it works ]{.smallcaps}[---]{.smallcaps}[ this is roughly how Antares Auto-Tune, Melodyne, and zplane\'s algorithms were refined over years.]{.smallcaps}**

**[The Honest Ceiling]{.smallcaps}**

**[You\'d likely get ]{.smallcaps}[very close but not identical]{.smallcaps}[, for a few reasons:]{.smallcaps}**

- **[If Roland is using a ]{.smallcaps}[proprietary basis function]{.smallcaps}[ or a non-standard transform, you can observe its behavior but can\'t know the exact implementation ]{.smallcaps}[---]{.smallcaps}[ two different algorithms can produce perceptually identical output while being architecturally different]{.smallcaps}**

- **[Some decisions may be ]{.smallcaps}[non-deterministic]{.smallcaps}[ or depend on internal state in ways that are hard to fully characterize from output alone]{.smallcaps}**

- **[The ]{.smallcaps}[edge cases]{.smallcaps}[ (extreme stretch ratios, unusual timbres) would be the last 5% that\'s hardest to nail]{.smallcaps}**

**[But \"very close\" in this context means perceptually indistinguishable on most real-world material ]{.smallcaps}[---]{.smallcaps}[ which for a VST plugin is absolutely good enough, and arguably the right goal. Nobody\'s going to null-test your plugin against a V-Synth in a mix.]{.smallcaps}**

**[This Is Actually a Viable Project]{.smallcaps}**

**[The combination of modern compute, good analysis tools (Python + librosa + numpy), and a methodical black-box testing approach means someone with strong DSP knowledge could make serious progress in months rather than years. The V-Synth is old enough that a determined developer with the right skills and access to the hardware could produce something genuinely compelling.]{.smallcaps}**

**[You said: Could Claude Code do this type of coding and testing using test files generated from an actual V-synth?]{.smallcaps}**

**[Could Claude Code do this type of coding and testing using test files generated from an actual V-synth?]{.smallcaps}**

**[Apr 3]{.smallcaps}**

**[Claude responded: Yes ]{.smallcaps}[---]{.smallcaps}[ Claude Code is actually quite well suited for this type of project.]{.smallcaps}**

**[Yes ]{.smallcaps}[---]{.smallcaps}[ Claude Code is actually quite well suited for this type of project. Here\'s a realistic picture of what it could and couldn\'t do:]{.smallcaps}**

**[What Claude Code Could Do Well]{.smallcaps}**

**[Signal analysis pipeline]{.smallcaps}**

- **[Write the entire Python analysis stack ]{.smallcaps}[---]{.smallcaps}[ librosa, scipy, numpy, matplotlib ]{.smallcaps}[---]{.smallcaps}[ for spectrogram comparison, LPC analysis, cepstral analysis, null testing, group delay measurement]{.smallcaps}**

- **[Iterate on that code rapidly based on what the analysis reveals]{.smallcaps}**

- **[Generate visualizations of formant trajectories, spectral differences, artifact signatures]{.smallcaps}**

**[Algorithm implementation]{.smallcaps}**

- **[Implement candidate algorithms ]{.smallcaps}[---]{.smallcaps}[ phase vocoder, sinusoidal modeling, source-filter models, various time-stretching approaches]{.smallcaps}**

- **[Write these in Python first for rapid prototyping, then C++ for a real VST]{.smallcaps}**

- **[Refactor and restructure as the hypothesis evolves]{.smallcaps}**

**[The iterative test loop]{.smallcaps}**

- **[Given a folder of V-Synth recordings and your plugin\'s output, it could automate the entire comparison pipeline]{.smallcaps}**

- **[Run batch null tests across all test files, summarize where differences are largest]{.smallcaps}**

- **[This is exactly the kind of systematic, repeatable task Claude Code excels at]{.smallcaps}**

**[JUCE/VST boilerplate]{.smallcaps}**

- **[Set up the plugin architecture, parameter handling, preset system, UI scaffolding]{.smallcaps}**

- **[The non-algorithmic parts of VST development that are tedious but well-defined]{.smallcaps}**

**[The Collaboration Model That Would Work Best]{.smallcaps}**

**[The most effective approach wouldn\'t be \"Claude Code do everything\" ]{.smallcaps}[---]{.smallcaps}[ it would be a tight loop between you and Claude Code where:]{.smallcaps}**

- **[You]{.smallcaps}[ capture test recordings from the V-Synth, listen critically, and make judgment calls about what artifacts matter]{.smallcaps}**

- **[Claude Code]{.smallcaps}[ writes the analysis tools, implements the algorithms, runs the comparisons, and proposes next steps based on the data]{.smallcaps}**

- **[You]{.smallcaps}[ interpret the musical/perceptual meaning of what the analysis shows]{.smallcaps}**

- **[Claude Code]{.smallcaps}[ translates that back into algorithmic adjustments]{.smallcaps}**

**[The perceptual judgment layer ]{.smallcaps}[---]{.smallcaps}[ \"this sounds metallic in a bad way\" or \"the formants feel slightly wrong on vowel transitions\" ]{.smallcaps}[---]{.smallcaps}[ genuinely needs a human. Claude Code can measure differences but can\'t reliably evaluate whether a given artifact is musically significant.]{.smallcaps}**

**[Realistic Workflow]{.smallcaps}**

**[V-Synth recordings (WAV files)]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[Claude Code: analysis pipeline]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[Spectrogram / LPC / null test reports]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[You: perceptual evaluation + hypothesis]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[Claude Code: implement revised algorithm]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[Plugin output WAVs]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[Claude Code: automated comparison vs V-Synth]{.smallcaps}**

**[ ]{.smallcaps}[↓]{.smallcaps}**

**[repeat]{.smallcaps}**

**[Claude Code could own every box except the perceptual evaluation step, and could maintain the entire codebase across sessions if you keep a well-structured project with good documentation ]{.smallcaps}[---]{.smallcaps}[ which it can also help write.]{.smallcaps}**

**[Practical Limitations]{.smallcaps}**

**[Context and memory]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ Claude Code has no memory between sessions by default. For a long project like this you\'d want a ]{.smallcaps}[`ARCHITECTURE.md`]{.smallcaps}[ and ]{.smallcaps}[`RESEARCH_LOG.md`]{.smallcaps}[ that gets fed back in at the start of each session, summarizing what\'s been tried and what the current hypothesis is. Claude Code can help maintain these documents.]{.smallcaps}**

**[DSP depth]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ Claude Code knows DSP theory well and can implement standard algorithms correctly. Where it may need guidance is in genuinely novel algorithmic territory ]{.smallcaps}[---]{.smallcaps}[ if the right answer turns out to be something non-standard, you may need to bring in a DSP specialist to unstick it.]{.smallcaps}**

**[C++ / JUCE complexity]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ it can write JUCE code but complex real-time audio C++ with careful memory management will need review. The analysis/prototyping phase in Python would be smoother.]{.smallcaps}**

**[It can\'t listen]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ all perceptual evaluation has to be you. It can tell you the null test residual is -42dB at 2-4kHz but it can\'t tell you if that matters musically.]{.smallcaps}**

**[Bottom Line]{.smallcaps}**

**[For a project like this, Claude Code would function as a highly capable ]{.smallcaps}[DSP engineering collaborator]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ one that can implement, test, measure, and iterate quickly, but needs a human in the loop for the perceptual and musical judgment calls. The analysis and automation side is where it would genuinely accelerate the work, potentially compressing months of tedious measurement work into weeks.]{.smallcaps}**

**[It\'s a legitimate use case and honestly a pretty exciting one ]{.smallcaps}[---]{.smallcaps}[ the kind of long-running, iterative, measurement-driven project that suits Claude Code\'s strengths well.]{.smallcaps}**

**[Recommended apps and extensions]{.smallcaps}**

**[Claude Code]{.smallcaps}**

**[Build, debug, and ship from your terminal.]{.smallcaps}**

**[Claude Code for VS Code]{.smallcaps}**

**[Ask Claude to explain, edit, or fix code right in VS Code.]{.smallcaps}**

**[If you\'re serious about pursuing this project, Claude Code via the terminal or VS Code extension would be the right environment ]{.smallcaps}[---]{.smallcaps}[ it can maintain context across your entire codebase, run the analysis scripts directly, and iterate on the algorithm without you having to copy-paste code back and forth.]{.smallcaps}**

**[You said: if I have claude code integrated into VScode, as well as a JUCE framework to build the plugin - what would be the best way to begin this project]{.smallcaps}**

**[if I have claude code integrated into VScode, as well as a JUCE framework to build the plugin - what would be the best way to begin this project]{.smallcaps}**

**[Apr 13]{.smallcaps}**

**[Claude responded: Great starting point.]{.smallcaps}**

**[Great starting point. Here\'s how I\'d structure the kickoff:]{.smallcaps}**

**[Phase 1: Project Foundation First (Before Any DSP)]{.smallcaps}**

**[The temptation is to dive into algorithms immediately ]{.smallcaps}[---]{.smallcaps}[ resist it. The scaffolding you set up now will determine how smoothly Claude Code can help you throughout the project.]{.smallcaps}**

**[Start by having Claude Code generate:]{.smallcaps}**

**[vsynth-emu/]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ ARCHITECTURE.md \# living document of design decisions]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ RESEARCH_LOG.md \# what\'s been tried, what the results were]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ analysis/ \# Python analysis pipeline (separate from plugin)]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ compare.py \# null testing, spectrogram diff]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ lpc.py \# formant extraction]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ batch_test.py \# automated comparison runner]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ test_files/ \# your V-Synth WAV recordings go here]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ plugin/ \# JUCE project lives here]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ CMakeLists.txt]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ Source/]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ PluginProcessor.cpp]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ VariphraseEngine.cpp \# isolated, testable]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[│]{.smallcaps}[ ]{.smallcaps}[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ \...]{.smallcaps}**

**[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ research/ \# notes, papers, spectrogram images]{.smallcaps}**

**[The key architectural decision is keeping the ]{.smallcaps}[VariPhrase engine as a completely isolated, independently testable module]{.smallcaps}[ from day one. This lets Python call into it for testing without the full VST host overhead.]{.smallcaps}**

**[Phase 2: Build the Analysis Pipeline Before the Plugin]{.smallcaps}**

**[Before writing a single line of DSP, you want your measurement tools working. Have Claude Code build:]{.smallcaps}**

**[`compare.py`]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ given two WAV files, outputs:]{.smallcaps}**

- **[Side-by-side spectrograms]{.smallcaps}**

- **[Null test residual with dB level]{.smallcaps}**

- **[Formant trajectory comparison via LPC]{.smallcaps}**

- **[A single \"similarity score\" you can track over time]{.smallcaps}**

**[`batch_test.py`]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ runs compare.py across your entire test file folder and produces a summary report. This becomes your automated regression test suite ]{.smallcaps}[---]{.smallcaps}[ every time you change the algorithm, you run this and see if things got better or worse across all test cases.]{.smallcaps}**

**[Get this working and validated ]{.smallcaps}*[before]{.smallcaps}*[ you record your V-Synth test files, so you know the tooling is solid.]{.smallcaps}**

**[Phase 3: Record Your V-Synth Test Battery Systematically]{.smallcaps}**

**[With your analysis tools ready, record deliberately:]{.smallcaps}**

**[test_files/]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ sustained/]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ sine_440_pitchup_major3rd.wav]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ sine_440_pitchdown_octave.wav]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ sawtooth_220_stretch_2x.wav]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ vocal_aah_formant_shift_up.wav]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ transients/]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ drum_hit_stretch_2x.wav]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ drum_hit_stretch_4x.wav]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ pluck_stretch_halfspeed.wav]{.smallcaps}**

**[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ polyphonic/]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ piano_chord_stretch_2x.wav]{.smallcaps}**

**[│]{.smallcaps}[ ]{.smallcaps}[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ two_sines_pitchup.wav]{.smallcaps}**

**[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ edge_cases/]{.smallcaps}**

**[ ]{.smallcaps}[├]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ whisper_pitchshift.wav]{.smallcaps}**

**[ ]{.smallcaps}[└]{.smallcaps}[─]{.smallcaps}[─]{.smallcaps}[ noise_stretch.wav]{.smallcaps}**

**[Document the exact V-Synth settings used for each recording ]{.smallcaps}[---]{.smallcaps}[ patch, VariPhrase parameters, input level. This metadata is critical so results are reproducible.]{.smallcaps}**

**[Phase 4: Minimal JUCE Shell]{.smallcaps}**

**[Now build the plugin ]{.smallcaps}[---]{.smallcaps}[ but keep it minimal at first. Have Claude Code generate a JUCE plugin that:]{.smallcaps}**

- **[Accepts audio input]{.smallcaps}**

- **[Has placeholder parameters matching V-Synth\'s VariPhrase controls (pitch shift, time stretch, formant shift)]{.smallcaps}**

- **[Routes audio through a ]{.smallcaps}[stub VariphraseEngine]{.smallcaps}[ that initially just passes audio through unchanged]{.smallcaps}**

- **[Can render audio offline to a WAV file for testing (this is crucial ]{.smallcaps}[---]{.smallcaps}[ you don\'t want to rely on real-time playback for measurement)]{.smallcaps}**

**[The offline render capability means you can close the loop: feed test audio in, get WAV out, pipe it into batch_test.py automatically.]{.smallcaps}**

**[How to Work With Claude Code Day to Day]{.smallcaps}**

**[A few habits that will make the collaboration much more effective:]{.smallcaps}**

**[Start every session by feeding it context:]{.smallcaps}**

**[\"Read ARCHITECTURE.md and RESEARCH_LOG.md before we begin. ]{.smallcaps}**

**[We\'re building a VariPhrase emulator. Last session we found ]{.smallcaps}**

**[that our phase vocoder has excessive smearing on transients ]{.smallcaps}**

**[---]{.smallcaps}[ today we\'re trying a sinusoidal + residual model instead.\"]{.smallcaps}**

**[End every session by updating the log:]{.smallcaps}[ Ask Claude Code to append a summary to RESEARCH_LOG.md ]{.smallcaps}[---]{.smallcaps}[ what was tried, what the batch test scores were, what the next hypothesis is. This becomes invaluable after 20+ sessions.]{.smallcaps}**

**[Let it write the tests, not just the code:]{.smallcaps}[ For every algorithm Claude Code implements, ask it to also write the corresponding test in batch_test.py. The test suite becomes your safety net.]{.smallcaps}**

**[The First Concrete Thing To Do]{.smallcaps}**

**[Open VS Code, start a Claude Code session, and give it this prompt:]{.smallcaps}**

**[\"I\'m starting a project to reverse-engineer Roland\'s VariPhrase algorithm by black-box testing against a real V-Synth. Set up the project structure above, create a JUCE CMakeLists.txt for a basic audio effect plugin, and build a Python analysis script that takes two WAV files and outputs a null test result, a spectrogram comparison image, and an LPC formant comparison. Use librosa and scipy.\"]{.smallcaps}**

**[That one prompt gives you the entire foundation and you\'ll have something real to work with in the first session.]{.smallcaps}**

**[You said: would sonnet or opus be the better model for this project?]{.smallcaps}**

**[would sonnet or opus be the better model for this project?]{.smallcaps}**

**[Apr 20]{.smallcaps}**

**[Claude responded: For this specific project, the answer shifts depending on the phase you\'re in:]{.smallcaps}**

**[For this specific project, the answer shifts depending on the phase you\'re in:]{.smallcaps}**

**[Sonnet for the Majority of the Work]{.smallcaps}**

**[Most of what this project involves ]{.smallcaps}[---]{.smallcaps}[ writing Python analysis scripts, implementing JUCE boilerplate, running batch tests, refactoring code, maintaining the research log ]{.smallcaps}[---]{.smallcaps}[ is well within Sonnet\'s capabilities. Claude Code defaults to Sonnet for good reason: it\'s faster and cheaper per token, which matters a lot in an agentic coding context where it might make dozens of file edits and tool calls in a single session. You don\'t want to burn Opus tokens on writing a CMakeLists.txt.]{.smallcaps}**

**[Opus for Specific High-Value Moments]{.smallcaps}**

**[Where Opus earns its cost in this project is the ]{.smallcaps}[hard reasoning steps]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ the moments where you\'re staring at a null test result and trying to figure out what it\'s telling you about Roland\'s algorithm. Specifically:]{.smallcaps}**

- **[Interpreting ambiguous analysis results]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ \"here\'s our spectrogram diff and LPC comparison after 3 iterations, what does this pattern suggest about their source-filter model?\"]{.smallcaps}**

- **[Algorithmic dead ends]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ when Sonnet has tried a few approaches and none are converging, Opus is better at stepping back and reconsidering the hypothesis]{.smallcaps}**

- **[Architecture decisions]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ the early decisions about how to structure VariphraseEngine that will be painful to undo later]{.smallcaps}**

- **[Reading DSP literature]{.smallcaps}[ ]{.smallcaps}[---]{.smallcaps}[ if you feed it academic papers on phase vocoders or sinusoidal modeling and ask it to propose an implementation strategy]{.smallcaps}**

**[Practical Approach]{.smallcaps}**

**[A good rhythm would be:]{.smallcaps}**

**[Opus ]{.smallcaps}[→]{.smallcaps}[ session planning, hypothesis formation, ]{.smallcaps}**

**[ interpreting results, major architecture decisions]{.smallcaps}**

**[Sonnet ]{.smallcaps}[→]{.smallcaps}[ everything else (which is 90% of the work)]{.smallcaps}**

**[You can switch models mid-project without any issue ]{.smallcaps}[---]{.smallcaps}[ they\'ll both read your ARCHITECTURE.md and RESEARCH_LOG.md equally well. The living documentation you maintain is what provides continuity, not the model choice.]{.smallcaps}**

**[One Caveat]{.smallcaps}**

**[Claude Code in VS Code uses Sonnet by default and model switching depends on your plan and how you\'ve configured it. Worth checking your current setup ]{.smallcaps}[---]{.smallcaps}[ if you\'re on a Pro plan you have access to both, and you can select Opus for specific conversations when you hit one of those high-reasoning moments.]{.smallcaps}**

**[The honest answer is that Sonnet will surprise you with how far it gets on the DSP work. Opus is the escalation path, not the default.]{.smallcaps}**
