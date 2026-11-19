# Licensing decision required before public distribution

This repository contains original VoiceMorph code and optional third-party integrations. The current JUCE-based plug-in target fetches JUCE 8.0.6, whose framework modules are dual-licensed under AGPLv3 or a commercial JUCE licence.

The repository does not yet assign a project-wide licence to the original VoiceMorph source. Do not assume that the code is MIT, BSD, or otherwise permissively licensed. Before making the repository public or distributing binaries, the owner must choose and record one of these routes:

1. **Open-source route:** use JUCE under AGPLv3 and review the obligations for the combined distributed work. Add the complete project licence text and SPDX metadata after that decision.
2. **Commercial/proprietary route:** obtain the appropriate commercial JUCE licence and record the terms that apply to VoiceMorph and its distributions.
3. **Framework migration:** replace JUCE with a framework whose licence matches the intended distribution model, then re-evaluate all dependency notices.

See `THIRD_PARTY_NOTICES.md` for the current dependency inventory. Signalsmith Stretch is MIT-licensed; WORLD is modified BSD. These permissive DSP licences do not override JUCE's licensing terms.
