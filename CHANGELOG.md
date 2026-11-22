# Changelog

## 0.8.4 — 2026-11-23 — Experimental beta

- Embedded Turbo Racers Regular in the plug-in so the dashboard lettering is consistent across systems and DAWs.
- Refined the rotary bezel with a chrome/aluminum metal ring, a compact cyan value tab, and a thinner amount arc around the LCD edge.
- Restored the full NATURAL / BALANCED / PRECISE Voice Mode labels. Kept the Input and Output faders equal in width; Voice Mix remains smaller than the matching Tracking and Breath dials, with no numeric readout inside them.
- Windows x64 VST3 builds passed with Signalsmith enabled and disabled. DSP tests and installer install/uninstall smoke checks passed.

**Experimental limitations:** This visual release does not change the pitch-shifting algorithm or establish natural voice quality. No Cakewalk Sonar / Zoom UAC-232 listening validation was performed; robotic or warbly artifacts may remain.


## 0.8.3 — 2026-11-20 — Experimental beta

- Added an independent **Pitch Dynamics** control. It scales voiced F0 movement around a slow phrase baseline, separately from fixed Pitch semitones.
- Dynamics range is 1.00x at 0% to a bounded 1.45x at 100%; default is 75%.
- Added voiced-confidence weighting, a one-frame hold for large F0 jumps until a confirming frame, and smoothed pitch modulation (45 ms attack / 75 ms release).
- Split processing into at-most-10 ms control blocks so pitch transposition can follow the analyzer updates more locally.
- Added a Pitch Dynamics dial to the lower GUI row, a numeric percentage readout, compact Voice Mode buttons, and a graphite/cyan dark theme.
- Extended the pitch contour monitor target with the effective dynamic offset. Presets now set their own Dynamics amount.
- Added synthetic DSP regression coverage for neutral mapping, upward F0 movement, contour-span expansion, isolated octave-like jumps, and unvoiced release.
- The supplied renders measured close median F0 (244.5 / 248.1 Hz) but different 10–90% spans (3.68 / 2.53 semitones; RoVee / VoiceMorph 0.8.1). Because the source takes are not confirmed identical or aligned, this supports an adjustable control rather than a direct sound-quality claim. See [docs/ROVEE_COMPARISON.md](docs/ROVEE_COMPARISON.md).

**Experimental limitations:** This is a contour-shaping update, not a new speaker-identity model. The periodic/residual splitter and Signalsmith path remain prototype quality. No Cakewalk Sonar / Zoom UAC-232 real-voice audition was performed; robotic or warbly artifacts may remain.

## 0.8.2 — 2026-11-20
## 0.8.2 — 2026-11-20

- Redesigned the plug-in editor with dark/light themes, a three-second pitch monitor, value arcs, and a separate resizable Advanced window.
- Reworked Voice Mode into a three-state button group, moved Input/Output faders side by side on the right, and resized the Pitch, Formant, and Voice Mix controls.
- Corrected parameter-choice and rotary display mapping and added input/target pitch history for the monitor.
- Kept Mix=100% free of the dry-reference signal while preserving only residual/transient content explicitly enabled by its controls.
- Added deterministic Signalsmith processing and regression coverage for mapping, Mix endpoints, synthetic signals, pitch, and latency.
- Updated automatic release packaging to require successful Windows VST3 and DSP workflows for the same `main` commit and to skip stale builds.

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
