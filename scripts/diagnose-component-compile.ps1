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
$project = Join-Path $repo 'foo_discogger.vcxproj'
$wtlRoot = Join-Path $env:RUNNER_TEMP 'discogger-wtl-10.0.9163'
$wtl = @(Get-ChildItem -LiteralPath $wtlRoot -Filter atlapp.h -File -Recurse)
if ($wtl.Count -ne 1) { throw 'Verified WTL headers not staged.' }
$wtlInclude = $wtl[0].DirectoryName
$foobar = Join-Path $staging 'foobar2000'
$includes = @($staging, $foobar, $wtlInclude, $repo, (Join-Path $repo 'zlib'), (Join-Path $repo 'jansson'), (Join-Path $repo 'oauthcpp')) -join ';'
$msbuild = (Get-Command msbuild.exe -ErrorAction Stop).Source
foreach ($platform in $Platforms) {
    Write-Host "=== Discogger first-party C++ compilation diagnostic: $platform ==="
    Write-Host 'Expected: missing third-party headers until pinned dependencies are staged.'
    $output = Join-Path $env:RUNNER_TEMP "discogger-first-party-$platform.log"
    # ClCompile is intentionally not Build: missing link libraries must not mask
    # the first actual component compiler diagnostics.
    & $msbuild $project /t:ClCompile /p:Configuration=Release "/p:Platform=$platform" "/p:AdditionalIncludeDirectories=$includes" /m:1 /v:minimal /nologo 2>&1 | Tee-Object -FilePath $output
    if ($LASTEXITCODE -ne 0) {
        Write-Host "EXPECTED DIAGNOSTIC (not a DLL build): $platform compiler failed. See errors above."
    } else {
        Write-Host "PASS: $platform first-party compilation (link not attempted)."
    }
}
Write-Host 'Diagnostic only. No DLL was linked or published.'
