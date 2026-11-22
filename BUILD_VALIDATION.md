# Build / validation status — 2026-11-23 — Prototype 0.8.5

## 0.8.5 candidate update — 2026-11-23

- Fixed low-confidence residual routing in the hybrid voice path and added onset, frication, intermittent-F0, fading-tail, and Voice Mix sweep regressions. Stable v0.8.3 and published v0.8.4 beta release assets are unchanged.
- These new C++ tests have not been compiled or run in this checkout: CMake and a C++ compiler are not installed in the available Windows environment. The Visual Studio files present here do not include the MSVC C++ toolset. The Windows VST3 candidate therefore remains unbuilt.
- RoVee / GVST GForm remain architecture and sound references only. No same-source DAW render or Sonar/UAC-232 listening comparison was available in this session.

## Validated in this workspace

- `cmake --list-presets=all` recognizes both Windows VS2022 configure presets and both build presets.
- Standalone speech-analysis / periodic-splitter / recombiner / hybrid-path / PSOLA-experiment / optional-adapter-stub tests configure and build with CMake 3.31.6 and GCC 14.2.
- CTest: 1/1 executable passed; the executable contains 70 assertions covering F0, quiet voice, sample-rate changes, noise/frication, silence, stale-F0 clearing, local periodicity gating, transients, splitter reconstruction, residual routing, latency-aligned recombination, hybrid stereo reconstruction, variable block sizes, transform failure fallback, in-place buffers, latency bounds, Signalsmith facade validation and dry fallback when its optional backend is absent, Tracking response-speed transition, Breath Preserve / Consonant Preserve / Transient Preserve gate behaviour, a narrow PSOLA pitch-up test, and correct unavailable-state reporting when Signalsmith is not compiled.
- The standalone target builds with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` and no warnings.
- The 70-assertion standalone suite passes in both normal and AddressSanitizer / UndefinedBehaviorSanitizer builds.
- A fresh standalone build passes AddressSanitizer + UndefinedBehaviorSanitizer with leak detection enabled.
- Two uploaded 48 kHz mono MP3 recordings (18.0 s and 9.64 s) were decoded to PCM and passed through the analyzer as an unlabeled stress check. For frames with voiced confidence >= 0.40, median estimated F0 was about 173 Hz (369 frames) and 256 Hz (474 frames), respectively. These are analyzer outputs only, not ground-truth accuracy measurements; the recordings are MP3 and are not confirmed as dry-mic captures.
- The current processor source routes the periodic stem through the optional Signalsmith hybrid facade, aligns residual/dry audio to reported latency, calls JUCE `setLatencySamples()`, and passes the Breath / Consonant / Transient Preserve parameters into the split gates. This source integration is not yet compiled as a JUCE plugin.
- The complete source ZIP passes `unzip -t`.
- The local Git bundle passes `git bundle verify` and records the complete local history.

## Findings that prevent release-quality audio

- `PitchSynchronousShifter` passed one synthetic 132 Hz / +7 semitone pitch-up check (about 196.7 Hz measured vs. 197.8 Hz expected). A wider manual matrix showed sizeable errors for several downward shifts at 220–400 Hz. In addition, the measured envelope alignment varied with F0. This implementation remains an experiment and is **not** connected to the plug-in's audible output.
- `SignalsmithPitchEngine` now supports a manual 48 ms block / 12 ms interval configuration candidate and reports input latency, output latency, and their sum. The default standalone test builds this adapter with the optional dependency disabled, so it only validates honest unavailable-state reporting; it does not build or test Signalsmith itself.

## Not validated in this workspace

- Full Visual Studio compilation of JUCE / Signalsmith / WORLD.
- A compiled Windows VST3 / Standalone binary.
- Runtime Signalsmith pitch/formant quality, actual stream latency, host PDC, CPU peak load, or subjective audio quality.
- Real-time playback in Cakewalk Sonar / Zoom UAC-232.
- Tracker / splitter reliability on labelled dry Japanese speech, breaths, voiced fricatives, and mouth clicks.
- Full JUCE audio-callback integration with a built Signalsmith stream; the source is connected, but only the standalone mock-transform contract and backend-disabled fallback have been tested.

The JUCE VST3 target has not been built here because MSVC / Visual Studio 2022 is not available in this Linux workspace, and the build container cannot resolve GitHub to fetch JUCE / DSP dependencies. The JUCE audible path now calls the hybrid engine facade. With the optional Signalsmith backend disabled or unavailable, the audible path remains unpitched apart from gain / De-Esser processing; with the backend enabled, the periodic stem is transformed in source but still requires full-build and listening validation. CI workflows have been added but cannot run until the project is pushed to a GitHub repository.
