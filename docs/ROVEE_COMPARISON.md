# RoVee / VoiceMorph processed-audio comparison

Analysis date: 2026-11-20. This document records the two processed renders supplied for VoiceMorph tuning.

## Files

- RoVee render: `recording_20261119T151002_320kbps.mp3` (10.019 s)
- VoiceMorph 0.8.1 render: `recording_20261119T151029_320kbps.mp3` (9.587 s)

## Method

Both MP3 files were analysed with Praat autocorrelation pitch tracking through Parselmouth, using identical settings: 10 ms time step, 65 Hz pitch floor, 450 Hz ceiling, 0.45 voicing threshold, 0.03 silence threshold, and identical octave / octave-jump / voiced-unvoiced costs. Only positive voiced F0 frames were included.

## Measurements

| Metric | RoVee | VoiceMorph 0.8.1 |
|---|---:|---:|
| F0 median | 244.5 Hz | 248.1 Hz |
| F0 10th percentile | 218.9 Hz | 228.5 Hz |
| F0 90th percentile | 270.7 Hz | 264.6 Hz |
| 10–90% F0 span | 3.68 semitones | 2.53 semitones |
| voiced-frame proportion | 59.1% | 53.4% |

The medians are close, while the observed VoiceMorph render has a narrower F0 span. The new Pitch Dynamics control scales voiced deviations around a slow local log-F0 phrase baseline, with a bounded maximum expansion ratio of 1.45x.

## Important limitations

These are processed renders, not confirmed dry microphone captures. Their utterances, timing and source takes have not been verified as identical; the files are not sample-aligned or loudness-normalized. The measurements cannot isolate either plug-in's response or prove that VoiceMorph alone flattened the contour. The new control must be tuned by listening to the same dry source through both plug-ins.

These measurements assess F0 movement only. They do not assess formant accuracy, breath / fricative preservation, transients, phase quality, perceived voice identity, or robotic / warbly artifacts.
