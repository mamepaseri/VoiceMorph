# Preserved VoiceMorph VST3 candidate packages

These are Windows x64 VST3 bundles for evaluation. They are separate from the official installer releases and should be installed only after closing the DAW.

## Included candidates

- **0.8.4 dropout fix:** keeps uncertain voiced material in the pitch/formant route to prevent an unshifted voice leak at Voice Mix 100%, with onset, frication, tail, and level regression coverage.
- **0.8.6:** adds exception containment around the Signalsmith transform and a long-running synthetic stereo stress regression.
- **0.8.7:** uses the system typeface to isolate a possible Live editor-rendering issue. That change did not establish a crash fix.
- **0.8.8 Live 12 latency candidate:** retains host-reported latency across prepare/release cycles and avoids redundant latency notifications. The Live crash cause was not confirmed.

## Source and release history

The ZIPs preserve local candidate binaries. The exact intermediate source worktrees for these builds were not recorded as independent Git commits or semantic-version tags, so these files are binary snapshots rather than reproducible source releases. The 0.8.8 candidate is distinct from the official 0.8.8 visual beta and the later 0.9.0 GUI beta. Existing official releases remain unchanged.

These candidates focus on dropouts and Live stability. They do not resolve the reported double-sounding or robotic voice quality; that follow-up is being worked against the current 0.9.0 source.

SHA-256 hashes are listed in `SHA256SUMS.txt`.
