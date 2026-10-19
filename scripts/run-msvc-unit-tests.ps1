param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('x86','x64')]
    [string]$Architecture
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Repo = Split-Path -Parent $PSScriptRoot
$Vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $Vswhere)) {
    throw 'vswhere.exe not found. Windows C++ build tools are required.'
}
$InstallPath = (& $Vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1)
if (-not $InstallPath) { throw 'Visual Studio with MSVC x86/x64 tools not found.' }
$VcVars = Join-Path $InstallPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (-not (Test-Path -LiteralPath $VcVars)) { throw "Missing VC environment: $VcVars" }

$TempRoot = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
$Work = Join-Path $TempRoot "discogger-msvc-tests-$Architecture"
New-Item -ItemType Directory -Force -Path $Work | Out-Null
$Batch = Join-Path $Work 'compile-and-test.cmd'
$Sources = @(
    'http_status_parser_test.cpp',
    'crc32_test.cpp',
    'mapping_snapshot_test.cpp',
    'collaboration_fallback_test.cpp',
    'write_safety_test.cpp',
    'multivalue_fields_test.cpp',
    'cache_lru_test.cpp',
    'cfg_snapshot_ops_test.cpp',
    'sqlite_symbol_namespace_test.cpp',
    'interruptible_wait_test.cpp'
)
$Commands = [System.Collections.Generic.List[string]]::new()
$Commands.Add('@echo off')
$Commands.Add('setlocal')
$Commands.Add("call `"$VcVars`" $Architecture > nul")
$Commands.Add('if errorlevel 1 exit /b 91')
$Commands.Add('where cl')
$Commands.Add('if errorlevel 1 exit /b 92')
foreach ($file in $Sources) {
    $stem = [IO.Path]::GetFileNameWithoutExtension($file)
    $source = Join-Path (Join-Path $Repo 'tests') $file
    if (-not (Test-Path -LiteralPath $source)) { throw "Missing unit source: $source" }
    $exe = Join-Path $Work "$stem.exe"
    $obj = Join-Path $Work "$stem.obj"
    $Commands.Add("cl /nologo /std:c++17 /EHsc /W4 /WX /Fo:`"$obj`" /Fe:`"$exe`" `"$source`"")
    $Commands.Add('if errorlevel 1 exit /b 93')
    $Commands.Add("`"$exe`"")
    $Commands.Add('if errorlevel 1 exit /b 94')
}
$Commands.Add('exit /b 0')
[IO.File]::WriteAllLines($Batch, $Commands, [Text.Encoding]::ASCII)
Write-Host "Compiling portable Discogger unit tests with MSVC for $Architecture."
& $env:ComSpec /d /c $Batch
if ($LASTEXITCODE -ne 0) { throw "MSVC $Architecture unit tests failed with exit code $LASTEXITCODE" }
Write-Host "PASS: native MSVC $Architecture C++17 unit tests."
