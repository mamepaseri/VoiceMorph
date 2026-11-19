# VoiceMorph

VoiceMorph is an experimental Windows x64 JUCE VST3 / Standalone voice-transformation project. The design goal is natural pitch and formant transformation while retaining breaths, sibilants, consonants, and transients.

## Publication status

The repository has been initialized, but the complete source tree has **not yet been imported**. The current development source is available as a Git bundle and source archive from the project maintainer.

The current source revision is Prototype 0.8.0. It contains a JUCE plug-in project shell, a streaming speech analyzer, a first-order periodic/residual splitter, a latency-aligned hybrid path, an optional Signalsmith Stretch adapter, and portable DSP tests.

## Validation limits

The portable DSP suite contains 70 assertions and passed in the development workspace, including strict-warning and AddressSanitizer/UndefinedBehaviorSanitizer builds.

**This is not a release-ready VST3.** The JUCE + Signalsmith plug-in target has not yet been compiled end-to-end in the development workspace. Real Signalsmith pitch/formant quality, actual stream latency, Cakewalk Sonar / Zoom UAC-232 playback, and perceptual performance on dry Japanese speech remain unverified. See `BUILD_VALIDATION.md` in the source package for details.

## Build prerequisites

- Windows 11 x64
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24+
- Git and internet access for CMake FetchContent dependencies

The source pins Signalsmith Stretch to commit `57b93f4e9206a089a45387eaa39bdc9f310d3308`. JUCE 8.0.6 is fetched by CMake and is available under AGPLv3 or a commercial licence; see the licensing notes in the source package.

## Scope

No proprietary reference plug-in binaries or extracted vendor code are included. The project's source uses original implementations; third-party libraries are fetched from their upstream repositories under their respective licences.

The complete source is being prepared for import into this repository. Until then, do not treat this repository's current contents as a buildable project.
