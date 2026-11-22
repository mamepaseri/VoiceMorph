# Third-party components and license notes

This file records the intended upstream dependencies. The CMake build fetches them from their respective repositories when enabled; their original source and license files remain in the build dependency directories. No vendor VST code or binary is copied into VoiceMorph.

| Component | Upstream | License | Usage / condition |
|---|---|---|---|
| JUCE 8.0.6 | https://github.com/juce-framework/JUCE | AGPLv3 (selected) | Existing plug-in framework, used under AGPLv3 for this product. Preserve JUCE notices and provide its corresponding source with every binary release. Do not use the commercial JUCE EULA for the combined VoiceMorph product. |
| Signalsmith Stretch | https://github.com/Signalsmith-Audio/signalsmith-stretch | MIT | Optional full-band pitch/formant comparison adapter, pinned at `57b93f4e9206a089a45387eaa39bdc9f310d3308`. Preserve upstream copyright/license. |
| Signalsmith Linear | https://github.com/Signalsmith-Audio/linear | MIT | Transitive dependency pinned by the Signalsmith Stretch CMake file at `0.3.1`. Preserve its own notices. |
| WORLD | https://github.com/mmorise/World | Modified BSD | Optional speech analysis backend. Preserve copyright/conditions/disclaimer from upstream LICENSE.txt. |
| Rubber Band | https://github.com/breakfastquay/rubberband | GPL-2.0-or-later or commercial license | Not enabled or linked by default. Only add after the project's source-distribution/commercial-license decision. |
| pbshift | https://github.com/pbtechlab/pbshift | MIT (with separate notices for third-party pieces) | Research benchmark only; not bundled or linked. Review exact revision and third-party notices before adoption. |
| libfvad | https://github.com/dpirch/libfvad | BSD-3-Clause plus repository PATENTS notice | Research candidate only; not bundled or linked. Review patent notice before adoption. |

## Important distinction: project framework vs DSP engines

Open-source DSP components do not automatically make the complete plug-in permissively licensed. The owner selected JUCE's AGPLv3 route for this product. VoiceMorph's complete covered source and all required notices must be provided under AGPLv3, with corresponding source for every binary release. Signalsmith Stretch and WORLD retain their own MIT/modified-BSD notices; they do not replace JUCE/VoiceMorph obligations.

## No proprietary plug-in implementation reused

User-supplied DLL/VST3 files were inspected only for file format, embedded strings, parameter-like names, and high-level workflow clues. No disassembly, decompilation, code extraction, DSP reverse engineering, or proprietary artwork reuse was performed. VoiceMorph's DSP implementations are original wrappers / integration work around the open-source projects listed above.

