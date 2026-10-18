param(
    [ValidateSet('Win32', 'x64')]
    [string[]] $Platforms = @('Win32', 'x64')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# The preceding probe step downloads, SHA-256 verifies and extracts the official
# 2026-10-01 SDK. Never compile an unverified SDK or use a machine-local SDK.
$staging = Join-Path $env:RUNNER_TEMP 'discogger-sdk-probe'
$projects = @(
    'pfc\\pfc.vcxproj',
    'foobar2000\\SDK\\foobar2000_SDK.vcxproj',
    'foobar2000\\foobar2000_component_client\\foobar2000_component_client.vcxproj',
    'libPPUI\\libPPUI.vcxproj'
)
# libPPUI depends on WTL's atlapp.h, which is not shipped in the SDK.
# Use the exact historical WTL version referenced by Discogger's VS project.
# NuGet verifies the signed package before any of its headers reach MSVC.
$wtlVersion = '10.0.9163'
$wtlUrl = "https://api.nuget.org/v3-flatcontainer/wtl/$wtlVersion/wtl.$wtlVersion.nupkg"
$wtlPackage = Join-Path $env:RUNNER_TEMP "wtl.$wtlVersion.nupkg"
$wtlRoot = Join-Path $env:RUNNER_TEMP "discogger-wtl-$wtlVersion"
Invoke-WebRequest -Uri $wtlUrl -OutFile $wtlPackage -MaximumRedirection 3
$wtlSha256 = (Get-FileHash -LiteralPath $wtlPackage -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host "WTL $wtlVersion NuGet package SHA256: $wtlSha256 (pin after independent verification)"
& dotnet nuget verify --all $wtlPackage
if ($LASTEXITCODE -ne 0) { throw 'WTL NuGet package signature verification failed.' }
[System.IO.Compression.ZipFile]::ExtractToDirectory($wtlPackage, $wtlRoot)
$wtlHeaders = @(Get-ChildItem -LiteralPath $wtlRoot -Filter 'atlapp.h' -File -Recurse)
if ($wtlHeaders.Count -ne 1) { throw "Expected exactly one WTL atlapp.h; found $($wtlHeaders.Count)." }
$wtlInclude = $wtlHeaders[0].DirectoryName
$env:CL = "/I`"$wtlInclude`" " + $env:CL
Write-Host "WTL include staged for MSVC from verified package: $wtlInclude"
$msbuild = (Get-Command 'msbuild.exe' -ErrorAction Stop).Source
foreach ($project in $projects) {
    if (-not (Test-Path -LiteralPath (Join-Path $staging $project))) {
        throw "Verified SDK staging missing project: $project"
    }
}
foreach ($platform in $Platforms) {
    foreach ($project in $projects) {
        $projectPath = Join-Path $staging $project
        Write-Host "Compiling official SDK project: $project ($platform Release)"
        & $msbuild $projectPath /t:Build /p:Configuration=Release "/p:Platform=$platform" /m:1 /v:minimal /nologo
        if ($LASTEXITCODE -ne 0) {
            throw "Official SDK project build failed: $project ($platform), exit $LASTEXITCODE"
        }
    }
    Write-Host "PASS: official SDK projects compiled for $platform."
}
Write-Host 'NOT A COMPONENT BUILD: Discogger third-party dependencies and DLL link remain separate acceptance gates.'
