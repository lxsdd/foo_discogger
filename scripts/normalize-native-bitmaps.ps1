# SPDX-License-Identifier: NONE
# Private CI helper for legacy Discogger source resources.
# Only ephemeral runner staging copies may be rewritten.
Set-StrictMode -Version Latest

function Get-DiscoggerBitmapFormat([byte[]]$Bytes) {
    if ($Bytes.Length -ge 54 -and $Bytes[0] -eq 66 -and $Bytes[1] -eq 77) {
        return 'BMP'
    }
    if ($Bytes.Length -ge 8 -and $Bytes[0] -eq 137 -and $Bytes[1] -eq 80 -and
        $Bytes[2] -eq 78 -and $Bytes[3] -eq 71 -and $Bytes[4] -eq 13 -and
        $Bytes[5] -eq 10 -and $Bytes[6] -eq 26 -and $Bytes[7] -eq 10) {
        return 'PNG'
    }
    return 'UNSUPPORTED'
}

function Convert-DiscoggerResourceBitmap([string]$Path, [string]$TemporaryRoot) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) {
        throw ("Required historical resource missing: {0}" -f $Path)
    }
    $originalBytes = [IO.File]::ReadAllBytes($Path)
    # Exact historic source forensic finding: bitmap_tracer_ext_drk.bmp
    # has TWO UTF-8 BOMs prepended to an otherwise valid 1,144-byte BMP.
    # Strip only these transport/editor bytes; never re-encode its pixels.
    $bomBytes = 0
    while (($originalBytes.Length - $bomBytes) -ge 3 -and
        $originalBytes[$bomBytes] -eq 239 -and
        $originalBytes[$bomBytes+1] -eq 187 -and
        $originalBytes[$bomBytes+2] -eq 191) {
        $bomBytes += 3
        if ($bomBytes -gt 48) { throw ("Excessive resource BOM prefix: {0}" -f $Path) }
    }
    if ($bomBytes -gt 0) {
        $cleanBytes = [byte[]]::new($originalBytes.Length - $bomBytes)
        [Array]::Copy($originalBytes,$bomBytes,$cleanBytes,0,$cleanBytes.Length)
        $originalBytes = $cleanBytes
    }
    $format = Get-DiscoggerBitmapFormat $originalBytes
    if ($format -eq 'UNSUPPORTED') {
        throw ("Resource is neither BMP nor PNG after validated BOM stripping: {0}" -f $Path)
    }
    if ($format -eq 'BMP' -and [BitConverter]::ToUInt32($originalBytes,14) -eq 40) {
        if ($bomBytes -eq 0) { return 'unchanged' }
        # The byte-for-byte original BITMAPFILEHEADER is still valid once the
        # BOM is removed; verify its own length before replacing staged data.
        if ([BitConverter]::ToUInt32($originalBytes,2) -ne $originalBytes.Length) {
            throw ("Original BMP length mismatch after BOM stripping: {0}" -f $Path)
        }
        $stagedFile = Join-Path $TemporaryRoot ("discogger-rc-" + [IO.Path]::GetFileName($Path))
        [IO.File]::WriteAllBytes($stagedFile,$originalBytes)
        Copy-Item -LiteralPath $stagedFile -Destination $Path -Force
        Write-Host ("RC_RESOURCE_BOM_CLEANED={0} bytes={1} pixels=unchanged" -f [IO.Path]::GetFileName($Path),$bomBytes)
        return 'normalized'
    }
    Add-Type -AssemblyName System.Drawing
    $inputStream = [IO.MemoryStream]::new($originalBytes, $false)
    $bitmap = $null
    $outputStream = $null
    $writer = $null
    try {
        $bitmap = [System.Drawing.Bitmap]::new($inputStream)
        $width = $bitmap.Width
        $height = $bitmap.Height
        if ($width -lt 1 -or $height -lt 1 -or ([long]$width * $height) -gt 262144) {
            throw ("Resource dimensions outside safe limits: {0}" -f $Path)
        }
        $bytesPerPixel = 4
        $pixelSize = [int]($width * $height * $bytesPerPixel)
        $data = [byte[]]::new($pixelSize)
        # Bottom-up, 32-bit BI_RGB. This is an RC.exe-compatible 40-byte DIB
        # header and retains every decoded BGRA channel including alpha.
        for ($y = 0; $y -lt $height; ++$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                $px = $bitmap.GetPixel($x,$y)
                $offset = (($height - 1 - $y) * $width + $x) * 4
                $data[$offset]     = $px.B
                $data[$offset + 1] = $px.G
                $data[$offset + 2] = $px.R
                $data[$offset + 3] = $px.A
            }
        }
        $outputStream = [IO.MemoryStream]::new()
        $writer = [IO.BinaryWriter]::new($outputStream)
        $writer.Write([byte]66)
        $writer.Write([byte]77)
        $writer.Write([UInt32](54 + $pixelSize))
        $writer.Write([UInt16]0)
        $writer.Write([UInt16]0)
        $writer.Write([UInt32]54)
        $writer.Write([UInt32]40)
        $writer.Write([Int32]$width)
        $writer.Write([Int32]$height)
        $writer.Write([UInt16]1)
        $writer.Write([UInt16]32)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]$pixelSize)
        $writer.Write([Int32]0)
        $writer.Write([Int32]0)
        $writer.Write([UInt32]0)
        $writer.Write([UInt32]0)
        $writer.Write($data)
        $writer.Flush()
        $convertedBytes = $outputStream.ToArray()
        if ($convertedBytes.Length -ne (54 + $pixelSize) -or
            [BitConverter]::ToUInt32($convertedBytes,14) -ne 40 -or
            [BitConverter]::ToUInt32($convertedBytes,10) -ne 54) {
            throw ("Incorrect RC-compatible BMP header: {0}" -f $Path)
        }
        # Verify the serialized (not merely staged) RGBA bytes before copy.
        for ($i = 0; $i -lt $data.Length; ++$i) {
            if ($data[$i] -ne $convertedBytes[54 + $i]) {
                throw ("Resource pixel mismatch after serialization: {0}" -f $Path)
            }
        }
    }
    finally {
        if ($null -ne $writer) { $writer.Dispose() }
        if ($null -ne $outputStream) { $outputStream.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
        $inputStream.Dispose()
    }
    # All image/stream handles are closed before replacing the staged source.
    $stagedFile = Join-Path $TemporaryRoot ("discogger-rc-" + [IO.Path]::GetFileName($Path))
    [IO.File]::WriteAllBytes($stagedFile, $convertedBytes)
    Copy-Item -LiteralPath $stagedFile -Destination $Path -Force
    Write-Host ("RC_RESOURCE_NORMALIZED={0} original={1} (runner staging only)" -f [IO.Path]::GetFileName($Path),$format)
    return 'normalized'
}

function Normalize-DiscoggerResourceBitmaps([string]$ComponentRoot, [string]$TemporaryRoot) {
    $resourceNames = @('bitmap_tracer.bmp','bitmap_r.bmp','bitmap_tracer_ext.bmp','bitmap_tracer_ext_drk.bmp')
    foreach ($name in $resourceNames) {
        $path = Join-Path $ComponentRoot ("ico/" + $name)
        $null = Convert-DiscoggerResourceBitmap -Path $path -TemporaryRoot $TemporaryRoot
    }
}
