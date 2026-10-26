$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
. (Join-Path (Split-Path -Parent $PSScriptRoot) 'scripts/normalize-native-bitmaps.ps1')
Add-Type -AssemblyName System.Drawing

$folder = Join-Path ([IO.Path]::GetTempPath()) ([IO.Path]::GetRandomFileName())
New-Item -ItemType Directory -Path $folder -Force | Out-Null
try {
    $image = [System.Drawing.Bitmap]::new(2,2,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    try {
        $image.SetPixel(0,0,[System.Drawing.Color]::FromArgb(64,7,22,33))
        $image.SetPixel(1,0,[System.Drawing.Color]::FromArgb(255,41,52,63))
        $image.SetPixel(0,1,[System.Drawing.Color]::FromArgb(128,70,80,90))
        $image.SetPixel(1,1,[System.Drawing.Color]::FromArgb(0,0,0,0))
        $fakeBmp = Join-Path $folder 'bitmap_tracer_ext_drk.bmp'
        # Deliberately misleading extension, as seen in the historical resource.
        $image.Save($fakeBmp,[System.Drawing.Imaging.ImageFormat]::Png)
        $original = [IO.File]::ReadAllBytes($fakeBmp)
        if ((Get-DiscoggerBitmapFormat $original) -ne 'PNG') { throw 'PNG sniff failed' }
        $status = Convert-DiscoggerResourceBitmap -Path $fakeBmp -TemporaryRoot $folder
        if ($status -ne 'normalized') { throw 'PNG-in-BMP was not converted' }
        $converted = [IO.File]::ReadAllBytes($fakeBmp)
        if ((Get-DiscoggerBitmapFormat $converted) -ne 'BMP') { throw 'BMP signature missing' }
        if ([BitConverter]::ToUInt32($converted,14) -ne 40) { throw 'BMP DIB not v3' }
        if ([BitConverter]::ToUInt32($converted,10) -ne 54) { throw 'BMP pixel offset wrong' }
        if ($converted.Length -ne (54 + (2*2*4))) { throw 'BMP pixel byte length wrong' }
        # Top-left pixel resides in second row of bottom-up BMP, preserving BGRA.
        $offset = 54 + 2*4
        if ($converted[$offset] -ne 33 -or $converted[$offset+1] -ne 22 -or
            $converted[$offset+2] -ne 7 -or $converted[$offset+3] -ne 64) {
            throw 'RGBA transparency lost in serialized BMP'
        }
        $sha1 = (Get-FileHash -LiteralPath $fakeBmp -Algorithm SHA256).Hash
        if ((Convert-DiscoggerResourceBitmap -Path $fakeBmp -TemporaryRoot $folder) -ne 'unchanged') {
            throw 'Legacy BMP should remain unchanged on second pass'
        }
        if ((Get-FileHash -LiteralPath $fakeBmp -Algorithm SHA256).Hash -ne $sha1) {
            throw 'No-op unexpectedly changed BMP'
        }
    } finally { $image.Dispose() }

    $invalid = Join-Path $folder 'invalid.bmp'
    [IO.File]::WriteAllBytes($invalid,[byte[]]@(1,2,3,4,5,6))
    $oldHash = (Get-FileHash -LiteralPath $invalid -Algorithm SHA256).Hash
    $rejected = $false
    try { $null = Convert-DiscoggerResourceBitmap -Path $invalid -TemporaryRoot $folder }
    catch { $rejected = $true }
    if (-not $rejected) { throw 'Invalid binary was accepted' }
    if ((Get-FileHash -LiteralPath $invalid -Algorithm SHA256).Hash -ne $oldHash) {
        throw 'Invalid binary was modified before rejection'
    }

    # Recognized but corrupt PNG must also fail without changing the input.
    $badPng = Join-Path $folder 'bad.png'
    [IO.File]::WriteAllBytes($badPng,[byte[]]@(137,80,78,71,13,10,26,10,0,0,0))
    $badHash = (Get-FileHash -LiteralPath $badPng -Algorithm SHA256).Hash
    $rejected = $false
    try { $null = Convert-DiscoggerResourceBitmap -Path $badPng -TemporaryRoot $folder }
    catch { $rejected = $true }
    if (-not $rejected) { throw 'Corrupt PNG was accepted' }
    if ((Get-FileHash -LiteralPath $badPng -Algorithm SHA256).Hash -ne $badHash) {
        throw 'Corrupt PNG source modified before rejection'
    }
    # Regression against the actual historical resource, not just synthetic
    # PNG fixtures: two accidental UTF-8 BOM markers precede a valid BMP.
    $historicalResource = Join-Path (Split-Path -Parent $PSScriptRoot) 'ico/bitmap_tracer_ext_drk.bmp'
    $historicHash = (Get-FileHash -LiteralPath $historicalResource -Algorithm SHA256).Hash
    $historicBytes = [IO.File]::ReadAllBytes($historicalResource)
    if ($historicBytes.Length -ne 1150) { throw 'Historic BMP byte count changed unexpectedly' }
    if (([BitConverter]::ToString($historicBytes,0,8)) -ne 'EF-BB-BF-EF-BB-BF-42-4D') {
        throw 'Historic double-BOM signature changed'
    }
    $expectedClean = [byte[]]::new($historicBytes.Length - 6)
    [Array]::Copy($historicBytes,6,$expectedClean,0,$expectedClean.Length)
    if ([BitConverter]::ToUInt32($expectedClean,2) -ne $expectedClean.Length) {
        throw 'Historic BMP header length mismatch'
    }
    $tempHistorical = Join-Path $folder 'real-source-bom.bmp'
    Copy-Item -LiteralPath $historicalResource -Destination $tempHistorical
    if ((Convert-DiscoggerResourceBitmap -Path $tempHistorical -TemporaryRoot $folder) -ne 'normalized') {
        throw 'Real historic BOM-prefixed BMP not normalized'
    }
    $actualClean = [IO.File]::ReadAllBytes($tempHistorical)
    if ([Convert]::ToHexString($expectedClean) -ne [Convert]::ToHexString($actualClean)) {
        throw 'Removing the BOM modified historic BMP pixel/palette bytes'
    }
    if ((Get-FileHash -LiteralPath $historicalResource -Algorithm SHA256).Hash -ne $historicHash) {
        throw 'Historical Git working-tree source modified'
    }
    if ((Convert-DiscoggerResourceBitmap -Path $tempHistorical -TemporaryRoot $folder) -ne 'unchanged') {
        throw 'Cleaned resource is not idempotent'
    }

    # Negative: a BOM prefix must not legitimize arbitrary or broken data.
    $badBom = Join-Path $folder 'bom-invalid.bmp'
    [IO.File]::WriteAllBytes($badBom,[byte[]]@(239,187,191,239,187,191,1,2,3,4))
    $badBomHash = (Get-FileHash -LiteralPath $badBom -Algorithm SHA256).Hash
    $rejected = $false
    try { $null = Convert-DiscoggerResourceBitmap -Path $badBom -TemporaryRoot $folder }
    catch { $rejected = $true }
    if (-not $rejected) { throw 'BOM-prefixed invalid content was accepted' }
    if ((Get-FileHash -LiteralPath $badBom -Algorithm SHA256).Hash -ne $badBomHash) {
        throw 'Invalid BOM-prefixed file changed before rejection'
    }
    Write-Host 'PASS: exact historical double-BOM bitmap normalized without changing source or pixel bytes.'
    Write-Host 'PASS: PNG disguised as BMP, alpha-preserving RC format, idempotency and negative inputs.'
}
finally { Remove-Item -LiteralPath $folder -Recurse -Force }
