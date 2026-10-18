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
$componentRoot = Join-Path $staging 'foobar2000/foo_discogger'
if (Test-Path -LiteralPath $componentRoot) { throw 'Refusing to overwrite existing staged component directory.' }
New-Item -ItemType Directory -Path $componentRoot -Force | Out-Null
# Preserve the historical relative paths: ../helpers, ../SDK, ../../pfc, ../../libPPUI.
# Only first-party source is copied into the isolated SDK workspace.
Copy-Item -Path (Join-Path $repo '*') -Destination $componentRoot -Recurse -Force
$project = Join-Path $componentRoot 'foo_discogger.vcxproj'
# Pinned upstream source is staged outside the repository; no vendor files are published.
$oauthRevision = '2893f1bf42e1dfd5acf0dd849519cf27ac9c7395'
$oauthRoot = Join-Path $env:RUNNER_TEMP 'discogger-liboauthcpp'
& git clone --quiet --no-checkout https://github.com/sirikata/liboauthcpp.git $oauthRoot
if ($LASTEXITCODE -ne 0) { throw 'OAuth source clone failed.' }
& git -C $oauthRoot checkout --quiet --detach $oauthRevision
if ($LASTEXITCODE -ne 0) { throw 'Pinned OAuth revision unavailable.' }
$actualRevision = (& git -C $oauthRoot rev-parse HEAD).Trim()
if ($actualRevision -ne $oauthRevision) { throw 'OAuth revision mismatch.' }
if (-not (Test-Path -LiteralPath (Join-Path $oauthRoot 'LICENSE'))) { throw 'OAuth license missing.' }
$oauthInclude = Join-Path $oauthRoot 'include'
if (-not (Test-Path -LiteralPath (Join-Path $oauthInclude 'liboauthcpp/liboauthcpp.h'))) { throw 'OAuth header missing.' }
Write-Host "OAuth source pinned and license present: $actualRevision"
# CL is honored by cl.exe; AdditionalIncludeDirectories is item metadata, not a reliable global MSBuild property.
$env:CL = '/I"' + $oauthInclude + '" ' + $env:CL

# Stage upstream zlib headers at an immutable, verified Git revision. No vendor
# source is copied into the repository and no binaries are published.
$zlibRevision = '51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf'
$zlibRoot = Join-Path $env:RUNNER_TEMP 'discogger-zlib-1.3.1'
& git clone --quiet --no-checkout https://github.com/madler/zlib.git $zlibRoot
if ($LASTEXITCODE -ne 0) { throw 'zlib source clone failed.' }
& git -C $zlibRoot checkout --quiet --detach $zlibRevision
if ($LASTEXITCODE -ne 0) { throw 'Pinned zlib revision unavailable.' }
$actualZlibRevision = (& git -C $zlibRoot rev-parse HEAD).Trim()
if ($actualZlibRevision -ne $zlibRevision) { throw 'zlib revision mismatch.' }
foreach ($required in @('LICENSE', 'zlib.h', 'zconf.h')) {
    if (-not (Test-Path -LiteralPath (Join-Path $zlibRoot $required))) { throw "zlib missing required file: $required" }
}
$env:CL = '/I"' + $zlibRoot + '" ' + $env:CL
Write-Host "zlib source pinned and license present: $actualZlibRevision"

$foobar = Join-Path $staging 'foobar2000'
# Stage exact Jansson 2.14 sources; the signed upstream v2.14 tag resolves
# to this immutable commit. CMake generates the platform-specific public config.
$janssonRevision = '684e18c927e89615c2d501737e90018f4930d6c5'
$janssonRoot = Join-Path $env:RUNNER_TEMP 'discogger-jansson-2.14'
& git clone --quiet --no-checkout https://github.com/akheron/jansson.git $janssonRoot
if ($LASTEXITCODE -ne 0) { throw 'Jansson source clone failed.' }
& git -C $janssonRoot checkout --quiet --detach $janssonRevision
if ($LASTEXITCODE -ne 0) { throw 'Pinned Jansson revision unavailable.' }
if ((& git -C $janssonRoot rev-parse HEAD).Trim() -ne $janssonRevision) { throw 'Jansson revision mismatch.' }
foreach ($required in @('LICENSE', 'src/jansson.h', 'CMakeLists.txt')) {
    if (-not (Test-Path -LiteralPath (Join-Path $janssonRoot $required))) { throw "Jansson missing: $required" }
}
$janssonInclude = Join-Path $componentRoot 'jansson'
New-Item -ItemType Directory -Path $janssonInclude -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $janssonRoot 'src/jansson.h') -Destination (Join-Path $janssonInclude 'jansson.h')
Write-Host "Jansson 2.14 source and MIT license pinned: $janssonRevision"

# The wrapper sqlite3.h uses this exact legacy relative path. Verify the
# official 3.40.1 amalgamation before exposing its headers to the compiler.
$sqliteUrl = 'https://sqlite.org/2022/sqlite-amalgamation-3400100.zip'
$sqliteSha256 = '49112cc7328392aa4e3e5dae0b2f6736d0153430143d21f69327788ff4efe734'
$sqliteZip = Join-Path $env:RUNNER_TEMP 'discogger-sqlite-3400100.zip'
$sqliteExtract = Join-Path $env:RUNNER_TEMP 'discogger-sqlite-3400100'
Invoke-WebRequest -Uri $sqliteUrl -OutFile $sqliteZip
$actualSqliteSha256 = (Get-FileHash -LiteralPath $sqliteZip -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSqliteSha256 -ne $sqliteSha256) { throw "SQLite source SHA-256 mismatch: $actualSqliteSha256" }
Expand-Archive -LiteralPath $sqliteZip -DestinationPath $sqliteExtract
$sqliteHeaders = @(Get-ChildItem -LiteralPath $sqliteExtract -Recurse -File -Filter sqlite3.h)
if ($sqliteHeaders.Count -ne 1) { throw 'Official SQLite amalgamation must contain exactly one sqlite3.h.' }
if (-not (Select-String -LiteralPath $sqliteHeaders[0].FullName -Pattern '^#define SQLITE_VERSION_NUMBER\s+3040001\b' -Quiet)) {
    throw 'SQLite amalgamation header version mismatch (expected 3.40.1).'
}
$sqliteInclude = Join-Path $foobar 'discogger libs/sqlite-amal-3401/include'
New-Item -ItemType Directory -Path $sqliteInclude -Force | Out-Null
Copy-Item -LiteralPath $sqliteHeaders[0].FullName -Destination (Join-Path $sqliteInclude 'sqlite3.h')
Write-Host "SQLite 3.40.1 official amalgamation SHA-256 verified: $actualSqliteSha256"

$wtlRoot = Join-Path $env:RUNNER_TEMP 'discogger-wtl-10.0.9163'
$wtl = @(Get-ChildItem -LiteralPath $wtlRoot -Filter atlapp.h -File -Recurse)
if ($wtl.Count -ne 1) { throw 'Verified WTL headers not staged.' }
$wtlInclude = $wtl[0].DirectoryName
$env:CL = '/I"' + $wtlInclude + '" ' + $env:CL
$includes = @($staging, $foobar, $wtlInclude, $oauthInclude, $componentRoot, (Join-Path $componentRoot 'zlib'), (Join-Path $componentRoot 'jansson'), (Join-Path $componentRoot 'oauthcpp')) -join ';'
$msbuild = (Get-Command msbuild.exe -ErrorAction Stop).Source
foreach ($platform in $Platforms) {
    Write-Host "=== Discogger first-party C++ compilation diagnostic: $platform ==="
    # Jansson's generated public header can vary by target architecture.
    $janssonBuild = Join-Path $env:RUNNER_TEMP "discogger-jansson-build-$platform"
    & cmake -S $janssonRoot -B $janssonBuild -G 'Visual Studio 17 2022' -A $platform -DJANSSON_WITHOUT_TESTS=ON -DJANSSON_BUILD_DOCS=OFF -DJANSSON_EXAMPLES=OFF -DJANSSON_BUILD_SHARED_LIBS=OFF -DCMAKE_POLICY_VERSION_MINIMUM=3.5
    if ($LASTEXITCODE -ne 0) { throw "Jansson $platform CMake configuration failed." }
    $janssonConfig = Join-Path $janssonBuild 'include/jansson_config.h'
    if (-not (Test-Path -LiteralPath $janssonConfig)) { throw "Jansson $platform generated public config missing." }
    Copy-Item -LiteralPath $janssonConfig -Destination (Join-Path $janssonInclude 'jansson_config.h') -Force
    Write-Host "Jansson $platform generated header ready; SQLite source header verified."
    $output = Join-Path $env:RUNNER_TEMP "discogger-first-party-$platform.log"
    # ClCompile is intentionally not Build: missing link libraries must not mask
    # the first actual component compiler diagnostics.
    $global:LASTEXITCODE = 0
    & $msbuild $project /t:ClCompile /p:Configuration=Release "/p:Platform=$platform" "/p:AdditionalIncludeDirectories=$($includes.Replace(';', '%3B'))" /m:1 /v:minimal /nologo 2>&1 | Tee-Object -FilePath $output
    if ($LASTEXITCODE -ne 0) {
        Write-Host "EXPECTED DIAGNOSTIC (not a DLL build): $platform compiler failed. See errors above."
    } else {
        Write-Host "PASS: $platform first-party compilation (link not attempted)."
    }
}
Write-Host 'Diagnostic only. No DLL was linked or published.'
exit 0
