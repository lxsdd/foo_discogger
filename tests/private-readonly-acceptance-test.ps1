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
    Expect-Rejected { & $script -Mode Baseline -FixtureRoot $fixture -ManifestPath $manifest -SourceSha $sha }
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
    Expect-Rejected { & $script -Mode Verify -FixtureRoot $env:ProgramFiles -ManifestPath $manifest -SourceSha $sha }
    Write-Host 'PASS: private copied-fixture baseline/no-op, SHA/mtime changes, new files, source mismatch, overwrite and non-TEMP rejection.'
}
finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
    if (Test-Path -LiteralPath $manifest) { Remove-Item -LiteralPath $manifest -Force }
}
