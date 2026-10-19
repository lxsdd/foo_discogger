param(
    [ValidateSet('Win32','x64')]
    [string[]] $Platforms = @('Win32','x64')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# HARD GATE: complete native foobar2000 component compilation + linking.
# Runs only inside ephemeral GitHub-hosted Windows runner staging. NO uploads,
# host deployment, release artifact or real-media modification.
$staging = Join-Path $env:RUNNER_TEMP 'discogger-sdk-probe'
$foobar = Join-Path $staging 'foobar2000'
$component = Join-Path $foobar 'foo_discogger'
$project = Join-Path $component 'foo_discogger.vcxproj'
$janssonRoot = Join-Path $env:RUNNER_TEMP 'discogger-jansson-2.14'
$oauthRoot = Join-Path $env:RUNNER_TEMP 'discogger-liboauthcpp'
$sqliteExtract = Join-Path $env:RUNNER_TEMP 'discogger-sqlite-3400100'
$zlibRoot = Join-Path $env:RUNNER_TEMP 'discogger-zlib-1.3.1'
$wtlRoot = Join-Path $env:RUNNER_TEMP 'discogger-wtl-10.0.9163'
foreach ($dir in @($staging,$component,$janssonRoot,$oauthRoot,$sqliteExtract,$zlibRoot,$wtlRoot)) {
    if (-not (Test-Path -LiteralPath $dir -PathType Container)) { throw "Verified native dependency staging missing: $dir" }
}
$wtl = @(Get-ChildItem -LiteralPath $wtlRoot -Recurse -File -Filter 'atlapp.h')
if ($wtl.Count -ne 1) { throw "Cannot locate unique previously verified WTL header." }
$oauthInclude = Join-Path $oauthRoot 'include'
$env:CL = '/I"' + $wtl[0].DirectoryName + '" /I"' + $oauthInclude + '" /I"' + $zlibRoot + '" ' + $env:CL
$msbuild = (Get-Command 'msbuild.exe' -ErrorAction Stop).Source
$includes = @($staging,$foobar,$wtl[0].DirectoryName,$oauthInclude,$component,
    (Join-Path $component 'zlib'),(Join-Path $component 'jansson'),(Join-Path $component 'oauthcpp')) -join ';'

function Invoke-Required([string]$Label,[scriptblock]$Action) {
    $global:LASTEXITCODE = 0
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "$Label failed with exit code $LASTEXITCODE" }
}
function Copy-UniqueLib([string]$SearchRoot,[string]$Name,[string]$Destination) {
    $found = @(Get-ChildItem -LiteralPath $SearchRoot -Recurse -File -Filter $Name)
    if ($found.Count -ne 1) { throw "Expected one $Name below $SearchRoot; found $($found.Count)." }
    Copy-Item -LiteralPath $found[0].FullName -Destination (Join-Path $Destination $Name) -Force
    if ((Get-Item -LiteralPath (Join-Path $Destination $Name)).Length -le 0) { throw "Empty library: $Name" }
}
function Assert-DllMachine([string]$Path,[UInt16]$Expected) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw "Native component DLL not created: $Path" }
    $bytes = [IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 512 -or $bytes[0] -ne 77 -or $bytes[1] -ne 90) { throw "Invalid MZ image: $Path" }
    $pe = [BitConverter]::ToInt32($bytes,0x3C)
    if ($pe -lt 64 -or $pe -gt ($bytes.Length - 26)) { throw "Invalid PE offset: $Path" }
    if ([Text.Encoding]::ASCII.GetString($bytes,$pe,4) -ne "PE`0`0") { throw "Invalid PE signature: $Path" }
    $actual = [BitConverter]::ToUInt16($bytes,$pe+4)
    if ($actual -ne $Expected) { throw "PE architecture mismatch: expected $Expected actual $actual ($Path)" }
}
$sqliteSources = @(Get-ChildItem -LiteralPath $sqliteExtract -Recurse -File -Filter sqlite3.c)
if ($sqliteSources.Count -ne 1) { throw 'Pinned SQLite 3.40.1 amalgamation sqlite3.c missing or ambiguous.' }
# CMake project is generated only in RUNNER_TEMP, never committed to repository.
$sqliteCmake = Join-Path $env:RUNNER_TEMP 'discogger-sqlite-project'
New-Item -ItemType Directory -Path $sqliteCmake -Force | Out-Null
$sqliteSourceCmake = $sqliteSources[0].FullName.Replace('\','/')
@(
    'cmake_minimum_required(VERSION 3.20)',
    'project(discogger_sqlite LANGUAGES C)',
    "add_library(sqlite3 STATIC `"$sqliteSourceCmake`")",
    'set_property(TARGET sqlite3 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")'
) | Set-Content -LiteralPath (Join-Path $sqliteCmake 'CMakeLists.txt') -Encoding UTF8

$failures = [System.Collections.Generic.List[string]]::new()
$manifest = [System.Collections.Generic.List[object]]::new()
foreach ($platform in $Platforms) {
    try {
        Write-Host "=== HARD NATIVE DLL BUILD AND LINK: $platform ==="
        $libDir = Join-Path $component "$platform/Release"
        New-Item -ItemType Directory -Path $libDir -Force | Out-Null
        $janssonBuild = Join-Path $env:RUNNER_TEMP "discogger-jansson-build-$platform"
        if (-not (Test-Path -LiteralPath (Join-Path $janssonBuild 'include/jansson_config.h'))) {
            throw "Pinned architecture-specific Jansson config missing: $platform"
        }
        Copy-Item -LiteralPath (Join-Path $janssonBuild 'include/jansson_config.h') -Destination (Join-Path $component 'jansson/jansson_config.h') -Force
        Invoke-Required "Jansson $platform" { & cmake --build $janssonBuild --config Release --target jansson }
        Copy-UniqueLib $janssonBuild 'jansson.lib' $libDir

        $oauthBuild = Join-Path $env:RUNNER_TEMP "discogger-oauth-build-$platform"
        Invoke-Required "OAuth configure $platform" {
            & cmake -S (Join-Path $oauthRoot 'build') -B $oauthBuild -G 'Visual Studio 17 2022' -A $platform -DLIBOAUTHCPP_BUILD_TESTS=OFF -DLIBOAUTHCPP_BUILD_DEMOS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
        }
        Invoke-Required "OAuth compile $platform" { & cmake --build $oauthBuild --config Release --target oauthcpp }
        Copy-UniqueLib $oauthBuild 'oauthcpp.lib' $libDir

        $sqliteBuild = Join-Path $env:RUNNER_TEMP "discogger-sqlite-build-$platform"
        Invoke-Required "SQLite configure $platform" { & cmake -S $sqliteCmake -B $sqliteBuild -G 'Visual Studio 17 2022' -A $platform }
        Invoke-Required "SQLite compile $platform" { & cmake --build $sqliteBuild --config Release --target sqlite3 }
        $sqliteOut = Join-Path $foobar "discogger libs/sqlite-amal-3401/$platform/Release"
        New-Item -ItemType Directory -Path $sqliteOut -Force | Out-Null
        Copy-UniqueLib $sqliteBuild 'sqlite3.lib' $sqliteOut

        $zlibBuild = Join-Path $env:RUNNER_TEMP "discogger-zlib-build-$platform"
        Invoke-Required "zlib configure $platform" { & cmake -S $zlibRoot -B $zlibBuild -G 'Visual Studio 17 2022' -A $platform -DZLIB_BUILD_TESTING=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL }
        Invoke-Required "zlib compile $platform" { & cmake --build $zlibBuild --config Release --target zlibstatic }
        Copy-UniqueLib $zlibBuild 'zlibstatic.lib' $libDir
        # zconf.h is generated by zlib CMake in the per-architecture build root.
        # Do not copy the x86 configuration into the x64 source tree (or vice versa).
        $zconf = Join-Path $zlibBuild 'zconf.h'
        if (-not (Test-Path -LiteralPath $zconf -PathType Leaf)) {
            throw "Generated zlib zconf.h missing for $platform: $zconf"
        }
        if ((Get-Item -LiteralPath $zconf).Length -eq 0) { throw "Empty generated zlib config: $zconf" }
        $nativeIncludes = @($zlibBuild, $includes) -join ';'

        # SDK libPPUI is a static library; place it at an architecture-specific
        # deterministic path (not the developer machine's default VS output).
        $ppuiProject = Join-Path $staging 'libPPUI/libPPUI.vcxproj'
        $ppuiObj = Join-Path $env:RUNNER_TEMP "discogger-libppui-int-$platform"
        Invoke-Required "libPPUI $platform" { & $msbuild $ppuiProject /t:Build /p:Configuration=Release "/p:Platform=$platform" "/p:OutDir=$libDir\" "/p:IntDir=$ppuiObj\" /m:1 /v:minimal /nologo }
        $ppuiLib = Join-Path $libDir 'libPPUI.lib'
        if (-not (Test-Path -LiteralPath $ppuiLib)) { throw "libPPUI static library missing: $ppuiLib" }

        $sharedLib = Join-Path $foobar "shared/shared-$platform.lib"
        if (-not (Test-Path -LiteralPath $sharedLib)) { throw "Official SDK shared import library missing: $sharedLib" }

        # Do not suppress a nonzero exit: a successful compilation alone is not DLL link PASS.
        Invoke-Required "Discogger native full build and LINK $platform" {
            & $msbuild $project /t:Build /p:Configuration=Release "/p:Platform=$platform" "/p:SolutionDir=$component\" "/p:AdditionalIncludeDirectories=$($nativeIncludes.Replace(';','%3B'))" /m:1 /v:minimal /nologo
        }
        $dll = Join-Path $libDir 'foo_discogger.dll'
        [UInt16] $arch = if ($platform -eq 'Win32') { 0x14C } else { 0x8664 }
        Assert-DllMachine $dll $arch
        $hash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash.ToLowerInvariant()
        $manifest.Add([pscustomobject]@{ commit=$env:GITHUB_SHA; architecture=$platform; file='foo_discogger.dll'; sha256=$hash; bytes=(Get-Item -LiteralPath $dll).Length })
        Write-Host "NATIVE_DLL_$platform=PASS SHA256=$hash (build-only, not host-qualified, NOT published)"
    } catch {
        $message = "NATIVE_DLL_$platform=FAIL: $($_.Exception.Message)"
        Write-Host $message
        $failures.Add($message)
    }
}
if ($manifest.Count -gt 0) {
    $manifestPath = Join-Path $env:RUNNER_TEMP 'discogger-native-build-manifest.json'
    ConvertTo-Json -InputObject @($manifest.ToArray()) -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
    Write-Host "Private runner-only build manifest: $manifestPath"
}
if ($failures.Count -gt 0) { throw ("Strict native component build gate failed: " + ($failures -join ' | ')) }
if ($manifest.Count -ne $Platforms.Count) { throw 'Missing architecture in strict DLL build manifest.' }
Write-Host 'PASS: full Discogger Win32 and x64 DLLs linked and PE architectures checked. No deployment/release and no host qualification.'
