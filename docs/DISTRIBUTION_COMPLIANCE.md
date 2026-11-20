# Distribution compliance and drop-in installation gates

Status: proposal for the 0.9.0 stabilization branch; no license route has been elected and no source-code release fix has been integrated.

## Non-negotiable release record

Every binary release must record: exact source commit SHA; the chosen JUCE route (Starter EULA or AGPLv3); license owner; release date; JUCE version; enabled optional dependencies; installer/ZIP SHA-256; build and validation results; and source/notice links. Release publication must stop when the license route is blank or its requirements have not been attested.

Do not infer an open-source license from public GitHub access. A LICENSE file and matching SPDX notices must explicitly identify the project license. The two JUCE routes are not to be mixed for the same combined product.

## Route-specific evidence

### JUCE Starter EULA

- Record the legal person/entity licensing JUCE and the Framework Users who contribute to or modify JUCE-dependent work; verify required seats under the current EULA.
- Record the owner's previous 12-month revenue/funding scope and verify it remains within the applicable $20,000 Starter limit before each binary release.
- Record the major JUCE version covered by the perpetual license. Do not treat free Starter as a source license for VoiceMorph or a grant of downstream reuse rights.
- Keep the publicly readable VoiceMorph source's license status clear. Preserve JUCE and all dependency notices in installer/release materials.
- Do not apply AGPLv3 to the same JUCE-combined product under this route without a reviewed licensing basis.

### JUCE under AGPLv3

- Add the complete AGPLv3 license text and project copyright/license notices; verify that every VoiceMorph contribution can be licensed this way.
- For every binary, publish complete Corresponding Source for the exact commit, including the source/build files and scripts needed to configure, build and install it. Pin and expose the JUCE 8.0.6 source and exact enabled dependency revisions; preserve compatible MIT/BSD notices.
- Place an equivalent, no-charge source download/link beside the VST3 installer and ZIP, and keep the source available while the binary is offered.
- Add an easy-to-find in-plugin About/Licenses view with the appropriate AGPL notices, warranty statement, and link/location for the license and source.
- Keep DRM/technical restrictions from preventing recipient rights granted by AGPLv3; confirm installer/update mechanisms do not block modified builds.

## Drop-in VST3 acceptance

- Install the bundle through one Windows x64 installer to the VST3-standard per-user path by default; offer machine-wide installation only with appropriate elevation. Do not require manual folder copying.
- The baseline installer script targets %LOCALAPPDATA%/Programs/Common/VST3 for per-user setup and %ProgramFiles%/Common Files/VST3 for all-user setup, matching Steinberg's documented Windows VST3 locations. Verify the final bundle structure and architecture in a clean Windows user profile.
- Provide a clean uninstall that removes the bundle but does not remove user projects or unrelated plug-ins.
- Test install, launch, host scan, insertion, audio pass-through/processing, saved project reload, upgrade-over-install, and uninstall in Cakewalk Sonar. A DAW restart or one explicit plug-in rescan may still be needed; do not promise that an installer can force every host to scan immediately.
- Keep the SmartScreen/signing state clear. Current README states the installer is unsigned; include the SHA-256 manifest and verify hashes against the exact release files.
- If AGPLv3 is selected, the installer/About page and adjacent release page must expose the correct license and exact source download for that binary.

## Current findings

- The checked-in Inno Setup script installs the VST3 bundle to the documented per-user/global VST3 locations, which is a good basis for one-installer use.
- The script currently copies only payload/VoiceMorph.vst3 into the destination. It does not visibly package third-party/license notices or AGPL source links; those need an installer/release design before AGPL binary distribution.
- Baseline documentation says to close the DAW, install, then rescan its VST3 plug-ins. No fresh install/uninstall, host scan, or DAW insertion result was available in this review.
- The baseline README states the installer is unsigned and may trigger Windows SmartScreen. Hashing does not replace signing or remove that warning.
- A draft checklist cannot establish actual compliance: the selected route, eligibility/rights evidence, package contents, hashes, and validation run must be attached to each release candidate.