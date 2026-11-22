# VoiceMorph licensing

## Selected route

The VoiceMorph project owner selected the GNU Affero General Public License version 3 (AGPL-3.0-only) for the public source repository and the JUCE-based product, effective for the stabilization candidate containing this notice. The exact license text is in LICENSE.

JUCE 8.0.6 is used under its AGPLv3 option, not under the JUCE Starter/Indie/Pro EULA. Do not combine or advertise the two routes for the same product. This route has no JUCE Starter revenue cap, but AGPL obligations apply when source and binaries are conveyed.

## Release requirements

- Keep the complete corresponding source for every binary available at no charge, including the exact VoiceMorph source, build scripts, JUCE source and enabled dependency revisions needed to generate, install, run, and modify the released plug-in.
- Put a clear link to that exact source archive beside every VST3 installer/ZIP and maintain availability while the binary is offered.
- Include LICENSE and THIRD_PARTY_NOTICES in the repository and release materials. Preserve upstream notices and conditions for Signalsmith Stretch (MIT) and WORLD (modified BSD).
- Provide appropriate AGPL legal notices in the plug-in's interactive interface and a visible route to the license and source.
- Do not impose DRM or terms that restrict users' AGPL rights to modify and redistribute the plug-in.

The selected route does not itself validate contributor rights, binary/source correspondence, installer contents, or release workflow. Those are checked per candidate in docs/DISTRIBUTION_COMPLIANCE.md and docs/STABILIZATION_ACCEPTANCE.md.

The existing public v0.8.3 release assets and tag are preserved; this branch does not rebuild or replace them. Future release candidates must pass the AGPL source and notice gates before publication.

See THIRD_PARTY_NOTICES.md for the dependency inventory and docs/LICENSING_ROUTE_REVIEW.md for the route comparison.