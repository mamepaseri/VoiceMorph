param(
    [Parameter(Mandatory = $true)]
    [string] $BuildDirectory,
    [string] $OutputDirectory = "dist"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $repoRoot

try {
    $versionLine = Select-String -Path "CMakeLists.txt" -Pattern '^project\(VoiceMorph VERSION ([0-9]+\.[0-9]+\.[0-9]+)'
    if (-not $versionLine) { throw "Could not read VoiceMorph version from CMakeLists.txt." }
    $version = $versionLine.Matches[0].Groups[1].Value

    $dirty = (& git status --porcelain --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw "Could not inspect the working tree state." }
    if (-not [string]::IsNullOrWhiteSpace(($dirty -join "`n"))) {
        throw "Refusing to package corresponding source from a working tree with uncommitted files."
    }

    $commit = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commit -notmatch '^[0-9a-f]{40}$') { throw "Could not resolve the source commit SHA." }

    $buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
    $cachePath = Join-Path $buildRoot "CMakeCache.txt"
    if (-not (Test-Path -LiteralPath $cachePath -PathType Leaf)) { throw "CMakeCache.txt is missing from $buildRoot." }
    $cache = Get-Content -LiteralPath $cachePath -Raw
    if (($cache -notmatch '(?m)^VOICEMORPH_ENABLE_SIGNALSMITH:BOOL=ON$') -or ($cache -notmatch '(?m)^VOICEMORPH_ENABLE_WORLD:BOOL=OFF$')) {
        throw "Corresponding source can only be packaged from the Signalsmith ON / WORLD OFF release build."
    }
    $dependencyRoot = Join-Path $buildRoot "_deps"
    $juceSource = Join-Path $dependencyRoot "juce-src"
    $signalsmithSource = Join-Path $dependencyRoot "signalsmith_stretch-src"
    $signalsmithLinearSource = Join-Path $dependencyRoot "signalsmith-linear-src"
    foreach ($source in @($juceSource, $signalsmithSource, $signalsmithLinearSource)) {
        if (-not (Test-Path -LiteralPath (Join-Path $source "CMakeLists.txt") -PathType Leaf)) {
            throw "Required dependency source was not fetched at the expected path: $source"
        }
    }
    $juceRevision = (& git -C $juceSource rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $juceRevision -notmatch '^[0-9a-f]{40}$') { throw "Could not resolve the fetched JUCE revision." }
    $signalsmithRevision = (& git -C $signalsmithSource rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $signalsmithRevision -notmatch '^[0-9a-f]{40}$') { throw "Could not resolve the fetched Signalsmith revision." }
    $signalsmithLinearRevision = (& git -C $signalsmithLinearSource rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $signalsmithLinearRevision -notmatch '^[0-9a-f]{40}$') { throw "Could not resolve the fetched Signalsmith Linear revision." }
    if (-not (Test-Path -LiteralPath "LICENSE" -PathType Leaf)) { throw "AGPL license text is missing." }
    if (-not (Test-Path -LiteralPath "THIRD_PARTY_NOTICES.md" -PathType Leaf)) { throw "Third-party notices are missing." }

    $outputRoot = Join-Path $repoRoot $OutputDirectory
    New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null
    $archive = Join-Path $outputRoot "VoiceMorph-Source-$version.zip"
    if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }

    $tempRoot = $env:RUNNER_TEMP
    if ([string]::IsNullOrWhiteSpace($tempRoot)) { $tempRoot = $env:TEMP }
    if ([string]::IsNullOrWhiteSpace($tempRoot) -or -not (Test-Path -LiteralPath $tempRoot -PathType Container)) {
        throw "A writable temporary directory is required to stage the corresponding source."
    }
    $stageRoot = Join-Path $tempRoot ("VoiceMorph-source-" + [guid]::NewGuid().ToString("N"))
    $projectRoot = Join-Path $stageRoot "VoiceMorph"
    New-Item -ItemType Directory -Force -Path $projectRoot | Out-Null

    $projectArchive = Join-Path $stageRoot "project.zip"
    & git archive --format=zip --output=$projectArchive $commit
    if ($LASTEXITCODE -ne 0) { throw "Could not archive tracked VoiceMorph source at $commit." }
    Expand-Archive -LiteralPath $projectArchive -DestinationPath $projectRoot
    Remove-Item -LiteralPath $projectArchive -Force

    $thirdPartyRoot = Join-Path $projectRoot "third_party"
    New-Item -ItemType Directory -Force -Path $thirdPartyRoot | Out-Null
    foreach ($dependency in @(
        @{ Source = $juceSource; Destination = (Join-Path $thirdPartyRoot "JUCE") },
        @{ Source = $signalsmithSource; Destination = (Join-Path $thirdPartyRoot "signalsmith-stretch") },
        @{ Source = $signalsmithLinearSource; Destination = (Join-Path $thirdPartyRoot "signalsmith-linear") }
    )) {
        New-Item -ItemType Directory -Force -Path $dependency.Destination | Out-Null
        $excludedDirectories = @(".git")
        if ($dependency.Destination -like "*signalsmith-stretch") {
            # This upstream examples-only submodule is not used by VoiceMorph's plugin build.
            $excludedDirectories += (Join-Path $dependency.Source "cmd\util")
        }
        & robocopy $dependency.Source $dependency.Destination /E /COPY:DAT /R:2 /W:1 /XD $excludedDirectories /XF .git | Out-Null
        if ($LASTEXITCODE -gt 7) { throw "Failed to stage dependency source from $($dependency.Source) (robocopy $LASTEXITCODE)." }
    }

    $requiredFiles = @(
        (Join-Path $projectRoot "LICENSE"),
        (Join-Path $projectRoot "THIRD_PARTY_NOTICES.md"),
        (Join-Path $projectRoot "third_party\JUCE\LICENSE.md"),
        (Join-Path $projectRoot "third_party\signalsmith-stretch\LICENSE.txt"),
        (Join-Path $projectRoot "third_party\signalsmith-linear\LICENSE.txt")
    )
    foreach ($requiredFile in $requiredFiles) {
        if (-not (Test-Path -LiteralPath $requiredFile -PathType Leaf)) {
            throw "A required project or upstream license notice is missing from the source archive: $requiredFile"
        }
    }

    $signalsmithPin = Select-String -Path "CMakeLists.txt" -Pattern '^set\(VOICEMORPH_SIGNALSMITH_GIT_TAG "([0-9a-f]{40})"'
    if (-not $signalsmithPin) { throw "Could not read the pinned Signalsmith revision." }
    if ($signalsmithRevision -ne $signalsmithPin.Matches[0].Groups[1].Value) {
        throw "Fetched Signalsmith Stretch revision differs from the CMake pin."
    }
    $manifest = @(
        "VoiceMorph corresponding source archive",
        "Project version: $version",
        "Source commit: $commit",
        "License: AGPL-3.0-only (see LICENSE)",
        "JUCE: 8.0.6 at $juceRevision, source included under its AGPLv3 option",
        "Signalsmith Stretch: $signalsmithRevision, source included (MIT notice retained)",
        "Signalsmith Linear: 0.3.1 at $signalsmithLinearRevision, transitive source included (MIT notice retained)",
        "Build configuration: Signalsmith Stretch ON, WORLD OFF",
        "Signalsmith Stretch cmd/util examples submodule omitted; not used by the VoiceMorph plugin build",
        "WORLD: disabled for the released VST3 build",
        "Build from this archive with the instructions in README.md. CMake uses the included dependency sources automatically."
    )
    $manifest | Set-Content -LiteralPath (Join-Path $projectRoot "SOURCE-MANIFEST.txt") -Encoding utf8

    Add-Type -AssemblyName System.IO.Compression
    $zip = [System.IO.Compression.ZipFile]::Open($archive, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        Get-ChildItem -LiteralPath $stageRoot -Force -File -Recurse |
            Where-Object { $_.FullName -ne $archive } |
            ForEach-Object {
                $relative = [System.IO.Path]::GetRelativePath($stageRoot, $_.FullName).Replace("\", "/")
                [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                    $zip, $_.FullName, $relative, [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
            }
    }
    finally {
        $zip.Dispose()
    }

    if (-not (Test-Path -LiteralPath $archive -PathType Leaf) -or (Get-Item -LiteralPath $archive).Length -lt 1MB) {
        throw "Corresponding source archive is missing or unexpectedly small."
    }
    if ($env:GITHUB_ENV) { "APP_VERSION=$version" >> $env:GITHUB_ENV }
    Write-Output "Created $archive for commit $commit."
}
finally {
    Pop-Location
    if ($stageRoot -and (Test-Path -LiteralPath $stageRoot)) {
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
}
