$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Same immutable pin independently verified in lxsdd/foobar2000_component_template.
$SdkVersion = '2026-10-01'
$ExpectedBytes = 793738
$ExpectedSha256 = 'd4c55077336fae81bf8df0259b5b2748fa45ea84132c656ead93eb123cbcdc26'
$Archive = Join-Path $env:RUNNER_TEMP "discogger-sdk-$SdkVersion.7z"
$Staging = Join-Path $env:RUNNER_TEMP 'discogger-sdk-probe'
$Url = "https://www.foobar2000.org/downloads/SDK-$SdkVersion.7z"

$SevenZip = (Get-Command '7z.exe' -ErrorAction SilentlyContinue).Source
if (-not $SevenZip) { $SevenZip = Join-Path $env:ProgramFiles '7-Zip\7z.exe' }
if (-not (Test-Path -LiteralPath $SevenZip)) { throw 'Official SDK probe requires 7z.exe.' }

Invoke-WebRequest -Uri $Url -OutFile $Archive -MaximumRedirection 5
$Size = (Get-Item -LiteralPath $Archive).Length
$Sha = (Get-FileHash -LiteralPath $Archive -Algorithm SHA256).Hash.ToLowerInvariant()
if ($Size -ne $ExpectedBytes -or $Sha -ne $ExpectedSha256) {
    throw "SDK archive integrity mismatch: got $Size bytes, $Sha"
}
& $SevenZip t $Archive | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'SDK archive integrity test failed.' }

New-Item -ItemType Directory -Force -Path $Staging | Out-Null
& $SevenZip x $Archive "-o$Staging" -y | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'SDK extraction failed.' }
$Required = @(
    'foobar2000\SDK\foobar2000.h',
    'foobar2000\SDK\foobar2000_SDK.vcxproj',
    'foobar2000\foobar2000_component_client\foobar2000_component_client.vcxproj',
    'pfc\pfc.vcxproj',
    'libPPUI\libPPUI.vcxproj',
    'sdk-readme.html'
)
foreach ($entry in $Required) {
    if (-not (Test-Path -LiteralPath (Join-Path $Staging $entry))) {
        throw "Official SDK archive is missing $entry"
    }
}
$Readme = Get-Content -LiteralPath (Join-Path $Staging 'sdk-readme.html') -Raw
if (-not $Readme.Contains("foobar2000 SDK, version $SdkVersion")) {
    throw 'Extracted SDK does not match version pin.'
}
# Verify SDK-provided linker inputs rather than assuming they are third-party.
# Use a recursive search because the SDK's packaging layout may change.
foreach ($library in @('shared-Win32.lib', 'shared-x64.lib')) {
    $matches = @(Get-ChildItem -LiteralPath $Staging -Filter $library -File -Recurse -ErrorAction Stop)
    if ($matches.Count -ne 1) {
        throw "Expected exactly one $library in official SDK archive; found $($matches.Count)."
    }
    $relative = [System.IO.Path]::GetRelativePath($Staging, $matches[0].FullName)
    Write-Host "SDK linker input verified: $relative"
}
Write-Host "PASS: official foobar2000 SDK $SdkVersion integrity and structure verified."
Write-Host 'NOT YET TESTED: full Discogger build. WTL and third-party dependencies still need reproducible bootstrap.'
