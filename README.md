# VoiceMorph — Windows x64 VST3 experimental beta 0.9.0

VoiceMorph is an experimental Windows x64 VST3 voice-transformation plug-in. It uses pitch/formant processing and a hybrid periodic/residual signal path. This is an early prototype: transformed speech can still sound robotic or warbly.

## Download and install

Get the latest Windows x64 installer from [GitHub Releases](https://github.com/mamepaseri/VoiceMorph/releases/latest). Download the direct [Windows x64 installer EXE](https://github.com/mamepaseri/VoiceMorph/releases/download/v0.9.0/VoiceMorph-Setup-x64-0.9.0.exe), or the [ZIP copy](https://github.com/mamepaseri/VoiceMorph/releases/download/v0.9.0/VoiceMorph-Setup-x64-0.9.0.zip). The installer offers per-user installation by default and can also install for all users. Close your DAW before installing or uninstalling, then rescan its VST3 plug-ins.

The installer is unsigned, so Windows SmartScreen may show an unfamiliar-app warning. It installs the built `VoiceMorph.vst3` bundle and includes an uninstaller.

## Automatic Windows releases

After the Windows x64 VST3 build (Signalsmith ON and OFF) and DSP test suite both pass on `main`, the packaging workflow takes the Signalsmith ON artifact, validates and smoke-tests the installer, then publishes the direct installer EXE, a ZIP copy, and SHA-256 checksums to GitHub Releases. The release version comes from `project(VERSION ...)` in `CMakeLists.txt`; `RELEASE_CHANNEL` selects `alpha`, `beta`, or `stable`. Alpha and beta releases appear as GitHub pre-releases and include the same downloadable Windows installer EXE. Use a new version number for every release, including when promoting a pre-release to stable: published versions and tags are never reused or repurposed. Repeated builds of an already published version do not create duplicate releases. A package-only or manual Actions run cannot publish unless it refers to a successful main-branch Windows VST3 build and the same commit has passing DSP tests.

## 0.9.0 changes

- Carries the metallic dashboard visual update introduced in 0.8.8 forward as the 0.9.0 interface baseline.
- Keeps the graphite/cyan dark theme, satin-aluminum light theme, machined chrome controls, and lighter Turbo Racers Regular rendering.
- No DSP or signal-routing behavior changed from 0.8.8. This release line remains experimental; natural voice quality and Sonar playback are not claimed.

## 0.8.8 changes

- Refined the automotive dashboard finish with brighter chrome-style rotary rings, layered specular highlights, brushed-metal panel shading, and machined button/fader caps.
- Matched dark mode to the graphite-and-cyan dashboard preview. Light mode keeps a white LCD with dark text and satin aluminum controls.
- Kept the embedded Turbo Racers Regular typeface and removed JUCE's extra synthetic bold style so labels read slightly lighter.
- This is a visual update; it does not change the pitch engine or establish natural voice quality.

## 0.8.5 changes

- Added a residual safety path for quiet onsets, unvoiced consonants, intermittent F0 tracking, and fading tails. A fast confidence estimate responds at voiced/unvoiced boundaries while the slower periodicity estimate keeps voiced splitting stable.
- At Voice Mix 100%, the separate delayed dry-reference branch remains out of the blend. If pitch classification is unreliable, the wet hybrid path can still carry unshifted residual audio as a safety fallback; this behavior is independent of Breath, Consonant, and Transient Preserve settings.
- Added deterministic offline regressions for quiet onsets, stale pitch during frication, intermittent F0, fading tails, unvoiced audio at 100% Mix, and neutral level across the Voice Mix range. Linux strict-warning, sanitizer, and Signalsmith-enabled DSP CI passed; Windows x64 VST3 builds passed with Signalsmith ON and OFF.

## 0.8.4 changes

- Embedded the Turbo Racers Regular typeface across the plug-in UI so the automotive lettering renders consistently on systems without the font installed.
- Reworked the dial bezel with a brighter chrome/aluminum ring, a compact cyan value tab on the metal rim, and a slimmer travel arc around the LCD edge.
- Expanded Voice Mode buttons to their full NATURAL / BALANCED / PRECISE labels. Input and Output keep equal widths on the right rail; Mix stays smaller than the matched Tracking and Breath dials, without numeric text inside those dials.

## 0.8.3 changes

- Added an independent **Pitch Dynamics** dial. It changes phrase-relative voiced F0 movement without changing the main Pitch semitone setting.
- Range: 0% = 1.00x contour movement; 100% = 1.45x; default = 75%.
- Uses a slow log-F0 phrase baseline, voiced-confidence weighting, one-frame jump rejection for large F0 excursions, and 45/75 ms smoothing.
- The central pitch monitor plots the adjusted target contour; Tracking and Breath remain independent controls.
- The dark theme now uses a graphite/cyan accent; Pitch Dynamics includes a numeric percentage readout and tooltip.
- Added synthetic tests for neutral contour, upward movement, dynamic span, single-frame octave outliers, and unvoiced release.
- A/B measurement details and caveats: [docs/ROVEE_COMPARISON.md](docs/ROVEE_COMPARISON.md).
- Added dark and light interface themes.
- Added a central three-second monitor for detected input F0 and the pitch-shift target, with gaps during unvoiced sections.
- Added value-range arcs and current-value markers to the pitch, formant, and mix controls.
- Moved advanced processing and de-esser controls into a separate resizable window.
- Changed Voice Mode to a three-state button group.
- Placed the Input and Output gain faders side by side in a taller panel on the right.
- Adjusted Pitch, Formant, and Voice Mix control sizes and positions.
- At Voice Mix 100%, the dry-reference signal is no longer mixed into the output. Breath/residual detail and transients remain controlled by their dedicated settings.
- Uses a fixed Signalsmith processing seed for repeatable output.
- Added regression coverage for parameter-choice mapping, Mix endpoints and residual/transient routing, synthetic voiced/unvoiced/transient signals, and Signalsmith-enabled pitch/latency behavior.

## Controls and signal path

- **Pitch:** fixed semitone shift.
- **Pitch Dynamics:** scales voiced pitch movement relative to a slow local phrase baseline; it is not a static pitch increase or neural speaker-identity conversion.

- **Pitch / Formant:** control pitch and spectral-envelope shift through the optional Signalsmith Stretch engine.
- **Voice Mix:** blends the transformed path with a latency-aligned dry reference. At 100%, the separate dry-reference blend is removed. When pitch or periodicity confidence is low, the hybrid path intentionally retains unshifted residual audio to prevent gaps; this fallback can leave uncertain or unvoiced sounds less shifted.
- **Breath Preserve / Transient Preserve:** control classified aperiodic residual and detected transients. The low-confidence safety fallback is separate from these controls.
- **Voice Mode:** selects one of three interface modes. Dedicated mode-specific DSP behavior is not implemented yet.
- **Input / Output:** input and output gain staging.
- **Advanced:** opens the de-esser and processing settings in a separate window.

The processor analyzes incoming audio, separates a periodic candidate from a residual, transforms the periodic candidate, then aligns and recombines the explicitly controlled residual. The splitter is a first-pass one-period predictive method, not reliable source separation. Signalsmith Stretch input/output latency is used for alignment and host compensation; actual latency and playback should still be checked in the target DAW.

## Validation and limitations

The release workflow requires successful Windows x64 VST3 builds with Signalsmith enabled and disabled, passing DSP tests, a validated installer payload, and a silent install/uninstall smoke test. Releases include a direct installer EXE, ZIP copy and SHA-256 checksum manifest.

The two supplied processed renders measured F0 medians of 244.5 Hz (RoVee) and 248.1 Hz (VoiceMorph 0.8.1). Their 10–90% spans were 3.68 and 2.53 semitones, respectively. The clips are not confirmed as the same dry take or sample-aligned, so the comparison is suggestive rather than controlled. It motivated an adjustable contour control; it does not prove an audio-quality match.

These checks do not establish natural voice quality. Pitch Dynamics is tested against synthetic contours, not the user's real voice. The hybrid pitch/residual path and Signalsmith engine may still sound robotic or warbly. No Cakewalk Sonar / Zoom UAC-232 listening validation has been performed.

## Build on Windows

Requirements: Visual Studio 2022 with the Desktop development with C++ workload, CMake 3.24+, Git, and internet access for JUCE and optional DSP dependencies.

Build with Signalsmith Stretch enabled:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOICEMORPH_ENABLE_SIGNALSMITH=ON -DVOICEMORPH_ENABLE_WORLD=OFF
cmake --build build --config Release --target VoiceMorph_VST3
```

Build without Signalsmith (the voice engine falls back to latency-free dry audio):

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOICEMORPH_ENABLE_SIGNALSMITH=OFF -DVOICEMORPH_ENABLE_WORLD=OFF
cmake --build build --config Release --target VoiceMorph_VST3
```

The VST3 bundle is normally under `build/VoiceMorph_artefacts/Release/VST3/`. Rescan plug-ins in your DAW after placing it in a standard VST3 folder.

## DSP tests

The portable DSP suite does not require JUCE or external downloads:

```powershell
cmake -S tests -B out/tests -DCMAKE_BUILD_TYPE=Release
cmake --build out/tests --config Release
ctest --test-dir out/tests -C Release --output-on-failure
```

The Signalsmith-enabled quality test is configured with `-DVOICEMORPH_TEST_SIGNALSMITH=ON`.

## Third-party components and licensing

Signalsmith Stretch is an optional MIT-licensed dependency. WORLD is an optional modified-BSD analysis dependency and is disabled by default. JUCE is dual-licensed; review the applicable terms in [LICENSING.md](LICENSING.md) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) before redistributing or modifying the project.

See [ENGINE_ARCHITECTURE.md](ENGINE_ARCHITECTURE.md), [BUILD_VALIDATION.md](BUILD_VALIDATION.md), and [PSOLA_EXPERIMENTAL.md](docs/PSOLA_EXPERIMENTAL.md) for implementation notes.
