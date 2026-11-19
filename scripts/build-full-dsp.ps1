$ErrorActionPreference = 'Stop'
cmake --preset windows-vs2022-full-dsp
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed ($LASTEXITCODE)" }
cmake --build --preset build-full-dsp-release --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed ($LASTEXITCODE)" }
Write-Host "VST3 output: out/build/windows-vs2022-full-dsp/VoiceMorph_artefacts/Release/VST3/VoiceMorph.vst3"
