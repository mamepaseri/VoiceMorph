# VoiceMorph — JUCE VST3 Prototype 0.8.1

VoiceMorph is an experimental Windows x64 VST3 voice-transformation plug-in. It uses pitch/formant processing and a hybrid periodic/residual signal path. This is an early prototype: transformed speech can still sound robotic or warbly.

## Download

Get the latest Windows x64 installer from [GitHub Releases](https://github.com/mamepaseri/VoiceMorph/releases/latest). Download the `VoiceMorph-Setup-x64-<version>.zip` asset, extract it, then run the included installer. The installer offers per-user installation by default; close your DAW before installing or uninstalling, then rescan its VST3 plug-ins.

The installer is unsigned, so Windows SmartScreen may show an unfamiliar-app warning. The installer workflow checks the VST3 payload and performs a silent install/uninstall smoke test.

## Automatic Windows releases

After a successful Windows x64 VST3 build on `main`, the packaging workflow validates and smoke-tests the installer, then publishes a versioned ZIP to GitHub Releases. The release version comes from `project(VERSION ...)` in `CMakeLists.txt`. Existing tags are never overwritten; increment the project version before merging work intended for a new public release. A package-only or manual Actions run cannot publish unless it refers to a successful main-branch Windows VST3 build.

## 0.8.1 changes

- Voice Mode is selected with a three-state button.
- Advanced de-esser settings open in a separate panel.
- Input and Output gain faders sit side by side in a taller panel on the right.
- Pitch, Formant, and Voice Mix control sizes and positions were adjusted.
- At Voice Mix 100%, the dry-reference signal is no longer mixed into the output. Breath/residual detail and transients can still be preserved through their dedicated controls.
- Signalsmith processing uses a fixed deterministic seed for repeatable output.
- Regression coverage includes parameter-choice mapping, Mix endpoints and residual/transient routing, synthetic voiced/unvoiced/transient signals, and a Signalsmith-enabled pitch/latency quality check.

## Controls and signal path

- **Pitch / Formant:** control pitch and spectral-envelope shift through the optional Signalsmith Stretch engine.
- **Voice Mix:** blend the processed voice path with the delayed dry reference. At 100%, only transformed audio and explicitly preserved residual detail are routed.
- **Breath Preserve / Transient Preserve:** control aperiodic residual and detected transient content on the wet path.
- **Voice Mode:** selects one of three UI modes. Dedicated mode-specific DSP behavior is not implemented yet.
- **Input / Output:** input and output gain staging.
- **Advanced:** opens the de-esser controls.

The processor analyzes incoming audio, separates a periodic candidate from a residual, transforms the periodic candidate, then aligns and recombines the explicitly controlled residual. The splitter is a first-pass one-period predictive method, not reliable source separation. Signalsmith Stretch input/output latency is used for alignment and host compensation; actual latency and playback should still be checked in the target DAW.

## Validation and limitations

GitHub Actions successfully built the Windows x64 VST3 with Signalsmith enabled and disabled. The DSP suite passed with strict warnings, AddressSanitizer/UndefinedBehaviorSanitizer, and the Signalsmith-enabled synthetic quality test. The installer workflow passed payload validation and install/uninstall smoke tests.

These checks do not establish natural voice quality. The pitch/residual algorithm may still produce robotic or warbly artifacts, especially on real speech. No Sonar listening validation has been performed. Parameter value mapping is covered by focused tests, but full APVTS state save/restore is not covered by a dedicated round-trip regression test.

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
