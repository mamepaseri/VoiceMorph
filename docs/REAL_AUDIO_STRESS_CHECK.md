# Unlabelled real-audio stress check — 2026-11-19

Two MP3 recordings already present in the conversation workspace were decoded to mono 48 kHz float PCM and passed through `SpeechFeatureAnalyzer`. The original audio files are intentionally not copied into this project archive.

| Clip | Duration | RMS | Frames with voiced confidence >= 0.40 | Median estimated F0 | 5th–95th percentile F0 |
|---|---:|---:|---:|---:|---:|
| A | 18.005 s | 0.0332 | 369 / 1800 | 172.6 Hz | 97.6–323.9 Hz |
| B | 9.643 s | 0.1130 | 474 / 964 | 256.4 Hz | 106.1–367.8 Hz |

## Interpretation limits

- These are raw analyzer estimates, not measured reference F0 labels.
- The MP3 source chain is unknown; neither clip is confirmed as an unprocessed UAC-232 microphone recording.
- The 5th–95th percentile ranges are wide because speech is not a sustained vowel and the gate is deliberately conservative around transitions. They should not be interpreted as a quality score.
- The recordings are useful for checking that the code runs through realistic non-stationary audio, but they cannot establish octave-error rate, breath leakage, or naturalness without a labelled/dry source and listening comparison.

## Follow-up corpus needed

Use dry mono or stereo WAV from the UAC-232 at 48 kHz / 24-bit, with ordinary Japanese conversation, low-register phrase endings, voiced and unvoiced transitions (/s/, /sh/, /h/, /k/, /t/), breath, whisper, mouth clicks, plosives, and silence. Preserve the same source takes for all A/B comparisons.
