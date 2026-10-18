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
