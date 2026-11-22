# VoiceMorph — open-source DSP engine plan

Updated: 2026-11-19 — Prototype 0.8.0

## Design target

VoiceMorph is a non-AI, real-time VST3 voice transformer for Cakewalk Sonar / Zoom UAC-232 ASIO. The target is to preserve the useful character of RoVee-style pitch/formant transformation while avoiding the objectionable processing of non-voiced sound (breath, sibilants, consonant releases, and mouth noise).

The target latency is 20–50 ms end-to-end, but this is a measurement goal, not a current result. The audio engine must report and compensate its actual delay to the host. A mode that cannot remain in budget must be presented as a higher-latency quality option, not silently substituted.

## Recommended signal path

```text
Input / trim
  -> analysis tap (mono/stereo link, sample-rate aware)
  -> pitch + voicing/periodicity analysis
  -> split into voiced / aperiodic / transient confidence
       voiced harmonic component
         -> pitch shift
         -> independent spectral-envelope / formant movement
       aperiodic component (breath, sibilance, frication)
         -> preserve original detail, or apply only gentle spectral shaping
       transient component (plosive onset / mouth click / consonant edge)
         -> latency-aligned original path with short confidence-controlled crossfade
  -> recombination with explicit phase/time alignment
  -> optional de-esser
  -> output trim / host PDC
```

Do not use a simple unaligned dry/wet blend as the solution. A delayed transformed path mixed with the immediate original can produce comb filtering or audible doubling. The preserved path must be delayed to the same effective time as the processed path, and the crossover must be smoothed to avoid abrupt voice-to-dry switching.

## Engine selection

### 1. Signalsmith Stretch — permissive real-time comparison engine

- Upstream: https://github.com/Signalsmith-Audio/signalsmith-stretch
- Revision policy: pinned to commit `57b93f4e9206a089a45387eaa39bdc9f310d3308` for reproducible builds. The pin should be revisited only after review and test.
- License: MIT.
- Role: a readily usable C++ pitch/time baseline with independent transpose and formant controls; suitable for an A/B benchmark and a fallback experiment.
- Limitation: upstream documentation says its formant correction is less sharp than monophonic methods such as PSOLA and requires an approximate F0 base. It is a full-band/polyphonic stretcher, not the final answer to protecting sibilants and breath.
- Current source state: `SignalsmithPitchEngine` adapter is behind `VOICEMORPH_ENABLE_SIGNALSMITH`. It preallocates scratch buffers in `prepare()`, accepts a manual 48 ms / 12 ms configuration candidate, exposes input and output latency separately, and returns their sum as a candidate host-PDC latency. Signalsmith's documentation defines these as two halves of its processing-time offset; the sum still requires a built-runtime check. It is connected to the periodic candidate through `SignalsmithHybridVoiceEngine`; this source integration still requires a full dependency build and real-host latency/audio validation.

### 2. WORLD — voice-specific analysis backbone

- Upstream: https://github.com/mmorise/World
- Revision: formal release tag `v1.0.1` (2026).
- License: modified BSD; upstream states the algorithms are not patent-encumbered.
- Role: voice analysis into F0, spectral envelope, and aperiodicity. DIO/Harvest provide F0 candidates, StoneMask refines DIO, CheapTrick estimates the spectral envelope, and D4C estimates aperiodicity. These representations are exactly the useful ingredients for independently moving pitch/formants while treating noisy consonant/breath content differently.
- Limitation: WORLD is an analysis/synthesis vocoder, not a complete low-latency female-voice changer. We still need a causal streaming schedule, quality-safe interpolation, phase/time-aligned preservation, and measured latency. CheapTrick's effective low-F0 floor depends on its FFT size; VoiceMorph records the actual floor returned by `GetF0FloorForCheapTrick()` rather than assuming the requested floor is supported.
- Current source state: `WorldSpeechAnalyzer` is added behind `VOICEMORPH_ENABLE_WORLD`. It is a worker-thread/window analysis utility only, not a real-time callback and not yet runtime-validated in the audible path.

### 3. Rubber Band — optional high-quality benchmark, not default

- Upstream: https://github.com/breakfastquay/rubberband
- License: GPL-2.0-or-later, or a separate commercial license.
- Role: useful A/B candidate for the higher-quality pitch/formant path. R3 supports independent formant scaling.
- Constraints: the current `RubberBandLiveShifter` documentation explicitly describes 50 ms or more delay depending on configuration. That is already at or beyond the top edge of the target. It is excluded from the default build until the user chooses a compatible project licensing/distribution model and measurements justify it.

### 4. pbshift — experimental benchmark candidate only

- Upstream: https://github.com/pbtechlab/pbshift
- License: MIT; its repository documents third-party notices for PFFFT and Signalsmith-derived code.
- Role: worth benchmarking because it documents a streaming voice mode with pitch-synchronous grains, YIN tracking, True Envelope formant handling, and a latency-focused tier.
- Constraints: the repository describes itself as pre-1.0 / under active development, has no stable release identified in this review, and reports its own latency/quality numbers. The reported values are not independent measurements. Do not make it a production dependency until the exact commit, dependency notices, build, and listening tests are reviewed.

### 5. libfvad / WebRTC VAD — optional gate, not voice/unvoiced separation

- Upstream: https://github.com/dpirch/libfvad
- License: BSD-3-Clause (repository also includes a PATENTS notice that must be reviewed and retained).
- Role: speech-activity gating for suspending or holding pitch tracking during silence / non-speech intervals.
- Limitation: VAD answers “speech activity?”; it does not separate voiced vowels from unvoiced /s/, /sh/, aspiration, or plosive transients. It must not be mistaken for the voiced/aperiodic separator.

## Streaming analysis layer added in prototype 0.3

`SpeechFeatureAnalyzer` runs on the audio callback with fixed-size ring storage and no per-sample heap allocation. It downsamples the mono analysis tap by 4, evaluates a 32 ms history every 10 ms, and publishes:

- a YIN-style cumulative-mean-normalized difference F0 candidate in the 65–450 Hz range (with a normalized correlation confidence); tested synthetic points include 68 Hz and a quiet approximately -60 dBFS-class 132 Hz tone;
- periodicity and smoothed voiced confidence;
- high-band energy ratio and smoothed aperiodic confidence;
- short-window RMS and a simple peak-delta transient indicator.

The analyzer's results are currently displayed in the UI status area. An `AdaptivePeriodicSplitter` consumes F0, voiced confidence and aperiodic confidence both in the actual hybrid path and in a separate meter-only path; it also applies a sample-domain slope/envelope transient gate so it can react on the transient sample rather than 10 ms late. It predicts a periodic component by averaging the current sample with a fractional one-period delayed sample, then computes the residual as `input - periodic`; those two stems sum exactly to the input. Breath Preserve sets the aperiodic protection amount; Consonant Preserve toggles that gate; Transient Preserve toggles the immediate transient mask. A 12 ms exponentially weighted local correlation between the current sample and the one-period predecessor adds a second gate. The hybrid path now uses tracker and local-periodicity confidence to retain residual detail while either classifier is uncertain, independently of the preservation controls. This catches transition errors where the 10 ms F0/voicing tracker still reports the previous vowel at the beginning of /s/, aspiration, or breath, and prevents the periodic stem and residual from being gated out together at a quiet onset. The analyzer also clears a decayed F0 instead of exposing a tiny stale positive value.

This is still a first-order predictor, not production-grade source separation. It can leak noisy energy into the periodic stem and can lose periodic energy under pitch jitter, rapid F0 movement, or phase discontinuities. The hybrid path currently sends the periodic candidate to the optional pitch/formant transform and keeps the residual on a parallel path. This first-order decomposition is not production-grade source separation. Both tracker and splitter need dry-mic corpus validation, especially for octave errors, breathy vowels, voiced fricatives, and very quiet phrase endings.

### Latency-aligned recombination groundwork

`LatencyAlignedRecombiner` (`Source/Dsp/LatencyAlignedRecombiner.*`) is a separately testable utility. It accepts a transformed-voice sample that is already delayed by the transform engine, delays the preserved residual and dry reference by the engine's measured latency, and provides a smoothed Voice Mix between the hybrid result and delayed dry fallback. Its delay memory is allocated in `prepare()`, and the sample loop allocates nothing. Unit tests use impulse locations and a zero-semitone identity case to verify sample alignment and reconstruction.

The helper does not estimate third-party latency or implement a pitch shifter. The JUCE processor now invokes the hybrid facade, obtains the Signalsmith input/output latency sum, aligns the preserved path to that value, and reports it through JUCE `setLatencySamples()`. The value is only a candidate until the Signalsmith dependency is built and actual host PDC is measured. Signalsmith describes its input and output latency as two separate timing offsets; input is supplied ahead of the processing time and output is returned behind it. The project must not treat either half alone as total latency (see the upstream [Signalsmith Stretch documentation](https://github.com/Signalsmith-Audio/signalsmith-stretch#latency)).

### Hybrid block orchestration integrated in prototype 0.8.0

`HybridVoicePath` (`Source/Dsp/HybridVoicePath.*`) now owns the streaming periodic splitters, per-channel residual / transformed scratch buffers, and latency-aligned recombiners. It accepts a caller-supplied block transform function, so the transform backend can be replaced without changing the split/recombine contract. The caller supplies per-sample F0, voiced confidence, and aperiodic confidence, plus Breath Preserve strength and Consonant / Transient Preserve switches. All buffers are prepared before audio processing; `processBlock()` performs no heap allocation and accepts changing block sizes up to its prepared capacity. A transform callback failure immediately selects the latency-aligned dry reference rather than fading through a residual-only signal. The class supports mono and stereo-linked channels and rejects oversized blocks before touching scratch memory.

The contract is verified with a deterministic fixed-delay mock transform: two-channel reconstruction, variable block sizes, exact latency placement, transform failure fallback, and in-place host buffers are covered. `SignalsmithHybridVoiceEngine` now binds the Signalsmith adapter to this hybrid path and keeps a zero-latency dry route when the optional backend is disabled. The JUCE processor now invokes the facade, updates Pitch/Formant parameters, passes per-sample analysis controls and the three first-pass preservation controls, and reports its configured latency to the host. In this workspace only the backend-disabled dry route can be tested. The actual Signalsmith transform has not been built, so no claim of audible pitch/formant quality or verified real-host PDC is made.

## Current source implementation status

| Component | Status | Notes |
|---|---|---|
| JUCE VST3 + Standalone shell | Present | UI and parameter state are wired. |
| Signalsmith adapter | Added, optional dependency | Manual 48 ms / 12 ms candidate; connected to periodic stem in source; not built or measured here. |
| WORLD analyzer | Added, optional dependency | Worker-thread analysis only; F0 / envelope / aperiodicity output vectors. |
| Streaming speech-feature tracker | Added, built-in | Fixed-memory YIN-style F0 / periodicity / high-band / transient heuristics. |
| Adaptive periodic/residual split | Added, built-in | One-period predictive split plus local correlation / transient gates feeds the hybrid path; exact residual + periodic candidate reconstruct input, but separation quality is not yet production-grade. |
| Latency-aligned recombination utility | Added, built-in | Unit-tested residual/dry delay, transform-failure fallback, and Voice Mix helper. |
| HybridVoicePath orchestrator | Integrated in source | Mock-transform tested for mono/stereo, variable blocks, fixed latency, failure fallback and in-place buffers; called by the JUCE processor through the facade. |
| Signalsmith hybrid facade | Integrated in source | Connects the optional Signalsmith adapter to the hybrid path; backend-disabled dry fallback is tested, actual backend is not yet built. |
| PSOLA pitch experiment | Added, experimental | One pitch-up test passes; down-shift / F0-dependent alignment limitations prevent output-path use. |
| WORLD realtime synthesis/recombination | Not implemented | Requires real streaming scheduler and integration with latency alignment. |
| Voiced / aperiodic / transient separation | Not implemented | A first-order periodic/residual shadow split exists, but it is not a reliable voiced/aperiodic separator; the hybrid path does align its output streams using the configured transform latency. |
| Main pitch + formant transform | Source-integrated, unverified | Pitch/Formant controls feed Signalsmith only when compiled in; no audible-quality or runtime claim until full build and host testing. |
| Rubber Band | Research-only | GPL/commercial terms and 50ms+ live delay. |
| pbshift | Research-only | Early repo; not bundled. |
| libfvad | Research-only | Optional speech-activity gate, not detailed consonant separation. |

## PSOLA experiment status

- `PitchSynchronousShifter` is an original sample-streaming PSOLA-style experiment for the periodic stem. It preallocates its ring buffers and has a fixed scheduled look-ahead.
- The first synthetic check (132 Hz input, +7 semitones) produced approximately 196.7 Hz against a 197.8 Hz target. That is a narrow, positive-shift test, not proof of general pitch accuracy.
- A broader test matrix found significant errors for several downward-shift cases at 220–400 Hz, and envelope-correlation timing varied with F0. It must not be used in the audible path or relied upon for residual alignment yet.
- It is pitch-only; no independent formant shift exists in this module. Detailed measurements are in `docs/PSOLA_EXPERIMENTAL.md`.

## Benchmark plan

Every engine must be tested on the same mono/stereo corpus at 44.1 / 48 / 96 kHz and block sizes 64 / 128 / 256 / 512 samples. Use:

1. Low male speech with falling pitch and quiet sentence endings (detect octave errors / dry reversion).
2. Japanese vowels and consonant transitions, including /s/, /sh/, /h/, /k/, /t/, and breathy phrase endings.
3. Whisper, breath, mouth clicks, plosives, and silence (measure unwanted pitch transformation).
4. Sustained vowels with glides and vibrato (measure pitch continuity and formant stability).
5. Loudness-matched blind A/B against RoVee and the user's chosen reference plugins.

Record measured plugin latency/PDC, CPU average and worst-case, F0 gross-error rate, voiced/unvoiced leakage, spectral-envelope discontinuity, and user-rated naturalness. Do not accept a “Live” mode based only on the vendor's or repository author's published number.
