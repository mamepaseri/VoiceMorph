# Distribution compliance and drop-in installation gates

Status: AGPL-3.0-only selected by the project owner on 2026-11-22. License identifiers, a source-packaging script, release-workflow gates, and editor links are now present on this stabilization branch, but none has passed a fresh release-candidate CI run yet. The v0.8.3 tag and release assets remain unchanged; this branch is not a new binary release.

## Release record required for every binary

Record the exact source commit SHA; build workflow/run ID; JUCE version and AGPLv3 route; enabled dependency revisions; source archive and installer/ZIP SHA-256 values; validator and host-test results; and links to LICENSE, third-party notices, and corresponding source. Block publication if source, binary, notices, or hashes do not all identify the same candidate.

## AGPLv3 compliance controls

- Keep the complete AGPLv3 license text and project-level license notices in the source repository and distribution package.
- Confirm all original VoiceMorph contributions can be licensed under AGPL-3.0-only. The accessible commit history reviewed so far shows author identities mamepaseri and codex; check for employer, contractor, and other third-party rights before formal release.
- Build a Corresponding Source archive for the exact released binary. Include VoiceMorph source, CMake/build/install scripts, JUCE 8.0.6 source, Signalsmith Stretch and its transitive Signalsmith Linear source, plus any patches/configuration needed to generate, install, run and modify the plug-in.
- Publish the source archive at no charge beside the installer and ZIP; include a plain source link in release notes, keep it available while offering that binary, and verify the archive hash/commit metadata.
- Preserve upstream conditions and notices for Signalsmith Stretch and Signalsmith Linear (MIT) and WORLD (modified BSD) when enabled. Do not include research-only dependencies unless their exact license/notices have been reviewed.
- Add an accessible About/Licenses view in the plug-in with the appropriate AGPL legal notices, warranty information, and exact-version license/source links.
- Do not ship DRM, terms, or installer behavior that prevents users from modifying and redistributing AGPL-covered builds.
- Do not use the JUCE Starter/Indie/Pro EULA for this same combined VoiceMorph product.

## Drop-in VST3 acceptance

- Install the bundle through one Windows x64 installer to the VST3-standard per-user path by default; offer machine-wide installation only with appropriate elevation. Do not require manual folder copying.
- The current Inno Setup script targets %LOCALAPPDATA%/Programs/Common/VST3 per-user and %ProgramFiles%/Common Files/VST3 machine-wide, matching Steinberg's documented Windows locations. Verify bundle structure and architecture in a clean Windows profile.
- Provide a clean uninstall that removes the VoiceMorph bundle without removing user projects or unrelated plug-ins.
- Test install, DAW launch/scan, plug-in insertion, audio processing, saved project reload, upgrade-over-install, and uninstall in Cakewalk Sonar. A DAW restart or one explicit plug-in rescan may still be needed; do not promise every host will scan immediately.
- Keep SmartScreen/signing state clear. Current README says the installer is unsigned; include and verify SHA-256 values for the exact release files.
- Put the AGPL license and corresponding-source link beside each installer/ZIP and provide the legal notices in the plug-in UI.

## Current findings

- The checked-in installer uses Steinberg's documented per-user/global VST3 locations, providing a good basis for one-installer setup.
- The installer script still copies only payload/VoiceMorph.vst3. A separate no-charge corresponding-source asset is now required by the packaging/release workflow, and exact-version license/source links are present in Advanced Settings. Both controls still need CI/package verification; release-page notes and SHA-256 manifest carry the source link/hash.
- Baseline install instructions require closing the DAW and rescanning afterward. No fresh install/uninstall, Sonar scan/insertion, upgrade, or validator evidence was available.
- The baseline README states that the installer is unsigned and may trigger Windows SmartScreen. Hashes do not replace signing or remove that warning.
- No source archive has been built from the selected AGPL branch yet, so AGPL binary release compliance is not established. The workflow expects the complete JUCE and Signalsmith dependency trees and verifies required license files and source commit before installer packaging.
