# VoiceMorph DSP test matrix

This is the repeatable A/B test plan for choosing an engine. Keep source WAVs, exact plugin settings, sample rate, block size, measured PDC, and render hashes with each run.

## Input cases

| ID | Source condition | What it exposes |
|---|---|---|
| V01 | Low register male speech, descending phrase endings | F0 floor, octave errors, pitch snap-back |
| V02 | Low register speech with quiet onset and low-level tail | Tracker confidence hysteresis, dry reversion |
| V03 | Japanese /s/, /sh/, /h/ and breathy vowels | Sibilance / aspiration handling |
| V04 | /k/, /t/, /p/ plosive starts and sentence transitions | Transient preservation / latency alignment |
| V05 | Sustained vowel with glide and vibrato | Pitch continuity, spectral envelope stability |
| V06 | Whisper, mouth click, breathing, silence | False voice detection and noise transformation |
| V07 | Loud speech followed by quiet speech | Gain-dependent tracker reliability |

## Realtime settings

- Sample rates: 44.1, 48, 88.2, 96, 176.4, and 192 kHz.
- Host block sizes: 64, 128, 256, and 512 samples.
- Channels: mono and stereo-linked.
- Run at least five minutes per configuration to identify state drift, memory growth, and glitches.
- Report host/plugin PDC and measure loopback latency independently; do not treat engine-reported input/output halves as end-to-end latency without applying the documented alignment.

## Current automated analyzer tests

The standalone test target currently covers 68, 82, 132, and 176 Hz harmonic signals, a quiet -60 dBFS-class 132 Hz tone, noise-like frication, silence, a single-sample transient (including the splitter's sample-domain gate), and F0 tracking at 44.1 / 48 / 96 / 192 kHz. It also checks that a stable 132 Hz sinusoid is concentrated into the periodic stem, zero confidence leaves noise in the residual, and `periodic + residual` exactly reconstructs the input. The expanded suite also checks the block-level `HybridVoicePath` contract with a deterministic fixed-latency mock transform: stereo reconstruction, variable block sizes, correct dry fallback on transform failure, and in-place buffers. These synthetic tests do not validate the actual Signalsmith engine, replace real mic recordings, or substitute for listening tests.

## Measurements

- F0 gross error / octave jump count.
- Fraction of input frames classified as voiced, unvoiced, and transient.
- Aperiodic energy retained versus transformed.
- Output spectral-envelope continuity over 5 ms hops.
- RMS and true-peak change.
- Average and worst-case processing time per callback.
- Dropouts / buffer underruns.
- Blind naturalness score against the unmodified source and RoVee reference.

## Acceptance gates

1. No low-register segment may abruptly switch to unprocessed pitch when the tracker loses confidence; use confidence hysteresis and a smooth hold/decay policy.
2. Sibilants and breaths must not become metallic or chorus-like. Preservation must use a time-aligned path, not an unaligned dry blend.
3. The engine must remain within the measured latency target for the selected mode; a >50 ms mode must be explicitly named as a higher-latency mode.
4. Audio-thread code must avoid allocation, locks, file access, and blocking work. WORLD window analysis belongs on a worker thread until a streaming-safe schedule is implemented.
