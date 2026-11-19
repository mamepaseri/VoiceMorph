# VoiceMorph — JUCE VST3 Prototype 0.8.0

A Windows x64 VST3 / Standalone source prototype for non-AI voice transformation. The product goal is RoVee-like pitch/formant control with substantially better handling of breaths, sibilants, consonants, and low-register speech.

This project uses only public parameter concepts and original implementations. It does **not** reuse any vendor binary code, extracted resources, artwork, or proprietary DSP implementation from the supplied plug-ins.

## Current status — Prototype 0.8.0

### Implemented in the prototype shell
- JUCE VST3 + Standalone project structure.
- Dark graphite UI with cool cyan accents; no orange/yellow.
- Large rotary Pitch/Formant controls with dark metal-ring/cyan-arc treatment.
- Input and Output gain faders grouped on the left.
- Central real-time input/output waveform monitor.
- Tracking, Breath Preserve, Voice Mode, Consonant Preserve and Transient Preserve controls (Tracking changes F0 / voicing smoothing speed; Breath / Consonant / Transient have first-pass split-gate behaviour; Voice Mode remains a UI control).
- A/B state snapshots, bypass and preset selection.
- Basic frequency-band de-esser utility (an original first-pass helper, not the T-De-Esser 2 algorithm).
- Parameter state save/restore and host automation IDs.

### Streaming speech analysis and hybrid path
- **SpeechFeatureAnalyzer** (`Source/Dsp/SpeechFeatureAnalyzer.*`): a fixed-memory, no-allocation streaming analyzer called from the audio callback. It updates about every 10 ms and estimates F0 (65–450 Hz), periodicity, voiced confidence, aperiodic/high-band energy, RMS and a transient indicator.
- The UI status readout now displays the analyzer's estimated F0/voicing or an aperiodic indication. These are analysis readings, not a processed pitch output.
- **AdaptivePeriodicSplitter** (`Source/Dsp/AdaptivePeriodicSplitter.*`): a one-period predictive splitter builds a periodic candidate and an exact residual (`input - periodic`) using fractional-delay interpolation. A 12 ms local one-period correlation gate and sample-domain transient gate reduce stale-F0 leakage. The main hybrid path uses this splitter before the optional pitch/formant stage; a second instance feeds the UI split meters.
- **LatencyAlignedRecombiner** (`Source/Dsp/LatencyAlignedRecombiner.*`): a recombination helper delays the preserved residual and dry reference by the transform engine's reported latency, then blends the transformed-voice + residual path against the delayed dry reference. It is used by the JUCE processor through `HybridVoicePath`; the reported latency still needs runtime verification.
- The analyzer now clears stale F0 after a voiced-to-silence transition instead of publishing tiny positive values such as 1–2 Hz. Tracking controls the smoothing response speed: lower settings favour stability, higher settings follow pitch / voicing transitions faster. A 132 → 220 Hz synthetic step reaches the target range in about 180 ms at 0% and 60 ms at 100% at 48 kHz.
- A standalone CMake test target is in `tests/`. Synthetic 68 / 82 / 132 / 176 Hz harmonic signals, a quiet -60 dBFS-class tone, noise-like frication, silence, a voiced-to-silence transition, stale-F0 noise rejection, a transient impulse, sample-accurate recombination, a narrow PSOLA pitch-up case, stereo hybrid-path reconstruction, variable block sizes, transform-failure fallback, in-place operation, and optional-backend status are covered; sample-rate paths at 44.1 / 96 / 192 kHz are included. All 70 current assertions pass in the Linux build environment.
- The JUCE `processBlock()` now calls `SignalsmithHybridVoiceEngine`: it analyzes the input, creates periodic/residual candidates, sends only the periodic candidate to the optional Signalsmith pitch/formant engine, and aligns the residual/dry paths to the engine-reported latency before recombination. The processor reports that latency to the host with `setLatencySamples()`. If Signalsmith is not compiled, bypass is enabled, or a transform call fails, the engine selects a latency-aligned dry fallback. The portable tests validate the hybrid contract and fallback, but the real Signalsmith backend and JUCE audio callback have not been built or runtime-tested in this workspace.

### Open-source engine integrations retained
- **Signalsmith Stretch adapter** (`Source/Dsp/SignalsmithPitchEngine.*`): optional MIT-licensed dependency. The 48 ms block / 12 ms interval setup is a configuration candidate, not a measured latency claim. The adapter exposes input, output, and summed latency and is called only on the periodic candidate through `SignalsmithHybridVoiceEngine`; original breath/frication/transient detail remains on the residual path. Runtime behaviour still needs a full build and listening tests.
- **WORLD analyzer** (`Source/Dsp/WorldSpeechAnalyzer.*`): optional modified-BSD dependency. It returns F0, spectral-envelope, and aperiodicity tracks from an analysis window. It is a worker-thread utility, not a function to call from `processBlock()`.
- `ENGINE_ARCHITECTURE.md` describes how these parts will support a voiced / aperiodic / transient hybrid pipeline.
- `THIRD_PARTY_NOTICES.md` records licenses and integration conditions.

### Not implemented yet — important
- The processor source is connected to the optional Signalsmith pitch/formant engine, but the dependency has not been built in this workspace. Consequently there is no verified pitch/formant sound, latency, CPU profile, or host playback result yet.
- The new real-time analyzer is a first-pass detector. Two uploaded MP3 recordings have been used as an unlabeled stress-check corpus, but the source recording path is not confirmed as dry microphone audio and there is no hand-labelled F0 ground truth.
- The periodic/residual split is only a first-order predictive decomposition with local correlation and transient gates, not reliable source separation. The JUCE callback integration exists, but the optional Signalsmith build and real-time stream alignment have not been verified. WORLD real-time scheduling and robust voiced/aperiodic/transient separation remain unimplemented.
- The Signalsmith adapter remains a reference candidate and is not compiled into the standalone test target with the third-party backend disabled. It has not been built or latency-measured in this workspace.
- The PSOLA experiment is not ready for downward shifts or fixed-latency hybrid recombination. See `docs/PSOLA_EXPERIMENTAL.md`.
- Pitch and Formant parameters are connected in source to the optional Signalsmith engine. Breath Preserve / Consonant Preserve / Transient Preserve currently control the first-pass split gates; they still need perceptual tuning on labelled dry speech. Tracking controls the analyzer response speed as a first-pass behaviour; Voice Mode remains a UI control without its final dedicated DSP behaviour.

This is not a ready-to-install VST3 binary. It is an expanded source / DSP integration prototype.

## Windows installer

The **Package Windows installer** GitHub Actions workflow packages the successful `VoiceMorph-VST3-ON` artifact from the Windows VST3 build. It creates `VoiceMorph-Setup-x64-0.8.0.exe` (the version follows `CMakeLists.txt`) and uploads it as a versioned Actions artifact for 90 days. The workflow runs automatically after a successful build on `main`; it can also be run manually with the successful build run ID.

Download the EXE from the packaging workflow's **Artifacts** section. Run it, then choose per-user installation (the default, no administrator rights) or all-users installation (requires elevation). The installer places the `VoiceMorph.vst3` bundle in the matching standard VST3 folder and registers an uninstaller under Windows Apps & Features. Close your DAW before installing or uninstalling, then rescan its VST3 plug-ins.

The installer is currently unsigned. Windows SmartScreen may show an unfamiliar-app warning. No code-signing certificate is configured, and the installer does not claim to be signed. The project has no GitHub Release process yet, so this build is available as an Actions artifact rather than a release download.

**Prototype and licensing limitations:** this packages the existing x64 Signalsmith-enabled prototype; it does not imply that pitch/formant quality, host behavior, or latency have passed listening/runtime validation. The project's JUCE licensing route is still undecided; see [LICENSING.md](LICENSING.md). Resolve the applicable JUCE terms before redistributing the installer.

## Build on Windows 11

Requirements:
- Visual Studio 2022 with the Desktop development with C++ workload
- CMake 3.24 or later
- Git
- Internet access for JUCE and the optional DSP dependencies

The default build fetches JUCE 8.0.6 and Signalsmith Stretch using CMake FetchContent. Signalsmith is pinned to commit `57b93f4e9206a089a45387eaa39bdc9f310d3308` so the DSP source is reproducible. WORLD is an optional research analyzer and defaults OFF because it is not in the live audio path. To build without fetching Signalsmith, set `VOICEMORPH_ENABLE_SIGNALSMITH=OFF`; the plug-in then uses a latency-free dry fallback for the voice engine.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOICEMORPH_ENABLE_SIGNALSMITH=ON -DVOICEMORPH_ENABLE_WORLD=OFF
cmake --build build --config Release --target VoiceMorph_VST3
```

Build without the optional Signalsmith DSP (JUCE still has to be fetched):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOICEMORPH_ENABLE_SIGNALSMITH=OFF -DVOICEMORPH_ENABLE_WORLD=OFF
cmake --build build --config Release --target VoiceMorph_VST3
```

The VST3 bundle should be under `build/VoiceMorph_artefacts/Release/VST3/` (the exact path can vary by generator/platform). Rescan plug-ins in Cakewalk Sonar after copying the bundle into the user's VST3 folder.

## Design / parameter map

| VoiceMorph control | Reference concept |
|---|---|
| Voice Mix | RoVee Mix / THE_PITCH 2 MINI Dry-Wet concept |
| Pitch | RoVee Pitch / GForm Pitch / Signalsmith transpose baseline |
| Formant | RoVee Formant / FormantShiftSemitones metadata / independent formant scale |
| Tracking / Voice Mode | Natural-vs-hard behavior and Live/Studio workflow concepts in THE_PITCH 2 MINI |
| Breath / Consonant / Transient Preserve | New controls targeted at the user's non-voiced artifact complaint |
| De-Esser | Original frequency-band detector using T-De-Esser 2 parameter concepts only |
| Input / Output | Gain staging, as in THE_PITCH 2 MINI and Cakewalk channel strips |

## Next implementation stages
1. Validate F0 / voicing and the periodicity gate on labelled dry UAC-232 recordings, including breathy vowels, voiced fricatives, /s/ /sh/ /h/ /k/ /t/, mouth clicks, and quiet phrase endings.
2. Replace the first-order predictor with a more robust causal voiced / aperiodic / transient estimator; measure leakage and reconstruction on real speech.
3. Build and benchmark Signalsmith Stretch on the periodic candidate only; verify pitch/formant quality, CPU spikes, and input/output latency at each sample rate.
4. Validate the integrated `processBlock()` path, PDC, and dry fallback in Cakewalk Sonar / UAC-232, then tune the splitter against labelled dry Japanese speech.
5. Compare against RoVee, THE_PITCH 2 MINI, Flayr Tune, Vocal Bender, Formant, and T-De-Esser 2 using a matched vocal test corpus and blind listening.

See `ENGINE_ARCHITECTURE.md`, `THIRD_PARTY_NOTICES.md`, `LICENSING.md`, `REFERENCE_ANALYSIS.md`, and `BUILD_VALIDATION.md` before changing the DSP stack. The current environment cannot resolve GitHub from the build container, so a full JUCE/third-party compile and real audio test have not yet been verified. GitHub Actions workflows are included for the portable DSP suite and a Windows VST3 hybrid prototype build once the repository is pushed.

## Standalone analyzer tests

These tests do not require JUCE or external downloads:

```powershell
cmake -S tests -B out/tests -DCMAKE_BUILD_TYPE=Release
cmake --build out/tests --config Release
ctest --test-dir out/tests -C Release --output-on-failure
```
