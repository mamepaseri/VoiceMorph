# VoiceMorph stabilization acceptance — 0.9.0 RC

Baseline: main commit 29591bf26bbc2dcf1c66c4d164fa0445a4af52d4 (public v0.8.3 prerelease retained unchanged).

This checklist governs the stabilization work and release decision. A checked item requires evidence attached in the relevant track report or CI run. Synthetic DSP results do not establish naturalness.

## Release gates

- [ ] G0 — Baseline and scope: confirm the candidate descends from the stated baseline; preserve v0.8.3 tag, assets, and release metadata.
- [ ] G1 — DSP/audio: add or extend deterministic synthetic tests for F0 register transitions, vibrato/glide, octave outliers, unvoiced release, timing cadence, and transient/breath/consonant behavior. Include sample-rate and block-size coverage. Document limits and accepted residual defects.
- [ ] G2 — GUI/parameters: verify rotary value-to-angle and marker/readout agreement; minimum editor size and layout; control clarity/accessibility; APVTS state save/restore; preset and A/B slot behavior. Confirm stable host parameter IDs and automation compatibility.
- [ ] G3 — RT/host stability: inspect the full callback path for allocation, locks, denormals, and unsafe state access. Exercise variable blocks, mono/stereo, 44.1/48/96/192 kHz, silence/near-silence, and long-run/stress paths. Verify finite output and latency/PDC behavior. Run VST3 validator/pluginval or a DAW scan where available.
- [ ] G4 — Release engineering: follow docs/DISTRIBUTION_COMPLIANCE.md; verify workflow dependencies, branch/ref guards, exact validated commit provenance, license/source notices inside or beside the package, hashes, one-installer per-user VST3 install, clean uninstall, host scan/insert in Sonar, unsigned/signed status, and documentation/version consistency.
- [ ] G5 — AGPLv3 compliance: owner selected AGPL-3.0-only on 2026-11-22. SPDX identifiers, dependency inventory, Advanced Settings license/source links, corresponding-source packaging, and release-workflow source/hash gates are implemented on this branch. Verify contributor rights; successfully build and inspect the complete source archive (VoiceMorph, build scripts, JUCE, Signalsmith Stretch and Signalsmith Linear); verify the exact-version UI/release links and hashes on the same validated commit. No formal GA or new binary publication until these items are evidenced.
- [ ] G6 — Integrated candidate: merge track changes into one candidate, run the complete required suite and platform build against that exact commit, attach artifact hashes/logs, and compare the candidate against this checklist.
- [ ] G7 — Listening evidence: listen to the candidate in Cakewalk Sonar using Zoom UAC-232 and representative dry voice material. Record operator, device/settings, examples, and observed defects. Do not describe sound as natural or production-ready without this evidence.

## Workstreams and integration checkpoints

1. DSP/audio quality owns Source/Dsp, synthetic audio tests, and DSP limitation notes.
2. GUI/parameter integrity owns editor/control/APVTS tests and parameter compatibility evidence.
3. RT stability / QA owns callback audit, host-matrix tests, sanitizer/validator evidence, and runtime defect triage.
4. Release engineering / licensing owns workflow/installer/docs audit and evidence for the selected AGPLv3 route.

Track changes should remain independently reviewable. License and install controls are specified in docs/LICENSING_ROUTE_REVIEW.md and docs/DISTRIBUTION_COMPLIANCE.md. Integration checkpoints: (A) each track reports findings, proposed patch, evidence, and unresolved risks; (B) integrate only reviewed track changes onto this candidate; (C) run cross-track regressions after integration; (D) tag/release only the exact passing candidate commit. No version string, prerelease flag, or release tag alone satisfies a gate.

## Known baseline evidence and blockers (2026-11-21)

- Existing documentation describes synthetic coverage for mapper contour behavior and broad DSP components, while explicitly stating that full JUCE/Signalsmith plugin builds and Sonar/UAC-232 listening have not been completed.
- Existing DSP suite already exercises neutral/upward mapping, vibrato span, single-frame outlier rejection and unvoiced release. Add sustained register changes, repeated octave slips, confidence drop/re-entry, monotonic glide, slow register drift, and alias-sensitive high-harmonic signals; the mapper cadence is hardcoded to 10 ms and must stay coupled to analyzer cadence.
- GUI audit found the stable parameter ID inventory and rotary value/angle mapping internally consistent. Add custom-state recognition for restored parameters: the preset picker can display Default for a saved custom state. Add versioned serialization/round-trip coverage for both A/B snapshots and active slot; current host state restoration appears to preserve only active APVTS state and then duplicate it into both slots.
- RT audit found preallocated callback scratch, relaxed atomic parameter reads, and ScopedNoDenormals in the visible path, but no processor-level harness. Nonfinite raw samples can bypass analyzer sanitization and propagate through the dry/split/recombine path. Add processor-level NaN/+Inf/-Inf tests and sanitize before downstream processing.
- Existing engine matrix includes 88.2 and 176.4 kHz, while baseline analyzer tests report 44.1/48/96/192 kHz. Align matrices and test claims. Existing tests cover hybrid mock blocks/stereo but not the actual JUCE processBlock callback or built Signalsmith backend.
- Baseline docs say Windows VST3 and full Signalsmith-on/off plugin builds, VST3 validation, host PDC, installer smoke tests, and Sonar/UAC-232 listening remain unverified. The GitHub connector returned no workflow runs/checks for the baseline commit; obtain fresh evidence from the actual workflows before candidate approval.
- The owner selected AGPL-3.0-only and added the license text to this branch. SPDX identifiers, a source archive build script, exact-source workflow checks, and license/source controls are drafted; fresh CI evidence is pending. The baseline v0.8.3 tag/release assets remain unchanged and predate these changes. Treat any missing workflow evidence, rights confirmation, host validation, or listening evidence as a blocker for a new binary release.
- The candidate is not approved for formal GA or a new binary release until all mandatory gates are evidenced, including AGPLv3 source delivery. A 0.9.0 RC may be prepared after technical integration gates pass; RC status does not waive license obligations.
