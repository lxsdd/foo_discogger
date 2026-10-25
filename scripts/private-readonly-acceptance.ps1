#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Baseline', 'Verify')]
    [string] $Mode,
    [Parameter(Mandatory = $true)]
    [string] $FixtureRoot,
    [Parameter(Mandatory = $true)]
    [string] $ManifestPath,
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^[0-9a-fA-F]{40}$')]
    [string] $SourceSha
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Media, CUE and artwork are only read. Only a separate TEMP copy is allowed.
$fixture = (Resolve-Path -LiteralPath $FixtureRoot -ErrorAction Stop).ProviderPath
$rootInfo = Get-Item -LiteralPath $fixture -Force
if (-not $rootInfo.PSIsContainer -or
    ($rootInfo.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'Fixture must be an ordinary directory, not a file or link.'
}
$fixture = [IO.Path]::GetFullPath($fixture).TrimEnd('\', '/')
$tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\', '/')
$prefix = $tempRoot + [IO.Path]::DirectorySeparatorChar
if (-not $fixture.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
    -not [IO.Path]::GetDirectoryName($fixture).Equals($tempRoot,
        [StringComparison]::OrdinalIgnoreCase)) {
    throw 'REFUSED: fixture must be a direct child of the current user TEMP directory.'
}
$manifest = [IO.Path]::GetFullPath($ManifestPath)
if (-not $manifest.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
    -not [IO.Path]::GetDirectoryName($manifest).Equals($tempRoot,
        [StringComparison]::OrdinalIgnoreCase) -or
    $manifest.StartsWith($fixture + [IO.Path]::DirectorySeparatorChar,
                         [StringComparison]::OrdinalIgnoreCase) -or
    $manifest.Equals($fixture, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'REFUSED: manifest must be a direct child of TEMP, outside the fixture.'
}
if (-not (Test-Path -LiteralPath ([IO.Path]::GetDirectoryName($manifest)) -PathType Container)) {
    throw 'Manifest parent directory does not exist.'
}

# Reject each junction/link BEFORE recursion could traverse outside TEMP.
function Get-OrdinaryFiles([string] $Directory) {
    foreach ($node in (Get-ChildItem -LiteralPath $Directory -Force -ErrorAction Stop)) {
        if ($node.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw 'REFUSED: fixture contains a junction or symbolic link.'
        }
        if ($node.PSIsContainer) {
            Get-OrdinaryFiles $node.FullName
        }
        else {
            $node
        }
    }
}
$files = @(Get-OrdinaryFiles $fixture | Sort-Object -Property FullName)
if ($files.Count -eq 0) { throw 'Fixture contains no files.' }
$rows = @(
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($fixture.Length + 1)
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        [pscustomobject][ordered]@{
            relative = $relative
            length = [long] $file.Length
            mtime_utc_ticks = [long] $file.LastWriteTimeUtc.Ticks
            sha256 = $hash
        }
    }
)
$source = $SourceSha.ToLowerInvariant()
if ($Mode -eq 'Baseline') {
    if (Test-Path -LiteralPath $manifest) {
        throw 'REFUSED: baseline manifest already exists; never overwrite the original.'
    }
    $snapshot = [ordered]@{
        schema = 1
        source_sha = $source
        fixture_root = $fixture
        files = $rows
    }
    $json = ConvertTo-Json -InputObject $snapshot -Depth 6
    [IO.File]::WriteAllText($manifest, $json, [Text.UTF8Encoding]::new($false))
    Write-Host ('DISCOGGER_PRIVATE_FIXTURE_BASELINE=PASS files=' + $rows.Count)
    return
}
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
    throw 'Missing original baseline manifest.'
}
$original = Get-Content -LiteralPath $manifest -Raw -Encoding UTF8 | ConvertFrom-Json
if ($original.schema -ne 1 -or $original.source_sha -cne $source -or
    -not $original.fixture_root.Equals($fixture, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Baseline schema, exact source SHA or fixture directory mismatch.'
}
$expected = @($original.files)
if ($expected.Count -ne $rows.Count) {
    throw 'FAIL: fixture file count changed.'
}
for ($i = 0; $i -lt $rows.Count; $i++) {
    $before = $expected[$i]
    $after = $rows[$i]
    if ($before.relative -cne $after.relative -or
        [long] $before.length -ne $after.length -or
        [long] $before.mtime_utc_ticks -ne $after.mtime_utc_ticks -or
        $before.sha256 -cne $after.sha256) {
        throw ('FAIL: copied fixture changed: ' + $after.relative)
    }
}
Write-Host ('DISCOGGER_PRIVATE_FIXTURE_VERIFY=PASS files=' + $rows.Count)
