# Changelog

## 0.8.0 — 2026-11-19

- Wired Breath Preserve, Consonant Preserve, and Transient Preserve into the periodic split-gating controls; synthetic regression tests verify each switch/amount changes the candidate as intended.
- Pinned Signalsmith Stretch to reviewed commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`; disabled shallow cloning for the exact-commit FetchContent declaration.
- Disabled the optional WORLD research analyzer by default because it is not needed by the live processing path.
- Expanded the standalone suite to 70 assertions.
## 0.7.0 — 2026-11-19

- Connected `SignalsmithHybridVoiceEngine` to the JUCE audio callback. The processor now supplies per-sample F0 / voiced / aperiodic controls, transforms only the periodic candidate, recombines the residual and dry reference at the engine-reported latency, and reports that latency to the host with JUCE `setLatencySamples()`.
- Added fixed-size per-block scratch arrays allocated in `prepareToPlay()` and chunking for host blocks that exceed the prepared block size; no audio-thread allocation is added by the processor integration.
- Added an explicit transform-bypass route that keeps the configured plugin latency and uses the aligned dry reference.
- Changed the Windows CI VST3 build to compile the optional Signalsmith backend while leaving WORLD disabled.
- The JUCE/Signalsmith plugin target remains unbuilt in this workspace; only the standalone DSP suite is verified here.
## 0.6.0 — 2026-11-19

- Added `HybridVoicePath`, a block-level orchestration layer for periodic/residual splitting, an externally supplied pitch/formant transform, and latency-aligned residual/dry recombination.
- Added `SignalsmithHybridVoiceEngine`, which binds the optional Signalsmith adapter to the hybrid path and uses an immediate dry fallback when the dependency is unavailable or a transform call fails.
- Added deterministic stereo/variable-block-size tests using a fixed-delay mock transform. The test checks reconstruction at the declared latency and verifies safe dry fallback when a transform callback fails.
- Hardened `LatencyAlignedRecombiner::prepare()` against allocation failure and invalid/unbounded latency requests. A failed re-prepare now leaves the processor unprepared rather than retaining stale active state.
- Added strict warning flags and optional ASan/UBSan configuration to the standalone test project; expanded the suite to 66 assertions.
- Added GitHub Actions workflows for portable DSP tests, sanitizer tests, and a Windows VS2022 VST3 prototype build.
- Clarified that JUCE must be used under AGPLv3 or its commercial licence; see `LICENSING.md`.
## 0.5.0 — 2026-11-19

- Added an original PSOLA-style pitch-only shifter experiment with preallocated ring buffers and a synthetic pitch-up test. Broader testing exposed downward-shift and envelope-alignment limits, so it is not connected to the audible path.
- Extended the Signalsmith adapter to use a manual 48 ms / 12 ms low-latency candidate configuration and expose summed input/output latency for later host-PDC measurement.
- Added explicit tests for the optional Signalsmith-unavailable build path and documented the evidence required before enabling either transform engine in the output path.
- Expanded the standalone suite to 33 assertions.
## 0.4.0 — 2026-11-19

- Added a 12 ms local one-period correlation gate to reduce stale-F0 leakage of breath and frication into the periodic candidate.
- Cleared stale F0 output when voiced confidence decays below the active threshold.
- Added `LatencyAlignedRecombiner`, a preallocated helper for delaying the preserved residual and dry fallback by a transform engine's measured latency.
- Expanded standalone DSP checks to 27 assertions, including stale-F0 noise rejection and impulse-based recombination alignment.
- Ran an unlabeled stress check on two user-uploaded 48 kHz mono MP3 recordings; results are documented without treating estimated F0 as ground truth.
## 0.3.0 — 2026-11-19

- Added streaming speech-feature analysis and the first-order periodic/residual shadow split.
- Added initial JUCE VST3 / Standalone project shell and supporting DSP architecture notes.

## Prototype 0.8.0 — Tracking control
- Connected the Tracking parameter to streaming F0 / voiced / aperiodic smoothing. Lower values favour stability; higher values follow pitch and voicing transitions faster.
- Added a regression test for a synthetic 132 Hz → 220 Hz transition. The test measures approximately 180 ms at 0% tracking and 60 ms at 100% tracking at 48 kHz.
