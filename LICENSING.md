# VoiceMorph licensing route — decision pending

This repository is public and the project has a public v0.8.3 prerelease. Public visibility alone does not grant downstream users a licence to reuse the original VoiceMorph source. This file records the current status; it does not make a license election.

The current JUCE-based plug-in target fetches JUCE 8.0.6. JUCE is dual-licensed under AGPLv3 or the JUCE 8 EULA. The owner’s interim direction is to keep this repository public and pursue a no-cost route.

Two routes are being evaluated:

1. **JUCE Starter EULA:** the current JUCE 8 terms list this as free/perpetual with a $20,000 annual revenue/funding limit. Verify the license owner’s revenue/funding scope, required Framework User seats, and exact product distribution terms. This route is not an AGPL license for VoiceMorph; keep the source license status explicit. See docs/LICENSING_ROUTE_REVIEW.md.
2. **JUCE under AGPLv3:** requires an explicit AGPLv3 license for the complete covered VoiceMorph work, appropriate rights from contributors, required notices, and corresponding source delivery for every binary release. See docs/LICENSING_ROUTE_REVIEW.md.

These are alternative licensing routes for the JUCE-based product. The JUCE 8 EULA restricts open-source licensing that would require JUCE or software combined with JUCE to be source-disclosed, derivative-work licensed, or freely redistributable. Do not combine the JUCE commercial EULA route with an AGPL license for the same combined product without a reviewed basis.

Until the owner confirms eligibility for Starter or elects AGPLv3 and the repository/release materials are made consistent with that route, no formal GA or further binary release is approved by this project checklist. This status does not retroactively alter the existing public v0.8.3 prerelease.

See THIRD_PARTY_NOTICES.md for the dependency inventory. Signalsmith Stretch is MIT-licensed and WORLD is modified BSD; their notices and conditions must be preserved. Recheck the actual enabled dependency set when building a release.