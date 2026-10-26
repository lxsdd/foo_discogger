#requires -Version 7.2
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Official foobar2000 SDK 2026-10-01 source, already captured by the
# owner's component template. The immutable Git tree is the evidence for
# SOURCE identity, not a false claim of a verified .7z on this CI run.
$SdkVersion = '2026-10-01'
$ExpectedBytes = 793738
$ExpectedSha256 = 'd4c55077336fae81bf8df0259b5b2748fa45ea84132c656ead93eb123cbcdc26'
$ExpectedUrl = "https://www.foobar2000.org/downloads/SDK-$SdkVersion.7z"
$TemplateCommit = '31a35947a7527826c2199491692802e3ce3d8bd7'
$TemplateSdkTree = '2ba8caeb73f72633ad7f60ee5f72ed8074a0eb17'
$TemplateCheckout = Join-Path (Split-Path -Parent $PSScriptRoot) 'external/sdk-template'
$Staging = Join-Path $env:RUNNER_TEMP 'discogger-sdk-probe'
if ($env:GITHUB_ACTIONS -cne 'true' -or [string]::IsNullOrWhiteSpace($env:RUNNER_TEMP)) {
    throw 'Discogger SDK probe requires a disposable GitHub Windows runner.'
}
if (-not (Test-Path -LiteralPath $TemplateCheckout -PathType Container)) {
    throw 'Pinned first-party SDK checkout is missing; cannot fall back to the vendor outage.'
}
$Git = (Get-Command 'git.exe' -ErrorAction Stop).Source
$actualCommit=(& $Git -C $TemplateCheckout rev-parse HEAD).Trim()
$actualTree=(& $Git -C $TemplateCheckout rev-parse 'HEAD:vendor/sdk').Trim()
if ($LASTEXITCODE -ne 0 -or $actualCommit -cne $TemplateCommit -or
    $actualTree -cne $TemplateSdkTree) {
    throw 'Pinned SDK template Git commit/tree mismatch.'
}
$manifest = Get-Content -LiteralPath (Join-Path $TemplateCheckout 'SDK-PIN.json') -Raw | ConvertFrom-Json
if ($manifest.sdk_version -cne $SdkVersion -or
    $manifest.upstream_url -cne $ExpectedUrl -or
    $manifest.upstream_archive_sha256 -cne $ExpectedSha256 -or
    [int64]$manifest.archive_size_bytes -ne $ExpectedBytes) {
    throw 'Pinned SDK template manifest has different upstream archive provenance.'
}
$source = Join-Path $TemplateCheckout 'vendor/sdk'
$Required = @(
    'sdk-license.txt', 'sdk-readme.html',
    'foobar2000/SDK/foobar2000.h',
    'foobar2000/SDK/foobar2000_SDK.vcxproj',
    'foobar2000/helpers/foobar2000_sdk_helpers.vcxproj',
    'foobar2000/foobar2000_component_client/foobar2000_component_client.vcxproj',
    'foobar2000/shared/shared-Win32.lib', 'foobar2000/shared/shared-x64.lib',
    'pfc/pfc.vcxproj', 'libPPUI/libPPUI.vcxproj'
)
foreach ($entry in $Required) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $entry) -PathType Leaf)) {
        throw "Verified SDK template source missing $entry"
    }
}
$readme=Get-Content -LiteralPath (Join-Path $source 'sdk-readme.html') -Raw
$license=Get-Content -LiteralPath (Join-Path $source 'sdk-license.txt') -Raw
if (-not $readme.Contains("foobar2000 SDK, version $SdkVersion") -or
    -not $license.Contains('Redistribution and use in source and binary forms')) {
    throw 'Pinned SDK readme or redistribution license missing.'
}
if (Test-Path -LiteralPath $Staging) { throw 'SDK staging path already exists; refusing to mix contents.' }
$temp=Join-Path $env:RUNNER_TEMP ('discogger-sdk-stage-'+[guid]::NewGuid().ToString('N'))
try {
    New-Item -ItemType Directory -Path $temp -Force | Out-Null
    Copy-Item -Path (Join-Path $source '*') -Destination $temp -Recurse -Force
    foreach ($entry in $Required) {
        if (-not (Test-Path -LiteralPath (Join-Path $temp $entry) -PathType Leaf)) {
            throw "Staged owner SDK source incomplete: $entry"
        }
    }
    Move-Item -LiteralPath $temp -Destination $Staging -ErrorAction Stop
} finally {
    Remove-Item -LiteralPath $temp -Force -Recurse -ErrorAction SilentlyContinue
}
Write-Host "DISCOGGER_SDK_SOURCE_GIT_TREE_VERIFIED=PASS version=$SdkVersion commit=$TemplateCommit tree=$TemplateSdkTree"
Write-Host 'DISCOGGER_SDK_ORIGINAL_ARCHIVE_SHA256=NOT_TESTED_BY_THIS_RUN'
Write-Host 'NOT YET TESTED: full DLL build, real Win32/x64 foobar host and writer tests.'
