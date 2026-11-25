# Voice quality candidate: 0.9.1

## Findings

The 0.9.0 hybrid path preserved a confidence-based residual as a safety path. When the pitch tracker became uncertain, part of the source voice therefore bypassed the pitch shifter. At 100% Voice Mix this could combine an unshifted vocal component with the transformed component and sound doubled. This protection was valuable against dropouts, but it did not distinguish uncertain voiced material from detail that should remain unshifted.

The Signalsmith adapter also applied pitch transposition without pitch-compensated formant handling. A pitch rise could therefore move the vocal envelope along with the fundamental even when the Formant control was neutral, making the result thinner or more synthetic. The phrase baseline could additionally start from a weak first estimate, and Pitch Dynamics expanded contours substantially by default.

## Candidate changes

- Route uncertain residual material through the pitch/formant transform. Keep only the separately classified breath, consonant, and transient contributions on the residual path.
- Turn on the Signalsmith compensation option so the pitch shift does not itself drag the formant envelope. The user-facing Formant value continues to control intentional envelope movement.
- Seed the formant reference after six consistent, confident F0 frames; use a neutral reference before then.
- Default Pitch Dynamics to neutral and cap the optional expansion at 1.25x.
- Keep the 0.9.0 editor sources and GUI assets unchanged.

## Verification limits

The DSP regression uses deterministic synthetic signals and a scaled mock transform to detect unshifted source leakage. It also checks onset/tail continuity, Voice Mix endpoints, and pitch-baseline startup. CI build and test results must pass before distribution. Listening in Cakewalk Sonar and Ableton Live 12 with a real microphone is still needed to judge naturalness and latency in a host.
