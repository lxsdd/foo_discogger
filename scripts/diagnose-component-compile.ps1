param(
    [ValidateSet('Win32','x64')]
    [string[]] $Platforms = @('Win32','x64')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Compile-only first diagnostic: do not link, deploy, publish or modify a host.
# The SDK and WTL have already been independently verified by the preceding steps.
$staging = Join-Path $env:RUNNER_TEMP 'discogger-sdk-probe'
$repo = Split-Path -Parent $PSScriptRoot
$componentRoot = Join-Path $staging 'foobar2000/foo_discogger'
if (Test-Path -LiteralPath $componentRoot) { throw 'Refusing to overwrite existing staged component directory.' }
New-Item -ItemType Directory -Path $componentRoot -Force | Out-Null
# Preserve the historical relative paths: ../helpers, ../SDK, ../../pfc, ../../libPPUI.
# Only first-party source is copied into the isolated SDK workspace.
Copy-Item -Path (Join-Path $repo '*') -Destination $componentRoot -Recurse -Force
$project = Join-Path $componentRoot 'foo_discogger.vcxproj'
# Pinned upstream source is staged outside the repository; no vendor files are published.
$oauthRevision = '2893f1bf42e1dfd5acf0dd849519cf27ac9c7395'
$oauthRoot = Join-Path $env:RUNNER_TEMP 'discogger-liboauthcpp'
& git clone --quiet --no-checkout https://github.com/sirikata/liboauthcpp.git $oauthRoot
if ($LASTEXITCODE -ne 0) { throw 'OAuth source clone failed.' }
& git -C $oauthRoot checkout --quiet --detach $oauthRevision
if ($LASTEXITCODE -ne 0) { throw 'Pinned OAuth revision unavailable.' }
$actualRevision = (& git -C $oauthRoot rev-parse HEAD).Trim()
if ($actualRevision -ne $oauthRevision) { throw 'OAuth revision mismatch.' }
if (-not (Test-Path -LiteralPath (Join-Path $oauthRoot 'LICENSE'))) { throw 'OAuth license missing.' }
$oauthInclude = Join-Path $oauthRoot 'include'
if (-not (Test-Path -LiteralPath (Join-Path $oauthInclude 'liboauthcpp/liboauthcpp.h'))) { throw 'OAuth header missing.' }
Write-Host "OAuth source pinned and license present: $actualRevision"
# CL is honored by cl.exe; AdditionalIncludeDirectories is item metadata, not a reliable global MSBuild property.
$env:CL = '/I"' + $oauthInclude + '" ' + $env:CL

$wtlRoot = Join-Path $env:RUNNER_TEMP 'discogger-wtl-10.0.9163'
$wtl = @(Get-ChildItem -LiteralPath $wtlRoot -Filter atlapp.h -File -Recurse)
if ($wtl.Count -ne 1) { throw 'Verified WTL headers not staged.' }
$wtlInclude = $wtl[0].DirectoryName
$env:CL = '/I"' + $wtlInclude + '" ' + $env:CL
$foobar = Join-Path $staging 'foobar2000'
$includes = @($staging, $foobar, $wtlInclude, $oauthInclude, $componentRoot, (Join-Path $componentRoot 'zlib'), (Join-Path $componentRoot 'jansson'), (Join-Path $componentRoot 'oauthcpp')) -join ';'
$msbuild = (Get-Command msbuild.exe -ErrorAction Stop).Source
foreach ($platform in $Platforms) {
    Write-Host "=== Discogger first-party C++ compilation diagnostic: $platform ==="
    Write-Host 'Expected: missing third-party headers until pinned dependencies are staged.'
    $output = Join-Path $env:RUNNER_TEMP "discogger-first-party-$platform.log"
    # ClCompile is intentionally not Build: missing link libraries must not mask
    # the first actual component compiler diagnostics.
    $global:LASTEXITCODE = 0
    & $msbuild $project /t:ClCompile /p:Configuration=Release "/p:Platform=$platform" "/p:AdditionalIncludeDirectories=$($includes.Replace(';', '%3B'))" /m:1 /v:minimal /nologo 2>&1 | Tee-Object -FilePath $output
    if ($LASTEXITCODE -ne 0) {
        Write-Host "EXPECTED DIAGNOSTIC (not a DLL build): $platform compiler failed. See errors above."
    } else {
        Write-Host "PASS: $platform first-party compilation (link not attempted)."
    }
}
Write-Host 'Diagnostic only. No DLL was linked or published.'
exit 0
