[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$QtPrefix,
    [string]$BuildDirectory = "",
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$qtDeploy = Join-Path $QtPrefix "bin\windeployqt.exe"

if ([string]::IsNullOrWhiteSpace($BuildDirectory)) {
    $BuildDirectory = Join-Path $root "build-release"
}
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $PSScriptRoot "out\VPet"
}

foreach ($path in @($QtPrefix, $qtDeploy, (Join-Path $root "Animation"), (Join-Path $root "GPT-SoVITS"), (Join-Path $root "tts_config.json"))) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required release input is missing: $path" }
}
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Release output already exists; choose a new output directory: $OutputDirectory"
}

cmake -S $root -B $BuildDirectory -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_PREFIX_PATH=$QtPrefix
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
cmake --build $BuildDirectory --config Release
if ($LASTEXITCODE -ne 0) { throw "Release build failed" }

$exeCandidates = @(
    (Join-Path $BuildDirectory "VPet.exe"),
    (Join-Path $BuildDirectory "Release\VPet.exe")
)
$exePath = $exeCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if ($null -eq $exePath) { throw "VPet.exe was not produced by the release build" }

New-Item -ItemType Directory -Path $OutputDirectory | Out-Null
Copy-Item -LiteralPath $exePath -Destination (Join-Path $OutputDirectory "VPet.exe") -Force
Copy-Item -LiteralPath (Join-Path $root "Animation") -Destination (Join-Path $OutputDirectory "Animation") -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root "GPT-SoVITS") -Destination (Join-Path $OutputDirectory "GPT-SoVITS") -Recurse -Force
Copy-Item -LiteralPath (Join-Path $root "tts_config.json") -Destination (Join-Path $OutputDirectory "tts_config.json") -Force

# TEMP may include disposable test output; the reference voice is the only required asset.
$releaseTemp = Join-Path $OutputDirectory "GPT-SoVITS\TEMP"
Get-ChildItem -LiteralPath $releaseTemp -Force | Where-Object { $_.Name -ne "ref_audio.wav" } | Remove-Item -Recurse -Force

& $qtDeploy --release --no-translations --compiler-runtime (Join-Path $OutputDirectory "VPet.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

& (Join-Path $PSScriptRoot "Test-Release.ps1") -ReleaseDirectory $OutputDirectory
if ($LASTEXITCODE -ne 0) { throw "Release self-check failed" }

Write-Host "Release directory created: $OutputDirectory"
