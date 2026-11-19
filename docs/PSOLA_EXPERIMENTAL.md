# PSOLA pitch shifter — experimental findings

Date: 2026-11-19

`PitchSynchronousShifter` is a first original attempt at pitch-synchronous overlap-add for the periodic candidate. It is not a vendor-derived implementation and it is not connected to the JUCE output.

## What passed

- At 48 kHz, a steady synthetic 132 Hz harmonic signal shifted up by 7 semitones measured approximately 196.7 Hz; the mathematical target is approximately 197.8 Hz.
- The module allocates its rings in `prepare()`, not in `processSample()`.
- A silent periodic input does not generate a residual tone.

## What did not pass broadly

A manual matrix using a 5-semitone downward shift produced these approximate measured frequencies (zero-crossing estimate):

| Input F0 | Expected | Measured |
|---:|---:|---:|
| 82 Hz | 61.4 Hz | 60.0 Hz |
| 132 Hz | 98.9 Hz | 98.3 Hz |
| 220 Hz | 164.8 Hz | 216.7 Hz |
| 300 Hz | 224.7 Hz | 253.3 Hz |
| 400 Hz | 299.7 Hz | 315.0 Hz |

Some upward-shift checks were close to target, but this asymmetry is not acceptable as a general-purpose voice shifter.

A separate slow-amplitude-envelope correlation check (48 kHz, zero-semitone setting) reported a nominal scheduled look-ahead of 2,224 samples, while the best envelope-correlation lag varied with source F0: 2,380 samples at 82 Hz, 2,312 at 132 Hz, 2,179 at 220 Hz, 2,273 at 300 Hz, and 2,252 at 400 Hz. This is a diagnostic measurement rather than a definitive group-delay characterization, but it is sufficient to show that the time-domain grain selection has not established the fixed cross-path alignment required by VoiceMorph.

## Decision

Do not connect this version to the audible output, do not report its nominal look-ahead as verified plugin latency, and do not use it to align the preserved breath/consonant path. Keep it as an isolated experiment. The next implementation candidate is Signalsmith Stretch applied only to the periodic candidate, with its input/output latency halves summed and independently measured before recombination.
