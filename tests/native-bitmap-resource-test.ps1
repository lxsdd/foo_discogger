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
    Write-Host 'PASS: PNG disguised as BMP, alpha-preserving RC format, idempotency and negative inputs.'
}
finally { Remove-Item -LiteralPath $folder -Recurse -Force }
