# Supplied plug-in files: metadata-level analysis

Scope: file format, architecture hints, embedded parameter names/help text and build metadata only. No disassembly, decompilation, code reuse, resource extraction, or copying of proprietary algorithms/assets was performed.

## RoVee.dll
- PE32 / x86 32-bit VST2 binary; file timestamp in PE header: 2013-04-06.
- Embedded VST2 entry point and original project path strings indicate an older VST2-era plug-in.
- Clear parameter names: `Mix`, `Pitch`, `Formant`.
- Preset labels found: `MaleToFemale`, `FemaleToMale`, `Toys`, `Bass`, `Robotic`, `None`.
- The supplied binary is 32-bit; VoiceMorph should be a native x64 VST3 rather than depending on RoVee at runtime.

## GForm.dll
- PE32+ / x64 VST2 binary.
- Embedded UI string identifies it as `GVST GForm ~ Pitch/formant shifter`.
- Visible control string found: `Pitch`; generic `mixDryWet` also appears.
- Useful reference: simple, direct pitch/formant-shifter presentation, but no detailed exposed parameter set was confirmed from the strings alone.

## THE_PITCH 2 MINI.vst3
- PE32+ / x64 VST3, JUCE 9 strings found.
- Parameter-like IDs found: `PitchMini1_amount`, `PitchMini1_key`, `PitchMini1_scale`, `PitchMini1_mode`, `PitchMini1_natural`, `PitchMini1_zero`, `PitchMini1_lean`, `PitchMini1_link`, `PitchMini1_tuning`, `PitchMini1_note`, `PitchMini1_tuneState`, `SignalPath1_dryWet`, `SignalPath1_inputGain`, `SignalPath1_outputGain`, `SignalPath1_saturation`, plus bypass.
- Embedded help text describes `HARD - NATURAL` as a continuum between hard note snapping and allowing slides/vibrato to pass through.
- Embedded help text describes LIVE at about 2 ms and STUDIO as looking about 50 ms ahead for a cleaner result. These are that product's own claims, not VoiceMorph performance promises.
- Useful reference: explicit tracking-behaviour control, an optional quality/latency choice, and visible pitch/key/scale feedback. VoiceMorph will use a timbre-transformation workflow; key/scale correction is optional and not part of the first core design.

## Formant.dll
- PE32+ / x64 DLL, JUCE 8 strings and Minimal Audio account/product URLs found.
- Parameter-/engine-like strings include `FormantShiftSemitones`, `characterGender`, `VoiceCountOption`, `Smooth`, `Mix`, `Dry`, `Wet`, `Frequency Shift`, `Latency`, `Attack`, `Release`, `MKUFormantXYNode_Main`.
- These are embedded strings/symbol-like identifiers, not a verified list of user-facing controls. The file appears to contain broader shared product/engine code, so no more specific feature claim is made.
- Useful reference: independent semitone-based formant shift, smoothing, and the concept of voice-character controls.

## Squash.dll
- PE32+ / x64 DLL, same broad Minimal Audio product/engine family strings found.
- Internal symbols include `MKUSquashCompressorNode_Main`, `AttackCurrentMapping`, `ReleaseCurrentMapping`, `Gain`, `Mix`, and multiband-style threshold/ratio/gain names (`TopThreshold`, `BottomThreshold`, `TopRatio`, `BottomRatio`, `OutputGain`, `BandBypass`).
- These are internal strings, not verified controls of the visible Squash UI. Useful only as evidence that a dynamics module may need attack/release and band-dependent control if VoiceMorph later adds an optional dynamics section.

## T-De-Esser 2.vst3 and T-De-Esser 2.dll
- Both PE32+ / x64; a VST3 build and a VST2/DLL build of the same product family were supplied.
- The VST3 binary contains parameter-like IDs: `Intensity`, `FrequencyRange`, `Mix`, `Quality`, `MakeupGain`, `OutputMode`, `InvertRange`, `LowLatencyMode`, `DryWetMix`, `GainReduction`, and `Solo`.
- Embedded help strings describe the frequency presets: Low-Hi (1–4 kHz), Mid-Hi (2–4 kHz), High (5–10 kHz), and Hi-End (5–20 kHz). They also describe Mix, Makeup Gain, Quality (oversampling/aliasing), Input/Output level, Filter listen, and Difference listen.
- VoiceMorph's initial helper De-Esser borrows the *control concept* (range / intensity / mix / attack / release), not this plug-in's code or sound.

## AmpliTube 5.dll
- PE32+ / x64, approximately 78.7 MB.
- Embedded `Gain`, `GainKnob`, and host/UI framework strings were found. They do not provide a dependable complete list of user-facing controls.
- Used only as general reference for tactile amp-style control hierarchy; the agreed knob appearance still follows the user's supplied Dribbble reference.

## WaveShell1-VST3 17.1_x64.vst3
- PE32+ / x64 Waves shell/container, not an individual Waves effect plug-in.
- Shell strings include VST3 and plugin enumeration infrastructure. No particular Waves processor can be inferred from this file alone.

## Flayr Tune.vst3 (newly supplied 2026-11-19)
- PE32+ / x64 Windows VST3 plug-in binary; embedded JUCE version string: `juce_version_8_0_16`.
- Embedded product and control strings include `Retune Speed`, `Formant Shift`, `Natural Vibrato`, `Vibrato Rate`, `Vibrato Depth`, `Vibrato Onset`, `Mix`, `Key`, `Scale`, `MIDI Target Notes`, `Live Mode`, and `Graph Mode`. Preset labels include `Natural Vocal`, `Hard Tune`, `Subtle Pitch Fix`, `Lush Vibrato`, and `Darker Voice`.
- Embedded help text describes Live as approximately 1.5–10 ms depending on voice and Studio as the higher-quality mode for large shifts. These are vendor/project claims, not VoiceMorph targets.
- The binary embeds `https://github.com/FlayrLabs/flayr-tune`. The public upstream README describes a pitch-correction plug-in, not a general voice-gender/timbre processor. It documents YIN pitch detection, TD-PSOLA in Studio, pitch-synchronous period splicing in Live, and an LPC-based formant path. It also identifies the project as AGPL-3.0. See upstream README for exact controls, implementation explanation, and license terms: https://github.com/FlayrLabs/flayr-tune.
- Useful reference: speech-oriented F0 tracking, pitch-synchronous processing, natural-vibrato preservation, and low-latency vs quality-mode tradeoffs. It is primarily a pitch-correction product; its architecture does not by itself demonstrate the selective breath/sibilance/transient preservation required by VoiceMorph.
- License caution: copying or adapting its source code would introduce AGPL-3.0 obligations. Treat it as an algorithm/architecture reference unless the project explicitly chooses an AGPL-compatible distribution and completes a license review.

## Vocal Bender.dll (newly supplied 2026-11-19)
- PE32+ / x64 Windows DLL, 3.7 MB. Embedded Waves build namespace and PDB path identify the product build as Waves `17.1.42` / `VocalBender`.
- String inspection finds internal names related to pitch, MIDI, modulation, voice mode, keyboard, sampler, LFO/sequence tables, flattening, and mixer controls. These names do not provide a dependable complete list of exposed user-facing parameters.
- The official Waves product description identifies Vocal Bender as a monophonic real-time vocal pitch/formant processor, with modulation options and a Mix control. The official manual documents Pitch, Formant, and Flatten behavior. See https://waves.com/plugins/vocal-bender and https://assets.wavescdn.com/pdf/plugins/vocal-bender.pdf.
- Useful reference: real-time monophonic pitch/formant transformation and modulation concepts. Do not infer specific DSP implementation or behavior solely from these strings.

## Waves processor support DLLs (newly supplied 2026-11-19)
- `AVX512Processor.dll`, `AVXProcessor.dll`, and `GenericMultiCoreProcessor.dll` are all PE32+ / x64. Their strings and imports identify SIMD processor variants and shared Waves matrix/mixer routines; the AVX variants import `mkl_waves.dll`, `libmmd.dll`, and `svml_dispmd.dll`.
- These are processor/backend DLLs, not standalone VST plug-ins. The direct import table of `Vocal Bender.dll` does not prove that each of these three DLLs is loaded by Vocal Bender specifically, so their exact runtime relationship is not asserted.

## unins000.exe / unins000.dat
- The `.dat` file identifies itself as an Inno Setup uninstall log for `Flayr Tune`.
- At the time of the original inspection these were the only Flayr-related files supplied; the actual `Flayr Tune.vst3` binary has since been supplied and is analyzed above.

## Design conclusion

VoiceMorph's parameter system is based on three independent groups:
1. **Voice transform:** Voice Mix, Pitch, Formant, Tracking, Voice Mode.
2. **Preservation:** Breath, Consonant Preserve, Transient Preserve.
3. **Sibilance cleanup:** De-Esser Frequency Range, Intensity, Mix, Attack, Release.

Input/Output gain is a separate utility strip on the left. The waveform monitor shows the input and output traces. The main Pitch/Formant algorithm remains a separate implementation task; no parameter-string inspection can recover or validate a vendor's DSP algorithm.

## Open-source engine candidates (researched 2026-11-19)

The following are public components evaluated for VoiceMorph. Their code is obtained from upstream at configure time when enabled; no upstream source is vendored into this prototype archive.

- **Signalsmith Stretch (MIT):** optional full-band baseline adapter. Useful to establish a real-time pitch/formant comparison. Its own README notes that its formant correction is less sharp than monophonic methods such as PSOLA and needs an approximate F0 base. See `Source/Dsp/SignalsmithPitchEngine.*`.
- **WORLD (modified BSD):** optional voice analysis backend. The current wrapper returns DIO/Harvest F0 candidates, StoneMask-refined F0 for the DIO path, CheapTrick spectral envelope, and D4C aperiodicity. See `Source/Dsp/WorldSpeechAnalyzer.*`. This is worker-thread analysis only; it is not yet a full streaming transform/synthesis pipeline.
- **Rubber Band (GPL-2.0-or-later / commercial license):** not included by default. It has independent pitch and formant scaling, but the official LiveShifter documentation says the delay is 50 ms or more depending on configuration. It is a possible benchmark only if the source-distribution license route is settled.
- **pbshift (MIT):** experimental benchmark candidate only. Its own repository describes itself as pre-1.0 / actively developed and publishes self-reported latency/quality values; we did not adopt it as a production dependency.
- **libfvad (BSD-3-Clause):** possible speech-activity gate for tracker hold/reset behavior; it is not a voiced/unvoiced or sibilance separator. The upstream PATENTS notice also needs a separate review before use.

See `ENGINE_ARCHITECTURE.md` and `THIRD_PARTY_NOTICES.md` for role and license notes.
