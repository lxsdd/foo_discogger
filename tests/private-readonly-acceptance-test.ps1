#requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script = Join-Path $PSScriptRoot '../scripts/private-readonly-acceptance.ps1'
$fixture = Join-Path $env:TEMP ('discogger-private-fixture-' + [guid]::NewGuid().ToString('N'))
$manifest = Join-Path $env:TEMP ('discogger-private-baseline-' + [guid]::NewGuid().ToString('N') + '.json')
$sha = '0123456789abcdef0123456789abcdef01234567'
function Expect-Rejected([scriptblock] $Action) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'Negative private fixture audit was incorrectly accepted.' }
}
try {
    $null = New-Item -ItemType Directory -Path $fixture
    $sub = Join-Path $fixture 'synthetic-album'
    $null = New-Item -ItemType Directory -Path $sub
    $audio = Join-Path $sub 'synthetic.mp3'
    $cue = Join-Path $sub 'synthetic.cue'
    $art = Join-Path $sub 'cover.jpg'
    [IO.File]::WriteAllBytes($audio, [byte[]]@(0,1,2,3,4,5))
    [IO.File]::WriteAllText($cue, 'FILE "synthetic.mp3" MP3')
    [IO.File]::WriteAllBytes($art, [byte[]]@(255,216,255,217))
    & $script -Mode Baseline -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    # A second baseline must not overwrite the first manifest, even when
    # the caller mistakenly reuses its path.
    $manifestHash = (Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash
    Expect-Rejected { & $script -Mode Baseline -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
    if ((Get-FileHash -LiteralPath $manifest -Algorithm SHA256).Hash -cne $manifestHash) {
        throw 'Rejected baseline attempt changed the original manifest.'
    }
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha ('a' * 40) }
    $mtime = (Get-Item -LiteralPath $audio).LastWriteTimeUtc
    [IO.File]::WriteAllBytes($audio, [byte[]]@(0,1,2,3,4,6))
    (Get-Item -LiteralPath $audio).LastWriteTimeUtc = $mtime
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
    [IO.File]::WriteAllBytes($audio, [byte[]]@(0,1,2,3,4,5))
    (Get-Item -LiteralPath $audio).LastWriteTimeUtc = $mtime
    & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    $extra = Join-Path $fixture 'unexpected.txt'
    [IO.File]::WriteAllText($extra, 'unexpected')
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
    Remove-Item -LiteralPath $extra -Force
    & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    # A removed file and a timestamp-only mutation are independent failure
    # modes; neither may be accepted just because remaining hashes match.
    $artBytes = [IO.File]::ReadAllBytes($art)
    $artMtime = (Get-Item -LiteralPath $art).LastWriteTimeUtc
    Remove-Item -LiteralPath $art -Force
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
    [IO.File]::WriteAllBytes($art, $artBytes)
    (Get-Item -LiteralPath $art).LastWriteTimeUtc = $artMtime
    & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    $cueMtime = (Get-Item -LiteralPath $cue).LastWriteTimeUtc
    (Get-Item -LiteralPath $cue).LastWriteTimeUtc = $cueMtime.AddSeconds(2)
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
    (Get-Item -LiteralPath $cue).LastWriteTimeUtc = $cueMtime
    & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    # A corrupt baseline must fail closed rather than be treated as an empty
    # list or silently replaced by a new baseline.
    $manifestBytes = [IO.File]::ReadAllBytes($manifest)
    try {
        [IO.File]::WriteAllText($manifest, '{"schema":1,"source_sha":"invalid"}')
        Expect-Rejected { & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
    }
    finally { [IO.File]::WriteAllBytes($manifest, $manifestBytes) }
    & $script -Mode Verify -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $env:ProgramFiles -ManifestPath $manifest -SourceSha $sha }
    Write-Host 'PASS: private copied-fixture baseline/no-op, hash/mtime-only, missing/new files, corrupt manifest, source mismatch, no-overwrite and non-TEMP rejection.'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
    if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest -Force }
}
