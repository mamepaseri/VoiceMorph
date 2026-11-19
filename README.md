# VoiceMorph

**Current package: Prototype 0.8.0 — source archive**

VoiceMorph is an experimental Windows x64 JUCE VST3 / Standalone voice-transformation project. Its target is natural pitch and formant transformation while retaining breaths, sibilants, consonants, and transients.

## Source

The complete 39-file source package is available at [VoiceMorph_0.8.0_source.zip](https://github.com/mamepaseri/VoiceMorph/blob/main/VoiceMorph_0.8.0_source.zip). Extract the archive to obtain the CMake project, C++ sources, tests, CI workflows, and design notes.

## Validation status

The portable DSP suite contains 70 assertions and passed in the development workspace, including strict-warning and AddressSanitizer/UndefinedBehaviorSanitizer builds.

**This is not a release-ready VST3.** The JUCE + Signalsmith plug-in target has not yet been compiled end-to-end in the development workspace. Real Signalsmith pitch/formant quality, actual stream latency, Cakewalk Sonar / Zoom UAC-232 playback, and perceptual performance on dry Japanese speech remain unverified. See `BUILD_VALIDATION.md` and `README.md` inside the source archive for details.

## Build prerequisites

- Windows 11 x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24+
- Git and internet access for CMake FetchContent dependencies

The source pins Signalsmith Stretch to commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`. JUCE 8.0.6 is fetched by CMake and is available under AGPLv3 or a commercial licence; see the licensing notes inside the archive.

## Scope and third-party code

No proprietary reference plug-in binaries or extracted vendor code are included. The included implementations are original project code; third-party libraries are fetched from their upstream sources under their respective licences.

This repository currently publishes the source as a versioned archive. The GitHub repository tree will be expanded to individual source files as the code is prepared for CI and release review.
