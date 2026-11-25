# Build / validation status — 2026-11-26 — Prototype 0.9.1

## 0.9.1 candidate

- Corrects the dashboard accent to orange and reduces the default editor resolution from 1220 × 810 to 1100 × 730. Existing resize limits remain 980 × 640 minimum and 1600 × 1060 maximum.
- This is based on 0.9.0 and does not change DSP behavior.
- Windows x64 VST3 build and installer smoke tests are pending for this candidate. Sonar visual/runtime behavior is not established here.

## 0.9.0 validation record



- Candidate commit `66af72965ac306cf426ea1e555f03ee36bc2db73` passed [Linux DSP CI](https://github.com/mamepaseri/VoiceMorph/actions/runs/45607489184), including strict warnings, ASan/UBSan, and Signalsmith-enabled synthetic quality checks.
- The same candidate passed [Windows x64 VST3 CI](https://github.com/mamepaseri/VoiceMorph/actions/runs/45607489381) with Signalsmith ON and OFF; both artifacts were uploaded.
- 0.9.0 carries forward the visual update already published as 0.8.8; no DSP or signal-routing behavior changed.
- This validates compilation and synthetic tests, not GUI rendering in Cakewalk Sonar / Zoom UAC-232 or natural voice quality. Robotic or warbly artifacts may remain.
- The main-branch commit will run the same checks before the installer workflow publishes 0.9.0.

## 0.8.8 validation record

## 0.8.8 visual candidate

- Updated the dashboard materials, graphite/cyan palette, and Turbo Racers text weight. No audio-processing behavior changed.
- The supplied Turbo Racers archive contains only the Regular TTF. The UI keeps that face and removes additional synthetic bolding rather than replacing it with a different family.
- [Windows x64 VST3 CI](https://github.com/mamepaseri/VoiceMorph/actions/runs/45605746681) passed with Signalsmith ON and OFF; both artifacts were uploaded. [DSP CI](https://github.com/mamepaseri/VoiceMorph/actions/runs/45605746768) passed strict-warning, sanitizer, and Signalsmith-enabled synthetic quality checks.
- The feature branch does not package installers. After the main commit passes both workflows, the release workflow will build the installer and run install/uninstall smoke checks.
- CI confirms compilation, not GUI rendering in a DAW. No Sonar playback or voice-quality claim is included.

## Earlier validation

## 0.8.5 release-candidate validation

- Candidate source commit `67c28f7ebb6839e82b426a0d60e9c6fba3a5a959` passed [Linux DSP CI](https://github.com/mamepaseri/VoiceMorph/actions/runs/45068254321): strict warnings, AddressSanitizer, UndefinedBehaviorSanitizer, and Signalsmith-enabled synthetic DSP quality tests.
- The same candidate passed [Windows x64 VST3 CI](https://github.com/mamepaseri/VoiceMorph/actions/runs/45068254248) with Signalsmith ON and OFF; both VST3 build artifacts were uploaded.
- The hybrid path now retains residual audio while pitch/local-periodicity confidence is uncertain. A faster confidence estimate releases that safety path promptly at voiced-to-fricative transitions while the slower estimate remains responsible for stable periodic splitting.
- At Voice Mix 100%, the separate delayed dry-reference blend is excluded. Low-confidence or unvoiced portions can still pass through as unshifted residual safety audio, independently of Breath, Consonant, and Transient Preserve. This can make those uncertain parts less shifted.
- These checks cover compilation and deterministic synthetic signals. They do not establish natural voice quality, labelled Japanese speech accuracy, or runtime behavior in Cakewalk Sonar / Zoom UAC-232. Robotic or warbly artifacts may remain.
- Stable v0.8.3 and the already published v0.8.4 beta remain unchanged.

## Earlier local validation (before 0.8.5)


- `cmake --list-presets=all` recognizes both Windows VS2022 configure presets and both build presets.
- Standalone speech-analysis / periodic-splitter / recombiner / hybrid-path / PSOLA-experiment / optional-adapter-stub tests configure and build with CMake 3.31.6 and GCC 14.2.
- CTest: 1/1 executable passed; the executable contains 70 assertions covering F0, quiet voice, sample-rate changes, noise/frication, silence, stale-F0 clearing, local periodicity gating, transients, splitter reconstruction, residual routing, latency-aligned recombination, hybrid stereo reconstruction, variable block sizes, transform failure fallback, in-place buffers, latency bounds, Signalsmith facade validation and dry fallback when its optional backend is absent, Tracking response-speed transition, Breath Preserve / Consonant Preserve / Transient Preserve gate behaviour, a narrow PSOLA pitch-up test, and correct unavailable-state reporting when Signalsmith is not compiled.
- The standalone target builds with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` and no warnings.
- The 70-assertion standalone suite passes in both normal and AddressSanitizer / UndefinedBehaviorSanitizer builds.
- A fresh standalone build passes AddressSanitizer + UndefinedBehaviorSanitizer with leak detection enabled.
- Two uploaded 48 kHz mono MP3 recordings (18.0 s and 9.64 s) were decoded to PCM and passed through the analyzer as an unlabeled stress check. For frames with voiced confidence >= 0.40, median estimated F0 was about 173 Hz (369 frames) and 256 Hz (474 frames), respectively. These are analyzer outputs only, not ground-truth accuracy measurements; the recordings are MP3 and are not confirmed as dry-mic captures.
- The processor routes the periodic stem through the optional Signalsmith hybrid facade, aligns residual/dry audio to reported latency, calls JUCE `setLatencySamples()`, and passes the Breath / Consonant / Transient Preserve parameters into the split gates. The candidate Windows CI run above compiled the JUCE VST3 with Signalsmith enabled and disabled; this confirms compilation and artifact creation, not host playback quality.
- The complete source ZIP passes `unzip -t`.
- The local Git bundle passes `git bundle verify` and records the complete local history.

## Findings that prevent release-quality audio

- `PitchSynchronousShifter` passed one synthetic 132 Hz / +7 semitone pitch-up check (about 196.7 Hz measured vs. 197.8 Hz expected). A wider manual matrix showed sizeable errors for several downward shifts at 220–400 Hz. In addition, the measured envelope alignment varied with F0. This implementation remains an experiment and is **not** connected to the plug-in's audible output.
- `SignalsmithPitchEngine` now supports a manual 48 ms block / 12 ms interval configuration candidate and reports input latency, output latency, and their sum. The default standalone test builds this adapter with the optional dependency disabled, so it only validates honest unavailable-state reporting; it does not build or test Signalsmith itself.

## Still not established by this validation

- Runtime Signalsmith pitch/formant quality, actual stream latency, host PDC, CPU peak load, or subjective audio quality.
- Real-time playback in Cakewalk Sonar / Zoom UAC-232.
- Tracker / splitter reliability on labelled dry Japanese speech, breaths, voiced fricatives, and mouth clicks.
- Full JUCE audio-callback integration with a built Signalsmith stream; the source is connected, but only the standalone mock-transform contract and backend-disabled fallback have been tested.

The JUCE VST3 target has not been built here because MSVC / Visual Studio 2022 is not available in this Linux workspace, and the build container cannot resolve GitHub to fetch JUCE / DSP dependencies. The JUCE audible path now calls the hybrid engine facade. With the optional Signalsmith backend disabled or unavailable, the audible path remains unpitched apart from gain / De-Esser processing; with the backend enabled, the periodic stem is transformed in source but still requires full-build and listening validation. CI workflows have been added but cannot run until the project is pushed to a GitHub repository.
