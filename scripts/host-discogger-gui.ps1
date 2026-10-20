#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Win32','x64')][string]$Architecture,
    [Parameter(Mandatory=$true)][string]$ComponentDll
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ($env:GITHUB_ACTIONS -ne 'true' -or -not $env:RUNNER_TEMP) {
    throw 'Only ephemeral GitHub Windows runners are authorized.'
}
if (@(Get-Process -Name foobar2000 -ErrorAction SilentlyContinue).Count) {
    throw 'Unrelated foobar2000 process: not isolated.'
}
$specs = @{
    Win32 = @{ Url='https://www.foobar2000.org/files/foobar2000_v2.26.1.exe'; Hash='849BC4424326DA81049746E9A2255A3A707E56B76CC69ED11714ED23C69ECE52'; PE=0x014c; Dir='user-components' }
    x64 = @{ Url='https://www.foobar2000.org/files/foobar2000-x64_v2.26.1.exe'; Hash='72FE15A88B085EE4E49D68114E2BB9655000C2E0FDF40DCC128BBEF8BCAF3600'; PE=0x8664; Dir='user-components-x64' }
}
$spec=$specs[$Architecture]
function Assert-PE([string]$File,[int]$Expected) {
    if (-not (Test-Path -LiteralPath $File -PathType Leaf)) { throw "Missing PE: $File" }
    $bytes=[IO.File]::ReadAllBytes($File)
    if($bytes.Length -lt 512 -or $bytes[0] -ne 77 -or $bytes[1] -ne 90) { throw "Not PE: $File" }
    $offset=[BitConverter]::ToInt32($bytes,0x3c)
    if($offset -lt 64 -or $offset -ge $bytes.Length-6 -or [Text.Encoding]::ASCII.GetString($bytes,$offset,4) -ne ("PE"+[char]0+[char]0)) { throw "Invalid PE header: $File" }
    if([BitConverter]::ToUInt16($bytes,$offset+4) -ne $Expected){throw "PE architecture mismatch: $File"}
}
$component=(Resolve-Path -LiteralPath $ComponentDll -ErrorAction Stop).Path
Assert-PE $component $spec.PE
$db=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../foo_discogger.dll.db')).Path
$signature=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($db),0,16)
if ($signature -ne ("SQLite format 3"+[char]0)) { throw 'Invalid original Discogger configuration database template.' }
$dbHash=(Get-FileHash -LiteralPath $db -Algorithm SHA256).Hash
$dir=Join-Path $env:RUNNER_TEMP ('discogger-gui-'+$Architecture+'-'+[guid]::NewGuid().ToString('N'))
$hostDir=Join-Path $dir 'host'
$installer=Join-Path $dir 'official-installer.exe'
$proc=$null
Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class DiscoggerWindowTest {
  delegate bool Callback(IntPtr hwnd,IntPtr data);
  [DllImport("user32.dll")] static extern bool EnumWindows(Callback f,IntPtr data);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] static extern int GetWindowTextW(IntPtr hwnd,StringBuilder text,int length);
  [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr hwnd);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr hwnd,int id);
  [DllImport("user32.dll")] static extern IntPtr SendMessageTimeoutW(IntPtr hwnd,uint msg,IntPtr w,IntPtr l,uint flags,uint timeout,out IntPtr result);
  public static string[] List(uint pid) {
    var list=new List<string>();
    EnumWindows((h,d)=>{
      uint p; GetWindowThreadProcessId(h,out p);
      if(p==pid && IsWindowVisible(h)){
        var sb=new StringBuilder(256); GetWindowTextW(h,sb,256);
        list.Add(sb.ToString());
      }
      return true;
    },IntPtr.Zero);
    return list.ToArray();
  }
  public static IntPtr[] Find(uint pid,string title) {
    var list=new List<IntPtr>();
    EnumWindows((h,d)=>{
      uint p; GetWindowThreadProcessId(h,out p);
      if(p==pid && IsWindowVisible(h)){
        var sb=new StringBuilder(256); GetWindowTextW(h,sb,256);
        if(sb.ToString()==title) list.Add(h);
      }
      return true;
    },IntPtr.Zero);
    return list.ToArray();
  }
  [DllImport("user32.dll")] public static extern bool PostMessageW(IntPtr hwnd,uint message,IntPtr wp,IntPtr lp);
  public static string[] AllVisible() {
    var list=new List<string>();
    EnumWindows((h,d)=>{
      if(!IsWindowVisible(h)) return true;
      uint p; GetWindowThreadProcessId(h,out p);
      var sb=new StringBuilder(256); GetWindowTextW(h,sb,256);
      string title=sb.ToString();
      if(title.Length>0 && list.Count<32) list.Add(p.ToString()+":"+title);
      return true;
    },IntPtr.Zero);
    return list.ToArray();
  }
  public static IntPtr[] FindReleaseDialogs(uint pid) {
    var matches=new List<IntPtr>();
    EnumWindows((h,d)=>{
      uint owner; GetWindowThreadProcessId(h,out owner);
      if(owner!=pid || !IsWindowVisible(h)) return true;
      int[] ids={1083,1086,1080,1018,1099,1107,1092,1198};
      foreach(var id in ids) if(GetDlgItem(h,id)==IntPtr.Zero) return true;
      matches.Add(h);
      return true;
    },IntPtr.Zero);
    return matches.ToArray();
  }
  public static bool Responsive(IntPtr h) {
    IntPtr result;
    return SendMessageTimeoutW(h,0,IntPtr.Zero,IntPtr.Zero,2,3000,out result)!=IntPtr.Zero;
  }
}
'@
function Write-DiscoggerCrashEvidence {
    # All evidence is taken from this disposable host/profile and Windows'
    # application error event metadata, never from user libraries or tokens.
    Write-Host ('DISCOGGER_CRASH_WINDOWS='+([DiscoggerWindowTest]::AllVisible() -join '|'))
    try {
        $reports=@(Get-ChildItem -LiteralPath $dir -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object {
                $_.Extension -in @('.txt','.log') -and
                $_.Name -match 'crash|failure|exception|report'
            } | Select-Object -First 6)
        Write-Host ('DISCOGGER_CRASH_REPORTS_FOUND='+$reports.Count)
        foreach($f in $reports) {
            Write-Host ('DISCOGGER_CRASH_REPORT_NAME='+$f.Name)
            $snippets=@(Get-Content -LiteralPath $f.FullName -ErrorAction SilentlyContinue |
                Where-Object { $_ -match 'Exception|Access violation|Fault|foo_discogger|Stack|Address|Module|foobar2000' } |
                Select-Object -First 24)
            foreach($line in $snippets) {
                # Strip runner-local absolute paths and cap log disclosure.
                $clean=$line -replace '[A-Za-z]:\\[^ ]+','<runner-path>'
                Write-Host ('DISCOGGER_CRASH_REPORT_LINE='+$clean.Substring(0,[Math]::Min(220,$clean.Length)))
            }
        }
    } catch { Write-Host 'DISCOGGER_CRASH_REPORT_READ=UNAVAILABLE' }
    try {
        $events=@(Get-WinEvent -FilterHashtable @{
            LogName='Application'; Id=1000,1001
            StartTime=[DateTime]::Now.AddMinutes(-20)
        } -MaxEvents 30 -ErrorAction Stop |
            Where-Object { $_.Message -match 'foobar2000.exe|foo_discogger.dll' } |
            Select-Object -First 4)
        foreach($event in $events) {
            Write-Host ('DISCOGGER_CRASH_EVENT_ID='+$event.Id)
            $details=@($event.Message -split "`r?`n" |
                Where-Object {$_ -match 'Faulting application|Faulting module|Exception code|Fault offset|Application name|Module name|Ausnahme|Fehlerhaftes Modul'} |
                Select-Object -First 9)
            foreach($entry in $details) {
                $clean=$entry -replace '[A-Za-z]:\\[^ ]+','<runner-path>'
                Write-Host ('DISCOGGER_CRASH_EVENT_LINE='+$clean.Substring(0,[Math]::Min(220,$clean.Length)))
            }
        }
        if($events.Count -eq 0){Write-Host 'DISCOGGER_CRASH_EVENT=NOT_PRESENT'}
    } catch { Write-Host 'DISCOGGER_CRASH_EVENT=UNAVAILABLE' }
}
try {
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    Invoke-WebRequest -Uri $spec.Url -OutFile $installer -ErrorAction Stop
    if((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $spec.Hash) {
        throw "Official installer hash mismatch: $Architecture"
    }
    $install=Start-Process -FilePath $installer -ArgumentList @('/S',"/D=$hostDir") -Wait -PassThru
    if($install.ExitCode -ne 0){throw "Official installer exited $($install.ExitCode)"}
    $exe=Join-Path $hostDir 'foobar2000.exe'
    Assert-PE $exe $spec.PE
    New-Item -ItemType File -Path (Join-Path $hostDir 'portable_mode_enabled') -Force | Out-Null
    $components=Join-Path $hostDir 'components'
    New-Item -ItemType Directory -Path $components -Force | Out-Null
    Copy-Item -LiteralPath $component -Destination (Join-Path $components 'foo_discogger.dll')
    # A full Discogger installation requires the upstream SQLite sidecar.
    # Seed the two known ephemeral portable-profile layouts, never a real profile.
    foreach($root in @($hostDir,(Join-Path $hostDir 'profile'))) {
        $sidecar=Join-Path $root ($spec.Dir+'/foo_discogger/foo_discogger.dll.db')
        New-Item -ItemType Directory -Path (Split-Path $sidecar) -Force | Out-Null
        Copy-Item -LiteralPath $db -Destination $sidecar
        New-Item -ItemType Directory -Path (Join-Path $root 'configuration') -Force | Out-Null
    }
    # A disposable 8kHz/16-bit mono PCM WAVE; no user media or writer actions.
    $wav=Join-Path $dir 'synthetic-silence.wav'
    $writer=[IO.BinaryWriter]::new([IO.File]::Open($wav,[IO.FileMode]::CreateNew))
    try {
        $writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'))
        $writer.Write([int]16036)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '))
        $writer.Write([int]16)
        $writer.Write([short]1)
        $writer.Write([short]1)
        $writer.Write([int]8000)
        $writer.Write([int]16000)
        $writer.Write([short]2)
        $writer.Write([short]16)
        $writer.Write([Text.Encoding]::ASCII.GetBytes('data'))
        $writer.Write([int]16000)
        $writer.Write([byte[]]::new(16000))
    } finally { $writer.Dispose() }
    $before=Get-Item -LiteralPath $wav
    $wavHash=(Get-FileHash -LiteralPath $wav -Algorithm SHA256).Hash
    $wavSize=$before.Length
    $wavTicks=$before.LastWriteTimeUtc.Ticks

    $proc=Start-Process -FilePath $exe -WorkingDirectory $hostDir -PassThru
    Start-Sleep -Seconds 5
    $proc.Refresh()
    if($proc.HasExited){throw 'foobar exited while initializing Discogger.'}
    if(-not @((Get-Process -Id $proc.Id).Modules | Where-Object {$_.ModuleName -ieq 'foo_discogger.dll'}).Count){
        throw 'Discogger module is absent from host process.'
    }
    # Validate the actual initialization side effect; absence of a visible
    # error dialog alone never proves that CONF.load() completed.
    $profileRoots=@($hostDir,(Join-Path $hostDir 'profile'))
    $configurationDatabases=@($profileRoots | ForEach-Object {
        Join-Path (Join-Path $_ 'configuration') 'foo_discogger.dll.db'
    })
    $installed=@($configurationDatabases | Where-Object {Test-Path -LiteralPath $_ -PathType Leaf})
    if($installed.Count -eq 0) {
        Write-Host 'DISCOGGER_INIT_DIAGNOSTIC=no_configuration_database'
        Write-Host ('DISCOGGER_HOST_VISIBLE_WINDOWS='+([DiscoggerWindowTest]::List([uint32]$proc.Id) -join '|'))
        throw 'Discogger CONF.load did not install its SQLite configuration DB into the portable profile.'
    }
    foreach($configured in $installed) {
        if ([Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($configured),0,16) -ne ("SQLite format 3"+[char]0)) {
            throw 'Discogger created an invalid SQLite configuration database.'
        }
    }
    if(@([DiscoggerWindowTest]::Find([uint32]$proc.Id,'foo_discogger initialization')).Count){
        throw 'Discogger configuration initialization blocked by a modal error.'
    }
    # The official player may show its one-time appearance chooser before it
    # accepts any forwarded /context_command. Dismiss ONLY that chooser,
    # and ONLY when its HWND belongs to this disposable foobar process.
    $appearance=@([DiscoggerWindowTest]::Find([uint32]$proc.Id,'Quick Appearance Setup'))
    if($appearance.Count -eq 1) {
        Write-Host 'DISCOGGER_FIRST_RUN_APPEARANCE=DISMISS_IN_TEST_ONLY'
        if(-not [DiscoggerWindowTest]::PostMessageW($appearance[0],0x0010,[IntPtr]::Zero,[IntPtr]::Zero)) {
            throw 'Could not dismiss isolated Quick Appearance Setup wizard.'
        }
        Start-Sleep -Seconds 2
        if(@([DiscoggerWindowTest]::Find([uint32]$proc.Id,'Quick Appearance Setup')).Count){
            throw 'First-run appearance dialog still blocks context commands.'
        }
    } elseif($appearance.Count -gt 1) {
        throw 'Multiple first-run appearance dialogs in disposable host.'
    }
    Write-Host "DISCOGGER_INITIALIZATION_$Architecture=PASS"
    Write-Host ('DISCOGGER_INITIAL_WINDOWS='+([DiscoggerWindowTest]::List([uint32]$proc.Id) -join '|'))
    $window=[IntPtr]::Zero
    foreach($name in @('Tagging/Discogger/Write tags...','Discogger/Write tags...','Write tags...')){
        $args='/context_command:"'+$name+'" "'+$wav+'"'
        $launcher=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $args -PassThru
        if(-not $launcher.WaitForExit(15000) -or $launcher.ExitCode -ne 0){throw "Context launcher failed: $name"}
        $proc.Refresh()
        if($proc.HasExited){throw "The actual foobar host exited after context route $name"}
        Write-Host ('DISCOGGER_CONTEXT_ROUTE='+$name)
        for($i=0;$i -lt 14;$i++){
            Start-Sleep -Milliseconds 500
            $proc.Refresh()
            $visible=@([DiscoggerWindowTest]::AllVisible())
            if($proc.HasExited -or @($visible | Where-Object {$_ -match 'foobar2000 has crashed'}).Count) {
                Write-Host ('DISCOGGER_CRASH_AFTER_CONTEXT_ROUTE='+$name)
                Write-DiscoggerCrashEvidence
                throw 'Real foobar2000 crashed during the first matching tagging context dispatch.'
            }
            $matches=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
            if($matches.Length -gt 1){throw 'Multiple native Find Releases dialogs with the expected control IDs.'}
            if($matches.Length -eq 1){$window=$matches[0];break}
        }
        if($window -ne [IntPtr]::Zero){break}
    }
    if($window -eq [IntPtr]::Zero){
        $proc.Refresh()
        Write-Host ('DISCOGGER_HOST_ALIVE='+(-not $proc.HasExited))
        $currentProcesses=@(Get-Process -Name foobar2000 -ErrorAction SilentlyContinue)
        Write-Host ('DISCOGGER_RUNNING_PROCESS_IDS='+($currentProcesses.Id -join ','))
        Write-Host ('DISCOGGER_WINDOWS='+([DiscoggerWindowTest]::List([uint32]$proc.Id) -join '|'))
        Write-Host ('DISCOGGER_ALL_VISIBLE_WINDOWS='+([DiscoggerWindowTest]::AllVisible() -join '|'))
        if($proc.HasExited -or @([DiscoggerWindowTest]::AllVisible() | Where-Object {$_ -match 'foobar2000 has crashed'}).Count) {
            Write-DiscoggerCrashEvidence
            throw 'foobar2000 crashed before Find Releases native controls could be verified.'
        }
        throw 'No native Discogger dialog with all eight expected controls; see isolated host diagnostics.'
    }
    if(-not [DiscoggerWindowTest]::Responsive($window)){throw 'Native Find Releases window not responsive.'}
    foreach($id in @(1083,1086,1080,1018,1099,1107,1092,1198)){
        if([DiscoggerWindowTest]::GetDlgItem($window,$id) -eq [IntPtr]::Zero){throw "Missing control ID $id"}
    }
    $after=Get-Item -LiteralPath $wav
    if((Get-FileHash -LiteralPath $wav -Algorithm SHA256).Hash -ne $wavHash -or
       $after.Length -ne $wavSize -or $after.LastWriteTimeUtc.Ticks -ne $wavTicks){
        throw 'Synthetic audio was modified by opening the native dialog.'
    }
    if((Get-FileHash -LiteralPath $db -Algorithm SHA256).Hash -ne $dbHash){throw 'Original DB template changed.'}
    Write-Host "DISCOGGER_GUI_DIALOG_$Architecture=PASS"
    Write-Host "DISCOGGER_GUI_CONTROLS_$Architecture=PASS"
    Write-Host "DISCOGGER_GUI_SYNTHETIC_WAV_NO_WRITE_$Architecture=PASS"
    Write-Host "MEDIA_WRITE_SAFETY_$Architecture=NOT_TESTED"
} finally {
    if($null -ne $proc){
        try {if(-not $proc.HasExited){Stop-Process -Id $proc.Id -Force -ErrorAction Stop}}
        catch {Write-Warning 'Disposable foobar process could not be stopped.'}
    }
    if(Test-Path -LiteralPath $dir){Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue}
}
