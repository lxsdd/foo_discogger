#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][ValidateSet('Win32','x64')][string]$Architecture,
    [Parameter(Mandatory=$true)][string]$ComponentDll,
    [string]$CompanionMetadataIsolationDll,
    [switch]$AuthorizedLive
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
$companion=$null
if($CompanionMetadataIsolationDll){
    $companion=(Resolve-Path -LiteralPath $CompanionMetadataIsolationDll -ErrorAction Stop).Path
    Assert-PE $companion $spec.PE
}
if($AuthorizedLive){
    # Credentials exist only in this protected workflow step's process memory.
    # Missing/partial values fail closed without disclosing their contents.
    $names=@('DISCOGS_OAUTH_CONSUMER_KEY','DISCOGS_OAUTH_CONSUMER_SECRET',
        'DISCOGS_OAUTH_ACCESS_TOKEN','DISCOGS_OAUTH_TOKEN_SECRET')
    foreach($name in $names){
        if(-not [Environment]::GetEnvironmentVariable($name,'Process')){
            throw 'Protected native GUI OAuth environment is incomplete.'
        }
    }
}

$db=(Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../foo_discogger.dll.db')).Path
$signature=[Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($db),0,16)
if ($signature -ne ("SQLite format 3"+[char]0)) { throw 'Invalid original Discogger configuration database template.' }
$dbHash=(Get-FileHash -LiteralPath $db -Algorithm SHA256).Hash
$dir=Join-Path $env:RUNNER_TEMP ('discogger-gui-'+$Architecture+'-'+[guid]::NewGuid().ToString('N'))
$hostDir=Join-Path $dir 'host'
$installer=$null
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
  [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool SetWindowTextW(IntPtr hwnd,string value);
  // Cross-process edit controls require WM_SETTEXT/WM_GETTEXT, not the
  // SetWindowText/GetWindowText convenience APIs (Microsoft Win32 contract).
  [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", CharSet=CharSet.Unicode)]
  static extern IntPtr SendTextMessageTimeoutW(IntPtr hwnd,uint msg,IntPtr w,string text,uint flags,uint timeout,out IntPtr result);
  [DllImport("user32.dll", EntryPoint="SendMessageTimeoutW", CharSet=CharSet.Unicode)]
  static extern IntPtr ReadTextMessageTimeoutW(IntPtr hwnd,uint msg,IntPtr w,StringBuilder text,uint flags,uint timeout,out IntPtr result);
  public static void SetEditText(IntPtr hwnd,string value) {
    IntPtr result;
    if(SendTextMessageTimeoutW(hwnd,0x000C,IntPtr.Zero,value,2,4000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Timed out setting native search edit text");
  }
  public static string EditText(IntPtr hwnd) {
    var sb=new StringBuilder(1024);
    IntPtr result;
    if(ReadTextMessageTimeoutW(hwnd,0x000D,new IntPtr(sb.Capacity),sb,2,4000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Timed out reading native search edit text");
    return sb.ToString();
  }

  public static string WindowText(IntPtr hwnd) {
    var sb=new StringBuilder(1024);
    GetWindowTextW(hwnd,sb,sb.Capacity);
    return sb.ToString();
  }
  public static int TreeCount(IntPtr hwnd) {
    IntPtr result;
    if(SendMessageTimeoutW(hwnd,0x1105,IntPtr.Zero,IntPtr.Zero,2,4000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Timed out reading native release tree count");
    return checked((int)result.ToInt64());
  }
  public static void ClickButton(IntPtr hwnd) {
    IntPtr result;
    if(SendMessageTimeoutW(hwnd,0x00F5,IntPtr.Zero,IntPtr.Zero,2,4000,out result)==IntPtr.Zero)
      throw new InvalidOperationException("Timed out pressing native Search button");
  }
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
  // Purely observational HWND geometry: never set focus, click a button,
  // post an input event or alter any selected media file.
  [StructLayout(LayoutKind.Sequential)]
  public struct Rect { public int Left,Top,Right,Bottom; }
  [StructLayout(LayoutKind.Sequential)]
  public struct Point { public int X,Y; }
  [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out Rect r);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h,out Rect r);
  [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h,ref Point p);
  [DllImport("user32.dll")] static extern bool IsWindowEnabled(IntPtr h);
  public static string CheckFindReleasesGeometry(IntPtr dialog) {
    Rect client;
    if (!GetClientRect(dialog,out client)) return "GetClientRect failed";
    Point origin=new Point { X=0,Y=0 };
    if (!ClientToScreen(dialog,ref origin)) return "ClientToScreen failed";
    int left=origin.X+client.Left, top=origin.Y+client.Top;
    int right=origin.X+client.Right, bottom=origin.Y+client.Bottom;
    if(right-left<200 || bottom-top<100) return "Find Releases client area too small";
    int[] required={1083,1086,1080,1018,1099,1092,1198};
    var rects=new Dictionary<int,Rect>();
    foreach(int id in required) {
      IntPtr control=GetDlgItem(dialog,id);
      if(control==IntPtr.Zero) return "Missing control ID "+id;
      Rect box;
      if(!GetWindowRect(control,out box)) return "GetWindowRect failed for ID "+id;
      if(box.Left>=box.Right || box.Top>=box.Bottom) return "Zero-size control ID "+id;
      // Client-relative containment also catches actual cut-offs caused by
      // DPI and resizable-window layout regressions.
      if(box.Left<left-2 || box.Top<top-2 ||
         box.Right>right+2 || box.Bottom>bottom+2)
        return "Clipped outside client area ID "+id;
      rects[id]=box;
    }
    Rect search=rects[1083], button=rects[1086], filter=rects[1099];
    int searchMid=search.Top+(search.Bottom-search.Top)/2;
    int buttonMid=button.Top+(button.Bottom-button.Top)/2;
    // Use a small pixel tolerance for DLU rounding across Win32 and x64.
    if(Math.Abs(searchMid-buttonMid)>6)
      return "Search text and Search button vertical centers differ";
    if(search.Right>button.Left || button.Left-search.Right>32)
      return "Search edit overlaps or is too far from Search button";
    Rect artists=rects[1080], releases=rects[1018];
    if(artists.Right>releases.Left) return "Artist list overlaps release tree";
    if(filter.Top>artists.Top || filter.Bottom>artists.Bottom)
      return "Release filter field is misaligned with list/tree region";
    return "";
  }
  [DllImport("user32.dll")] static extern bool MoveWindow(IntPtr hwnd,int x,int y,int width,int height,bool repaint);
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int id);
  public static int[] Bounds(IntPtr hwnd) {
    Rect r;
    if(!GetWindowRect(hwnd,out r)) throw new InvalidOperationException("GetWindowRect failed");
    return new int[] {r.Left,r.Top,r.Right-r.Left,r.Bottom-r.Top};
  }
  public static string ResizeForLayoutTest(IntPtr dialog) {
    int[] old=Bounds(dialog);
    int screenW=GetSystemMetrics(0),screenH=GetSystemMetrics(1);
    int width=old[2]+120,height=old[3]+64;
    if(width>screenW-16 || height>screenH-16)
      return "Insufficient disposable desktop work area for native resize";
    int x=Math.Max(8,Math.Min(old[0],screenW-width-8));
    int y=Math.Max(8,Math.Min(old[1],screenH-height-8));
    if(!MoveWindow(dialog,x,y,width,height,true)) return "MoveWindow failed";
    int[] resized=Bounds(dialog);
    if(resized[2]<width-8 || resized[3]<height-8)
      return "Find Releases did not honor resize request";
    return CheckFindReleasesGeometry(dialog);
  }
  public static bool Responsive(IntPtr h) {
    IntPtr result;
    return SendMessageTimeoutW(h,0,IntPtr.Zero,IntPtr.Zero,2,3000,out result)!=IntPtr.Zero;
  }
}
'@
function Write-DiscoggerCrashEvidence {
    if($AuthorizedLive){
        Write-Host 'DISCOGGER_AUTHENTICATED_CRASH_DETAILS=WITHHELD_FOR_SECRET_SAFETY'
        # Never expose minidumps, crash text, process memory, paths, requests or
        # credentials. Only numeric Windows event metadata may reach CI logs.
        try {
            $proc.Refresh()
            if($proc.HasExited){
                Write-Host ('DISCOGGER_AUTHENTICATED_HOST_EXIT_CODE='+[int64]$proc.ExitCode)
            }
        } catch { Write-Host 'DISCOGGER_AUTHENTICATED_HOST_EXIT_CODE=UNAVAILABLE' }
        try {
            $events=@(Get-WinEvent -FilterHashtable @{
                LogName='Application'; Id=1000
                StartTime=[DateTime]::Now.AddMinutes(-10)
            } -MaxEvents 20 -ErrorAction Stop |
                Where-Object { $_.Properties.Count -ge 7 -and
                    [string]$_.Properties[0].Value -eq 'foobar2000.exe' } |
                Select-Object -First 1)
            if($events.Count){
                $event=$events[0]
                $module=[IO.Path]::GetFileName([string]$event.Properties[3].Value)
                if($module -notin @('foobar2000.exe','foo_discogger.dll','foo_metadata_isolation.dll')){
                    $module='other'
                }
                $exception=[string]$event.Properties[6].Value
                if($exception -notmatch '^(0x)?[0-9a-fA-F]{8}$'){$exception='UNKNOWN'}
                Write-Host "DISCOGGER_AUTHENTICATED_CRASH_SIGNATURE=module:$module code:$exception"
            } else {
                Write-Host 'DISCOGGER_AUTHENTICATED_CRASH_SIGNATURE=NO_EVENT'
            }
        } catch { Write-Host 'DISCOGGER_AUTHENTICATED_CRASH_SIGNATURE=UNAVAILABLE' }
        return
    }
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
    # Two preceding SHA-pinned shared foobar host-smokes downloaded the
    # original installers to the *same job's* content-addressed RUNNER_TEMP.
    # This full GUI test must not repeat the blocked upstream download.
    $cacheDir=Join-Path $env:RUNNER_TEMP 'foobar2000-verified-installer-v1'
    $cacheName="$Architecture-2.26.1-$($spec.Hash.ToLowerInvariant()).exe"
    $installer=Join-Path $cacheDir $cacheName
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw "DISCOGGER_GUI_INSTALLER_CACHE_MISSING_${Architecture}: pinned shared host-smoke must run first; GUI_NOT_TESTED"
    }
    if ((Get-FileHash -LiteralPath $installer -Algorithm SHA256).Hash -ine $spec.Hash) {
        throw "DISCOGGER_GUI_INSTALLER_HASH_MISMATCH_${Architecture}: refusing modified cached executable; GUI_NOT_TESTED"
    }
    Write-Host "DISCOGGER_GUI_INSTALLER_SOURCE=VERIFIED_JOB_LOCAL_CACHE arch=$Architecture version=2.26.1"

    $install=Start-Process -FilePath $installer -ArgumentList @('/S',"/D=$hostDir") -Wait -PassThru
    if($install.ExitCode -ne 0){throw "Official installer exited $($install.ExitCode)"}
    $exe=Join-Path $hostDir 'foobar2000.exe'
    Assert-PE $exe $spec.PE
    New-Item -ItemType File -Path (Join-Path $hostDir 'portable_mode_enabled') -Force | Out-Null
    $components=Join-Path $hostDir 'components'
    New-Item -ItemType Directory -Path $components -Force | Out-Null
    Copy-Item -LiteralPath $component -Destination (Join-Path $components 'foo_discogger.dll')
    if($companion){
        Copy-Item -LiteralPath $companion -Destination (Join-Path $components 'foo_metadata_isolation.dll')
    }
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
    # A synthetic external CUE references only disposable test audio.
    $cue=Join-Path $dir 'synthetic-album.cue'
    $cueBody=@'
PERFORMER "Discogger Synthetic Artist"
TITLE "Discogger Synthetic Album"
FILE "synthetic-silence.wav" WAVE
  TRACK 01 AUDIO
    TITLE "Synthetic Track"
    INDEX 01 00:00:00
'@
    [IO.File]::WriteAllText($cue,$cueBody,[Text.Encoding]::ASCII)
    $cueBefore=Get-Item -LiteralPath $cue
    $cueHash=(Get-FileHash -LiteralPath $cue -Algorithm SHA256).Hash
    $cueSize=$cueBefore.Length
    $cueTicks=$cueBefore.LastWriteTimeUtc.Ticks

    # Synthetic silence with ID3v2, generated once and SHA-256 pinned.
    # Encoded payload is locally generated audio, never any user content.
    $mp3=Join-Path $dir 'synthetic-metadata.mp3'
    & python (Join-Path $PSScriptRoot '../tests/create-synthetic-mp3.py') $mp3
    if($LASTEXITCODE -ne 0){throw 'Synthetic MP3 generator failed'}
    $flac=Join-Path $dir 'synthetic-embedded.flac'
    $flacGenerator=Join-Path (Get-Location) 'external/discogger-rich-fixtures/.github/actions/foobar-host-smoke/synthetic-embedded-flac.py'
    if(-not (Test-Path -LiteralPath $flacGenerator -PathType Leaf)){throw 'Pinned FLAC generator unavailable'}
    & python $flacGenerator $flac
    if($LASTEXITCODE -ne 0){throw 'Pinned embedded CUE FLAC generator failed'}
    # Add original synthetic artwork, multivalues and ReplayGain to be
    # compared byte-for-byte/semantically after actual foobar metadata writes.
    & python (Join-Path $PSScriptRoot '../tests/enrich-synthetic-flac.py') $flac
    if($LASTEXITCODE -ne 0){throw 'Synthetic FLAC rich metadata generator failed'}
    $sideB=Join-Path $dir 'synthetic-side-b.wav'
    Copy-Item -LiteralPath $wav -Destination $sideB
    $multiCue=Join-Path $dir 'synthetic-multifile.cue'
    $multiCueBody=@'
PERFORMER "Discogger Synthetic Artist"
TITLE "Runner Multi-File Release"
FILE "synthetic-silence.wav" WAVE
  TRACK 01 AUDIO
    TITLE "Runner First"
    INDEX 01 00:00:00
FILE "synthetic-side-b.wav" WAVE
  TRACK 02 AUDIO
    TITLE "Runner Second"
    INDEX 01 00:00:00
'@
    [IO.File]::WriteAllText($multiCue,$multiCueBody,[Text.Encoding]::ASCII)
    $moreMedia=@($mp3,$flac,$sideB,$multiCue)
    $moreBefore=@{}
    foreach($path in $moreMedia) {
        $item=Get-Item -LiteralPath $path
        $moreBefore[$path]=@{ hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;
            size=$item.Length; ticks=$item.LastWriteTimeUtc.Ticks }
    }
    # QA-only DLL gets access to this ephemeral folder; no user music path.
    [Environment]::SetEnvironmentVariable('DISCOGGER_QA_MEDIA_DIR',$dir,'Process')
    $proc=Start-Process -FilePath $exe -WorkingDirectory $hostDir -PassThru
    Start-Sleep -Seconds 5
    $proc.Refresh()
    if($proc.HasExited){throw 'foobar exited while initializing Discogger.'}
    $loadedModules=@((Get-Process -Id $proc.Id).Modules | ForEach-Object {$_.ModuleName})
    if(-not @($loadedModules | Where-Object {$_ -ieq 'foo_discogger.dll'}).Count){
        throw 'Discogger module is absent from host process.'
    }
    if($companion){
        if(-not @($loadedModules | Where-Object {$_ -ieq 'foo_metadata_isolation.dll'}).Count){
            throw 'Metadata Isolation companion is absent from native foobar host process.'
        }
        Write-Host "DISCOGGER_METADATA_ISOLATION_NATIVE_COLOAD_$Architecture=PASS"
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
    $chosenContextRoute=''
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
            if($matches.Length -eq 1){$window=$matches[0];$chosenContextRoute=$name;break}
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
    $layoutFailure=[DiscoggerWindowTest]::CheckFindReleasesGeometry($window)
    if($layoutFailure) {
        throw ('Discogger GUI layout/controls failed: '+$layoutFailure)
    }
    Write-Host "DISCOGGER_GUI_LAYOUT_$Architecture=PASS"
    # Only manipulate a disposable modeless HWND. No Search, Next,
    # tagging, artwork, ReplayGain or CUE-write button is ever activated.
    $resizeError=[DiscoggerWindowTest]::ResizeForLayoutTest($window)
    if($resizeError){throw ('Native Find Releases resize failed: '+$resizeError)}
    Start-Sleep -Milliseconds 450
    $resizedGeometry=[DiscoggerWindowTest]::Bounds($window)
    Write-Host "DISCOGGER_GUI_RESIZE_$Architecture=PASS"

    # Normal WM_CLOSE runs cfgDialogPosition's OnDestroy handler in the
    # temporary foobar profile, so size and position should be restored.
    if(-not [DiscoggerWindowTest]::PostMessageW($window,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)){
        throw 'Cannot close disposable Find Releases dialog.'
    }
    $closed=$false
    for($n=0;$n -lt 35;$n++){
        Start-Sleep -Milliseconds 200
        $proc.Refresh()
        if($proc.HasExited){throw 'foobar2000 terminated while closing the Find Releases dialog.'}
        if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count -eq 0){
            $closed=$true;break
        }
    }
    if(-not $closed){throw 'Find Releases did not close after WM_CLOSE.'}
    Start-Sleep -Milliseconds 500
    if(-not $chosenContextRoute){throw 'Missing previously qualified context route.'}
    $reopenArgs='/context_command:"'+$chosenContextRoute+'" "'+$wav+'"'
    $launcherAgain=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $reopenArgs -PassThru
    if(-not $launcherAgain.WaitForExit(15000) -or $launcherAgain.ExitCode -ne 0){
        throw 'Context command failed when reopening Find Releases.'
    }
    $reopened=[IntPtr]::Zero
    for($n=0;$n -lt 30;$n++){
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
        if($proc.HasExited){throw 'foobar2000 terminated during second native dialog opening.'}
        $windows=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
        if($windows.Length -gt 1){throw 'Duplicate Find Releases windows after reopening.'}
        if($windows.Length -eq 1){$reopened=$windows[0];break}
    }
    if($reopened -eq [IntPtr]::Zero){throw 'Find Releases did not reopen after closing.'}
    if(-not [DiscoggerWindowTest]::Responsive($reopened)){throw 'Reopened Find Releases is unresponsive.'}
    $reopenGeometry=[DiscoggerWindowTest]::Bounds($reopened)
    for($axis=0;$axis -lt 4;$axis++){
        if([Math]::Abs($resizedGeometry[$axis]-$reopenGeometry[$axis]) -gt 24){
            throw "Dialog size/position persistence failed (axis $axis)."
        }
    }
    $reopenedLayoutError=[DiscoggerWindowTest]::CheckFindReleasesGeometry($reopened)
    if($reopenedLayoutError){throw ('Reopened Find Releases layout failed: '+$reopenedLayoutError)}
    Write-Host "DISCOGGER_GUI_PERSISTENCE_$Architecture=PASS"

    # Populate a real native Discogger GUI with five synthetic release objects.
    # This validates actual artist filtering and release tree rendering, not
    # authenticated search or metadata commits.
    $guiEvidence=Join-Path $dir 'qa-populated-gui.txt'
    if(Test-Path -LiteralPath $guiEvidence){throw 'Populated GUI evidence unexpectedly exists.'}
    $popCmd=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA populated GUI"' -PassThru
    if(-not $popCmd.WaitForExit(15000) -or $popCmd.ExitCode -ne 0){
        throw 'Could not dispatch synthetic populated native GUI QA.'
    }
    $populated=$false
    for($i=0;$i -lt 50;$i++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $guiEvidence -PathType Leaf){
            $evidence=Get-Content -LiteralPath $guiEvidence -Raw
            if($evidence.Contains('ARTISTS=1') -and $evidence.Contains('RELEASES=5')){
                $populated=$true;break
            }
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Host exited while populating synthetic native releases.'}
    }
    if(-not $populated){throw 'No native populated artist/release tree evidence.'}
    if(-not [DiscoggerWindowTest]::Responsive($reopened)){
        throw 'Populated native Find Releases GUI became unresponsive.'
    }
    Write-Host "DISCOGGER_GUI_POPULATED_SYNTHETIC_RELEASES_$Architecture=PASS"

    # Read-only external CUE context route; no tag/CUE writer is activated.
    if(-not [DiscoggerWindowTest]::PostMessageW($reopened,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)){
        throw 'Cannot close disposable WAV dialog before the CUE context test.'
    }
    $oldDialogClosed=$false
    for($n=0;$n -lt 35;$n++){
        Start-Sleep -Milliseconds 200
        if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count -eq 0){
            $oldDialogClosed=$true;break
        }
    }
    if(-not $oldDialogClosed){throw 'WAV dialog remains open before the CUE test.'}
    $cueArgs='/context_command:"'+$chosenContextRoute+'" "'+$cue+'"'
    $cueLauncher=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $cueArgs -PassThru
    if(-not $cueLauncher.WaitForExit(15000) -or $cueLauncher.ExitCode -ne 0){
        throw 'External CUE context command could not be dispatched.'
    }
    $cueDialog=[IntPtr]::Zero
    for($n=0;$n -lt 30;$n++){
        Start-Sleep -Milliseconds 300
        $proc.Refresh()
        if($proc.HasExited){throw 'foobar2000 terminated during external CUE context load.'}
        $matches=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
        if($matches.Length -gt 1){throw 'Multiple Find Releases dialogs during external CUE test.'}
        if($matches.Length -eq 1){$cueDialog=$matches[0];break}
    }
    if($cueDialog -eq [IntPtr]::Zero -or -not [DiscoggerWindowTest]::Responsive($cueDialog)){
        throw 'External CUE did not open a responsive native Find Releases dialog.'
    }
    $cueAfter=Get-Item -LiteralPath $cue
    if((Get-FileHash -LiteralPath $cue -Algorithm SHA256).Hash -ne $cueHash -or
       $cueAfter.Length -ne $cueSize -or $cueAfter.LastWriteTimeUtc.Ticks -ne $cueTicks){
        throw 'The external CUE was changed by a read-only context/dialog operation.'
    }
    Write-Host "DISCOGGER_GUI_EXTERNAL_CUE_CONTEXT_NO_WRITE_$Architecture=PASS"
    $after=Get-Item -LiteralPath $wav
    if((Get-FileHash -LiteralPath $wav -Algorithm SHA256).Hash -ne $wavHash -or
       $after.Length -ne $wavSize -or $after.LastWriteTimeUtc.Ticks -ne $wavTicks){
        throw 'Synthetic audio was modified by opening the native dialog.'
    }
    # Three additional REAL foobar contexts with throwaway physical media.
    # Test invokes only read-only Find Releases, never any write UI action.
    $activeDialog=$cueDialog
    foreach($testCase in @(
        @{Name='MP3';Path=$mp3},
        @{Name='FLAC_EMBEDDED_CUE';Path=$flac},
        @{Name='MULTIFILE_CUE';Path=$multiCue}
    )) {
        if(-not [DiscoggerWindowTest]::PostMessageW($activeDialog,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)){
            throw 'Cannot close read-only Discogger fixture dialog'
        }
        $closedCase=$false
        for($n=0;$n -lt 35;$n++){
            Start-Sleep -Milliseconds 200
            if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count -eq 0){
                $closedCase=$true; break
            }
        }
        if(-not $closedCase){throw 'Previous read-only Discogger fixture dialog stayed open'}
        $argsCase='/context_command:"'+$chosenContextRoute+'" "'+$testCase.Path+'"'
        $launched=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $argsCase -PassThru
        if(-not $launched.WaitForExit(15000) -or $launched.ExitCode -ne 0){
            throw "Read-only context launch failed for $($testCase.Name)"
        }
        $opened=[IntPtr]::Zero
        for($n=0;$n -lt 35;$n++){
            Start-Sleep -Milliseconds 300
            $proc.Refresh()
            if($proc.HasExited){throw "Host exited for $($testCase.Name)"}
            $matches=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
            if($matches.Length -gt 1){throw 'Unexpected duplicate Find Releases dialogs'}
            if($matches.Length -eq 1){$opened=$matches[0];break}
        }
        if($opened -eq [IntPtr]::Zero -or -not [DiscoggerWindowTest]::Responsive($opened)){
            throw "Read-only Find Releases window failed for $($testCase.Name)"
        }
        $activeDialog=$opened
        Write-Host ("DISCOGGER_GUI_READONLY_"+$testCase.Name+"_"+$Architecture+"=PASS")
    }
    foreach($path in $moreMedia){
        $original=$moreBefore[$path]
        $after=Get-Item -LiteralPath $path
        if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $original.hash -or
            $after.Length -ne $original.size -or $after.LastWriteTimeUtc.Ticks -ne $original.ticks){
            throw "Read-only native context altered fixture: $([IO.Path]::GetFileName($path))"
        }
    }
    Write-Host "DISCOGGER_GUI_MP3_FLAC_CUE_READONLY_$Architecture=PASS"
    # Real SDK file_info_manager tag commits, runner-only compiled command.
    # These tests do not qualify artwork or failed multi-file rollback.
    $qaPaths=@(@{Name='MP3';File=$mp3},@{Name='FLAC';File=$flac})
    foreach($qa in $qaPaths){
        $beforeHash=(Get-FileHash -LiteralPath $qa.File -Algorithm SHA256).Hash
        $original=Join-Path $dir ('qa-original-'+$qa.Name.ToLowerInvariant()+'.backup')
        Copy-Item -LiteralPath $qa.File -Destination $original
        $command='/command:"Discogger QA '+$qa.Name+' write"'
        $commandExe=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $command -PassThru
        if(-not $commandExe.WaitForExit(15000) -or $commandExe.ExitCode -ne 0){
            throw "Failed to invoke test-only $($qa.Name) metadata writer."
        }
        $completion=Join-Path $dir ('qa-'+$qa.Name.ToLowerInvariant()+'-completed.txt')
        if(Test-Path -LiteralPath $completion){throw 'Runner QA completion unexpectedly exists before first commit.'}
        $changed=$false
        $committed=$false
        for($n=0;$n -lt 70;$n++){
            Start-Sleep -Milliseconds 400
            $changed=(Get-FileHash -LiteralPath $qa.File -Algorithm SHA256).Hash -ne $beforeHash
            $committed=(Test-Path -LiteralPath $completion -PathType Leaf) -and
                ((Get-Content -LiteralPath $completion -Raw).Trim() -ceq '1')
            if($changed -and $committed){break}
            $proc.Refresh()
            if($proc.HasExited){throw "Foobar exited during $($qa.Name) physical writer."}
        }
        if(-not ($changed -and $committed)){
            throw "Expected physical tag write AND completion callback did not occur for $($qa.Name)."
        }
        & python (Join-Path $PSScriptRoot '../tests/verify-runner-physical-tags.py') $original $qa.File
        if($LASTEXITCODE -ne 0){throw "QA $($qa.Name) metadata preservation failed"}
        $stableHash=(Get-FileHash -LiteralPath $qa.File -Algorithm SHA256).Hash
        $stableTicks=(Get-Item -LiteralPath $qa.File).LastWriteTimeUtc.Ticks
        $noOp=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $command -PassThru
        if(-not $noOp.WaitForExit(15000) -or $noOp.ExitCode -ne 0){
            throw "No-op test-only $($qa.Name) metadata command failed."
        }
        $secondConfirmed=$false
        for($n=0;$n -lt 60;$n++){
            Start-Sleep -Milliseconds 300
            if((Test-Path -LiteralPath $completion -PathType Leaf) -and
               (Get-Content -LiteralPath $completion -Raw).Trim() -ceq '2'){
                $secondConfirmed=$true;break
            }
            $proc.Refresh()
            if($proc.HasExited){throw "Foobar exited during $($qa.Name) no-op test."}
        }
        if(-not $secondConfirmed){
            throw "QA $($qa.Name) identical second tag command lacked success/no-op callback."
        }
        if($qa.Name -eq 'FLAC'){
            $stage=Join-Path $dir 'qa-flac-stage-evidence.txt'
            if(-not (Test-Path -LiteralPath $stage -PathType Leaf)){
                throw 'Missing FLAC physical readback evidence.'
            }
            $observed=@(Get-Content -LiteralPath $stage)
            if($observed.Count -ne 2 -or
               $observed[0] -cne 'count=0 unchanged=0' -or
               $observed[1] -cne 'count=1 unchanged=1'){
                throw ('FLAC physical metadata readback did not expose the unchanged tag: '+($observed -join '|'))
            }
            Write-Host "DISCOGGER_FLAC_PHYSICAL_SDK_READBACK_$Architecture=PASS"
        }
        $bytesChanged=(Get-FileHash -LiteralPath $qa.File -Algorithm SHA256).Hash -ne $stableHash
        $timeChanged=(Get-Item -LiteralPath $qa.File).LastWriteTimeUtc.Ticks -ne $stableTicks
        if($bytesChanged -or $timeChanged){
            if($qa.Name -eq 'FLAC'){
                $stage=Join-Path $dir 'qa-flac-stage-evidence.txt'
                if(Test-Path -LiteralPath $stage -PathType Leaf){
                    Write-Host ('DISCOGGER_QA_FLAC_STAGES='+((Get-Content -LiteralPath $stage) -join '|'))
                    $reader=Join-Path $dir 'qa-flac-input-reader-evidence.txt'
                    if(Test-Path -LiteralPath $reader -PathType Leaf){
                        Write-Host ('DISCOGGER_QA_FLAC_SDK_READ='+((Get-Content -LiteralPath $reader) -join '|'))
                    }
                }
            }
            Write-Host ("DISCOGGER_QA_NOOP_DIAGNOSTIC="+$qa.Name+
              " bytesChanged="+$bytesChanged+" mtimeChanged="+$timeChanged)
            throw "QA $($qa.Name) identical second tag commit changed bytes or timestamp."
        }
        Write-Host ("DISCOGGER_PHYSICAL_TAG_WRITE_NOOP_"+$qa.Name+"_"+$Architecture+"=PASS")
    }
    # Real foobar SDK must refuse virtual CUE and duplicate physical targets
    # atomically. Check exact bytes and timestamps on disposable fixtures.
    $guardBefore=@{}
    foreach($path in @($mp3,$flac)){
        $guardBefore[$path]=@{ Hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;
          Ticks=(Get-Item -LiteralPath $path).LastWriteTimeUtc.Ticks }
    }
    $denial=Join-Path $dir 'qa-unsafe-targets-rejected.txt'
    if(Test-Path -LiteralPath $denial){throw 'Unsafe target evidence unexpectedly pre-exists'}
    $guardCommand=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA unsafe targets"' -PassThru
    if(-not $guardCommand.WaitForExit(15000) -or $guardCommand.ExitCode -ne 0) {
        throw 'Could not dispatch native physical target rejection QA.'
    }
    $guardEvidence=$false
    for($n=0;$n -lt 60;$n++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $denial -PathType Leaf) {
            $evidence=Get-Content -LiteralPath $denial -Raw
            if($evidence.Contains('VIRTUAL=REJECTED') -and $evidence.Contains('DUPLICATE=REJECTED')){
                $guardEvidence=$true;break
            }
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Host died during unsafe-target write rejection'}
    }
    if(-not $guardEvidence){throw 'Native virtual/duplicate write denial not confirmed'}
    foreach($path in @($mp3,$flac)){
        if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $guardBefore[$path].Hash -or
           (Get-Item -LiteralPath $path).LastWriteTimeUtc.Ticks -ne $guardBefore[$path].Ticks) {
            throw "Physical target changed after rejected write: $path"
        }
    }
    Write-Host "DISCOGGER_PHYSICAL_VIRTUAL_DUPLICATE_DENIAL_$Architecture=PASS"

    # Actual SDK batch must reject a missing/stale physical snapshot before
    # touching either disposable MP3 or FLAC (no partial preflight commit).
    $beforeBatch=@{}
    foreach($path in @($mp3,$flac)){
        $beforeBatch[$path]=@{ Hash=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash;
          Ticks=(Get-Item -LiteralPath $path).LastWriteTimeUtc.Ticks }
    }
    $batchMarker=Join-Path $dir 'qa-incomplete-batch-rejected.txt'
    if(Test-Path -LiteralPath $batchMarker){throw 'Batch preflight marker already exists.'}
    $batchCommand=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA incomplete batch"' -PassThru
    if(-not $batchCommand.WaitForExit(15000) -or $batchCommand.ExitCode -ne 0){
        throw 'Unable to dispatch native incomplete-batch QA.'
    }
    $batchObserved=$false
    for($n=0;$n -lt 60;$n++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $batchMarker -PathType Leaf){
            $data=(Get-Content -LiteralPath $batchMarker -Raw).Trim()
            if($data -ceq 'BATCH=REJECTED_BEFORE_IO'){$batchObserved=$true;break}
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Foobar exited during physical batch preflight QA.'}
    }
    if(-not $batchObserved){throw 'No native incomplete-batch preflight evidence.'}
    foreach($path in @($mp3,$flac)){
        if((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $beforeBatch[$path].Hash -or
          (Get-Item -LiteralPath $path).LastWriteTimeUtc.Ticks -ne $beforeBatch[$path].Ticks) {
            throw "Incomplete-batch preflight modified physical media: $path"
        }
    }
    Write-Host "DISCOGGER_PHYSICAL_BATCH_PREFLIGHT_ATOMIC_$Architecture=PASS"

    # A simulated external tag editor modifies a disposable MP3 after
    # Discogger's snapshot. The real writer must fail closed without changing
    # that external edit or the physical file's bytes and mtime.
    $staleMarker=Join-Path $dir 'qa-stale-disk-rejected.txt'
    if(Test-Path -LiteralPath $staleMarker){throw 'Stale QA marker unexpectedly exists.'}
    $staleProcess=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA stale disk"' -PassThru
    if(-not $staleProcess.WaitForExit(15000) -or $staleProcess.ExitCode -ne 0) {
        throw 'Could not dispatch actual SDK stale physical metadata QA.'
    }
    $staleDone=$false
    for($i=0;$i -lt 60;$i++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $staleMarker -PathType Leaf){
            $proof=Get-Content -LiteralPath $staleMarker -Raw
            if($proof.Contains('STALE=REJECTED') -and $proof.Contains('EXTERNAL=RETAINED')){
                $staleDone=$true;break
            }
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Foobar exited during external-edit race test.'}
    }
    if(-not $staleDone){throw 'Missing actual foobar physical stale-snapshot rejection.'}
    Write-Host "DISCOGGER_PHYSICAL_STALE_DISK_REJECTION_$Architecture=PASS"

    # A real host must reject even a valid physical FLAC snapshot if the
    # pending changes remove GENRE without approval or flat-edit CUESHEET.
    $removalBefore=@{Hash=(Get-FileHash -LiteralPath $flac -Algorithm SHA256).Hash;
       Ticks=(Get-Item -LiteralPath $flac).LastWriteTimeUtc.Ticks}
    $removalMarker=Join-Path $dir 'qa-unapproved-removal-denied.txt'
    if(Test-Path -LiteralPath $removalMarker){throw 'Unexpected destructive-edit marker'}
    $removalCommand=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA unsafe removals"' -PassThru
    if(-not $removalCommand.WaitForExit(15000) -or $removalCommand.ExitCode -ne 0){
        throw 'Could not dispatch native destructive-edit denial QA.'
    }
    $removalPassed=$false
    for($n=0;$n -lt 60;$n++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $removalMarker -PathType Leaf){
            $proof=Get-Content -LiteralPath $removalMarker -Raw
            if($proof.Contains('GENRE=DENIED') -and $proof.Contains('CUESHEET=DENIED')){
                $removalPassed=$true;break
            }
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Host exited during destructive-edit QA'}
    }
    if(-not $removalPassed){throw 'No native destructive-edit denial proof.'}
    if((Get-FileHash -LiteralPath $flac -Algorithm SHA256).Hash -ne $removalBefore.Hash -or
      (Get-Item -LiteralPath $flac).LastWriteTimeUtc.Ticks -ne $removalBefore.Ticks){
        throw 'Unapproved edit changed synthetic FLAC bytes or timestamp.'
    }
    Write-Host "DISCOGGER_PHYSICAL_UNAPPROVED_REMOVAL_DENIAL_$Architecture=PASS"
    # Inject failure after the first REAL synthetic MP3 commit. The second
    # FLAC is not written: the production readback must classify 1/1 and
    # invalidate the snapshots. This does NOT impersonate an SDK failure code.
    $partialBeforeMp3=(Get-FileHash -LiteralPath $mp3 -Algorithm SHA256).Hash
    $partialBeforeFlac=@{ Hash=(Get-FileHash -LiteralPath $flac -Algorithm SHA256).Hash;
        Ticks=(Get-Item -LiteralPath $flac).LastWriteTimeUtc.Ticks }
    $partialMarker=Join-Path $dir 'qa-injected-partial-audit.txt'
    if(Test-Path -LiteralPath $partialMarker){throw 'Injected-partial evidence already exists.'}
    $partialCmd=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA injected partial"' -PassThru
    if(-not $partialCmd.WaitForExit(15000) -or $partialCmd.ExitCode -ne 0){
        throw 'Unable to dispatch native injected-partial test.'
    }
    $partialObserved=$false
    for($i=0;$i -lt 60;$i++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $partialMarker -PathType Leaf){
            $proof=Get-Content -LiteralPath $partialMarker -Raw
            if($proof.Contains('FIRST=REQUESTED') -and $proof.Contains('SECOND=ORIGINAL') -and
               $proof.Contains('SNAPSHOTS=REVOKED')){$partialObserved=$true;break}
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Host exited during injected partial-commit test.'}
    }
    if(-not $partialObserved){
        $partialErrorMarker=Join-Path $dir 'qa-injected-partial-error.txt'
        if(Test-Path -LiteralPath $partialErrorMarker -PathType Leaf){
            $partialReason=(Get-Content -LiteralPath $partialErrorMarker -Raw).Trim()
            throw "Native injected-partial QA failed: $partialReason"
        }
        throw 'Native partial-commit physical readback evidence missing (command did not run or terminated without evidence).'
    }
    if((Get-FileHash -LiteralPath $mp3 -Algorithm SHA256).Hash -eq $partialBeforeMp3){
        throw 'Injected first-file physical write did not change synthetic MP3.'
    }
    if((Get-FileHash -LiteralPath $flac -Algorithm SHA256).Hash -ne $partialBeforeFlac.Hash -or
       (Get-Item -LiteralPath $flac).LastWriteTimeUtc.Ticks -ne $partialBeforeFlac.Ticks){
        throw 'Injected partial test unexpectedly modified the second synthetic FLAC.'
    }
    Write-Host "DISCOGGER_PHYSICAL_INJECTED_PARTIAL_READBACK_$Architecture=PASS"

    # Call the production sidecar writer only on a newly generated runner-local
    # fixture. Its native QA command verifies exact replacement, identical
    # byte/timestamp no-op and rejection of an empty update.
    $sidecarError=Join-Path $dir 'qa-sidecar-error.txt'
    if(Test-Path -LiteralPath $sidecarError){throw 'Sidecar artwork error marker already exists.'}
    $sidecarMarker=Join-Path $dir 'qa-sidecar-artwork.txt'
    if(Test-Path -LiteralPath $sidecarMarker){throw 'Sidecar artwork marker already exists.'}
    $sidecarCommand=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA sidecar artwork"' -PassThru
    if(-not $sidecarCommand.WaitForExit(15000) -or $sidecarCommand.ExitCode -ne 0){
        throw 'Could not dispatch native sidecar artwork QA.'
    }
    $sidecarPassed=$false
    for($n=0;$n -lt 60;$n++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $sidecarError -PathType Leaf){
            $stage=(Get-Content -LiteralPath $sidecarError -Raw).Trim()
            if($stage -notmatch '^STAGE=[a-z-]+$'){throw 'Malformed runner-local sidecar diagnostic.'}
            throw ("Native sidecar artwork QA failed at "+$stage)
        }
        if(Test-Path -LiteralPath $sidecarMarker -PathType Leaf){
            # C++ text-mode output is CRLF on Windows; compare logical lines,
            # not a hard-coded LF byte sequence.
            $proof=@(Get-Content -LiteralPath $sidecarMarker)
            if($proof.Count -eq 3 -and
               $proof[0] -ceq 'NOOP=PASS' -and
               $proof[1] -ceq 'REPLACE=PASS' -and
               $proof[2] -ceq 'EMPTY=DENIED'){
                $sidecarPassed=$true;break
            }
            throw 'Malformed runner-local sidecar success marker.'
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Host exited during sidecar artwork QA.'}
    }
    if(-not $sidecarPassed){throw 'Native sidecar artwork safety evidence missing.'}
    Write-Host "DISCOGGER_SIDECAR_ARTWORK_WRITE_NOOP_$Architecture=PASS"

    # Exercise production embedded artwork on the disposable FLAC only.
    $embeddedError=Join-Path $dir 'qa-embedded-error.txt'
    $embeddedMarker=Join-Path $dir 'qa-embedded-artwork.txt'
    if((Test-Path -LiteralPath $embeddedError) -or (Test-Path -LiteralPath $embeddedMarker)){
        throw 'Embedded artwork QA marker already exists.'
    }
    $embeddedCommand=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA embedded artwork"' -PassThru
    if(-not $embeddedCommand.WaitForExit(15000) -or $embeddedCommand.ExitCode -ne 0){
        throw 'Could not dispatch native embedded artwork QA.'
    }
    $embeddedPassed=$false
    for($n=0;$n -lt 60;$n++){
        Start-Sleep -Milliseconds 250
        if(Test-Path -LiteralPath $embeddedError -PathType Leaf){
            $stage=(Get-Content -LiteralPath $embeddedError -Raw).Trim()
            if($stage -notmatch '^STAGE=[a-z-]+$'){throw 'Malformed runner-local embedded diagnostic.'}
            throw ("Native embedded artwork QA failed at "+$stage)
        }
        if(Test-Path -LiteralPath $embeddedMarker -PathType Leaf){
            $proof=@(Get-Content -LiteralPath $embeddedMarker)
            if($proof.Count -eq 5 -and
               $proof[0] -ceq 'FRONT_NOOP=PASS' -and
               $proof[1] -ceq 'BACK_ADDED=PASS' -and
               $proof[2] -ceq 'BACK_NOOP=PASS' -and
               $proof[3] -ceq 'EMPTY=DENIED' -and
               $proof[4] -ceq 'META=UNCHANGED'){
                $embeddedPassed=$true;break
            }
            throw 'Malformed runner-local embedded success marker.'
        }
        $proc.Refresh()
        if($proc.HasExited){throw 'Host exited during embedded artwork QA.'}
    }
    if(-not $embeddedPassed){throw 'Native embedded artwork safety evidence missing.'}
    Write-Host "DISCOGGER_EMBEDDED_ARTWORK_WRITE_NOOP_$Architecture=PASS"

    if($AuthorizedLive){
        # Close the prior synthetic model and open a fresh ordinary Find Releases
        # dialog. Never use a test-populated release list as live search evidence.
        if(-not [DiscoggerWindowTest]::PostMessageW($activeDialog,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)){
            throw 'Could not close synthetic Find Releases before live OAuth test.'
        }
        for($n=0;$n -lt 35;$n++){
            Start-Sleep -Milliseconds 200
            if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count -eq 0){break}
        }
        if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count){
            throw 'Synthetic Find Releases window remained open before live search.'
        }
        $liveArgs='/context_command:"'+$chosenContextRoute+'" "'+$wav+'"'
        $liveLauncher=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $liveArgs -PassThru
        if(-not $liveLauncher.WaitForExit(15000) -or $liveLauncher.ExitCode -ne 0){
            throw 'Could not open fresh native Discogger Find Releases for live search.'
        }
        $liveDialog=[IntPtr]::Zero
        for($n=0;$n -lt 40;$n++){
            Start-Sleep -Milliseconds 250
            $proc.Refresh()
            if($proc.HasExited){throw 'Host exited before authenticated Find Releases search.'}
            $found=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
            if($found.Length -eq 1){$liveDialog=$found[0];break}
        }
        if($liveDialog -eq [IntPtr]::Zero){throw 'Fresh native Find Releases dialog not found.'}
        $searchEdit=[DiscoggerWindowTest]::GetDlgItem($liveDialog,1083)
        $searchButton=[DiscoggerWindowTest]::GetDlgItem($liveDialog,1086)
        $releaseTree=[DiscoggerWindowTest]::GetDlgItem($liveDialog,1018)
        if($searchEdit -eq [IntPtr]::Zero -or $searchButton -eq [IntPtr]::Zero -or
            $releaseTree -eq [IntPtr]::Zero){throw 'Native Discogger search controls missing.'}
        if([DiscoggerWindowTest]::TreeCount($releaseTree) -ne 0){
            throw 'Fresh authenticated GUI search is not starting with an empty release tree.'
        }
        # This query is a known exact public release; the disposable profile has
        # no Discogger search cache, so a populated tree requires a live request.
        $query='artist= Estiva & title= Via Balearia'
        [DiscoggerWindowTest]::SetEditText($searchEdit,$query)
        if([DiscoggerWindowTest]::EditText($searchEdit) -cne $query){
            throw 'Native Search text did not match the submitted known fixture query.'
        }
        [DiscoggerWindowTest]::ClickButton($searchButton)
        $postClickText=[DiscoggerWindowTest]::EditText($searchEdit)
        Write-Host "DISCOGGER_AUTHENTICATED_UI_POSTCLICK_${Architecture}=length:$($postClickText.Length) retained:$([int]($postClickText -ceq $query))"
        $liveCount=0
        for($n=0;$n -lt 120;$n++){
            Start-Sleep -Milliseconds 500
            $proc.Refresh()
            if($proc.HasExited){
                Write-DiscoggerCrashEvidence
                throw 'Host exited during authenticated Discogger search.'
            }
            $liveCount=[DiscoggerWindowTest]::TreeCount($releaseTree)
            if($liveCount -gt 0){break}
        }
        # The search fills the artist list before the release tree. Inspect the
        # exact, real result identity and open it via the native tree presenter.
        $liveProbe=Join-Path $dir 'qa-live-search-probe.txt'
        if(Test-Path -LiteralPath $liveProbe){throw 'Live-search probe evidence unexpectedly exists.'}
        $probeCmd=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA live search probe"' -PassThru
        if(-not $probeCmd.WaitForExit(15000) -or $probeCmd.ExitCode -ne 0){
            throw 'Could not dispatch native live-search result inspection.'
        }
        for($n=0;$n -lt 180;$n++){
            Start-Sleep -Milliseconds 250
            if(Test-Path -LiteralPath $liveProbe -PathType Leaf){break}
            $proc.Refresh()
            if($proc.HasExited){throw 'Host exited during live-search result inspection.'}
        }
        if(-not (Test-Path -LiteralPath $liveProbe -PathType Leaf)){
            throw 'Native live-search result inspection produced no evidence.'
        }
        $proof=Get-Content -LiteralPath $liveProbe -Raw
        $expected=@('ARTISTS','KNOWN_RELEASE_MATCHES','TREE','BUTTON','ROUTED',
            'INPUT_NONEMPTY','INPUT_NONEMPTY_CALLS','INPUT_EMPTY_CALLS',
            'INPUT_MEMBER_VALID','INPUT_HWND_MATCH','INPUT_MEMBER_LENGTH','INPUT_LIVE_LENGTH',
            'PARSED_FIELDS','QUERY_MINIMAL','QUERY_DETECTED',
            'ALT_MODE','REPEAT_QUERY','EARLY_EXIT',
            'WORKER_STARTED','WORKER_ADVANCED','WORKER_SUCCESS','WORKER_ERROR','WORKER_ABORT',
            'RESPONSE_PAGES','RESPONSE_ITEMS','PARSED_MASTERS','PARSED_RELEASES',
            'QUERY_LIMIT','API_IDENTITY','API_EXACT_OK','API_EXACT_ITEMS',
            'API_EXACT_KNOWN','API_BROAD_OK','API_BROAD_ITEMS','API_BROAD_KNOWN',
            'API_COHORT_TOTAL','API_COHORT_OK','API_COHORT_NONEMPTY','API_COHORT_KNOWN',
            'API_COHORT_MASK','API_COHORT_KNOWN_MASK',
            'API_COHORT_PAGE2_ATTEMPT_MASK','API_COHORT_PAGE2_OK_MASK',
            'API_COHORT_PAGE2_KNOWN_MASK','API_COHORT_SUNMOON_TARGETED_OK',
            'API_COHORT_SUNMOON_TARGETED_KNOWN')
        $counts=@{}
        foreach($line in ($proof -split '\r?\n')){
            if($line -match '^([A-Z_]+)=(\d+)$' -and $Matches[1] -in $expected){
                $counts[$Matches[1]]=[int]$Matches[2]
            }
        }
        if($counts.Count -ne $expected.Count){throw 'Malformed native live-search stage evidence.'}
        Write-Host "DISCOGGER_AUTHENTICATED_GUI_RESULT_COUNTS_${Architecture}=artists:$($counts['ARTISTS']) known:$($counts['KNOWN_RELEASE_MATCHES']) tree:$($counts['TREE'])"
        Write-Host "DISCOGGER_AUTHENTICATED_SEARCH_STAGES_${Architecture}=button:$($counts['BUTTON']) routed:$($counts['ROUTED']) started:$($counts['WORKER_STARTED']) advanced:$($counts['WORKER_ADVANCED']) success:$($counts['WORKER_SUCCESS']) error:$($counts['WORKER_ERROR']) abort:$($counts['WORKER_ABORT']) pages:$($counts['RESPONSE_PAGES']) items:$($counts['RESPONSE_ITEMS']) masters:$($counts['PARSED_MASTERS']) releases:$($counts['PARSED_RELEASES']) capped:$($counts['QUERY_LIMIT'])"
        Write-Host "DISCOGGER_AUTHENTICATED_UI_BOUNDARY_${Architecture}=calls:$($counts['BUTTON']) nonempty:$($counts['INPUT_NONEMPTY_CALLS']) empty:$($counts['INPUT_EMPTY_CALLS']) member_valid:$($counts['INPUT_MEMBER_VALID']) hwnd_match:$($counts['INPUT_HWND_MATCH']) member_length:$($counts['INPUT_MEMBER_LENGTH']) live_length:$($counts['INPUT_LIVE_LENGTH'])"
        Write-Host "DISCOGGER_AUTHENTICATED_GUI_PARSE_${Architecture}=input:$($counts['INPUT_NONEMPTY']) fields:$($counts['PARSED_FIELDS']) minimal:$($counts['QUERY_MINIMAL']) query:$($counts['QUERY_DETECTED']) alt:$($counts['ALT_MODE']) retry:$($counts['REPEAT_QUERY']) exit:$($counts['EARLY_EXIT'])"
        Write-Host "DISCOGGER_AUTHENTICATED_API_PROBE_${Architecture}=identity:$($counts['API_IDENTITY']) exact_ok:$($counts['API_EXACT_OK']) exact_items:$($counts['API_EXACT_ITEMS']) exact_known:$($counts['API_EXACT_KNOWN']) broad_ok:$($counts['API_BROAD_OK']) broad_items:$($counts['API_BROAD_ITEMS']) broad_known:$($counts['API_BROAD_KNOWN'])"
        Write-Host "DISCOGGER_AUTHENTICATED_API_COHORT_${Architecture}=total:$($counts['API_COHORT_TOTAL']) completed:$($counts['API_COHORT_OK']) nonempty:$($counts['API_COHORT_NONEMPTY']) expected_on_page:$($counts['API_COHORT_KNOWN']) nonempty_mask:$($counts['API_COHORT_MASK']) known_mask:$($counts['API_COHORT_KNOWN_MASK'])"
        # Diagnose only, never reclassify original page-one API evidence.
        Write-Host "DISCOGGER_AUTHENTICATED_API_COHORT_FOLLOWUP_${Architecture}=page2_attempt_mask:$($counts['API_COHORT_PAGE2_ATTEMPT_MASK']) page2_ok_mask:$($counts['API_COHORT_PAGE2_OK_MASK']) page2_known_mask:$($counts['API_COHORT_PAGE2_KNOWN_MASK']) sunmoon_targeted_ok:$($counts['API_COHORT_SUNMOON_TARGETED_OK']) sunmoon_targeted_known:$($counts['API_COHORT_SUNMOON_TARGETED_KNOWN'])"
        if($counts['API_COHORT_TOTAL'] -ne 14 -or $counts['API_COHORT_OK'] -ne 14){
            throw 'Authenticated independent Discogs query corpus was incomplete.'
        }
        # A few historic exact-format releases can be absent from the current
        # search index; the full bitmap remains visible as *numeric* evidence.
        # Detect systemic regressions while preserving individual diagnostics.
        if($counts['API_COHORT_NONEMPTY'] -lt 9){
            throw 'Authenticated independent Discogs query corpus had fewer than nine populated searches.'
        }
        if($counts['BUTTON'] -lt 1){throw 'Native Search button did not reach its actual Discogger handler.'}
        if($counts['ROUTED'] -lt 1){throw ('Discogger Search handler did not route its parsed query; early-exit stage '+$counts['EARLY_EXIT']+'.')}
        if($counts['WORKER_STARTED'] -lt 1){throw 'Discogger routed search but the asynchronous worker never started.'}
        if($counts['WORKER_ADVANCED'] -lt 1){throw 'Discogger parsed advanced query was routed to plain artist-name search.'}
        if($counts['WORKER_ERROR'] -gt 0){throw 'Discogger asynchronous search failed before publishing results.'}
        if($counts['WORKER_ABORT'] -gt 0){throw 'Discogger asynchronous search was aborted.'}
        if($counts['QUERY_LIMIT'] -gt 0){throw 'Discogger search was incorrectly stopped by its configured result cap.'}
        if($counts['ARTISTS'] -lt 1){
            if($counts['API_IDENTITY'] -lt 1){throw 'Discogger OAuth identity request failed independently of the GUI.'}
            if($counts['API_EXACT_OK'] -lt 1 -and $counts['API_BROAD_OK'] -lt 1){
                throw 'Discogger direct API search probes failed independently of the GUI.'
            }
            if($counts['RESPONSE_PAGES'] -gt 0 -and
               ($counts['PARSED_MASTERS'] -gt 0 -or $counts['PARSED_RELEASES'] -gt 0)){
                throw 'Discogger parsed actual releases but did not publish artist rows.'
            }
            throw 'Discogger worker completed but produced no artist-list rows.'
        }
        if($counts['KNOWN_RELEASE_MATCHES'] -lt 1){throw 'Authenticated Discogger results did not contain expected release 38012673.'}
        $liveCount=[DiscoggerWindowTest]::TreeCount($releaseTree)
        if($liveCount -lt 1 -or $counts['TREE'] -lt 1){
            throw 'Authenticated Discogger result was not displayed in the native release tree.'
        }
        Write-Host "DISCOGGER_AUTHENTICATED_GUI_SEARCH_$Architecture=PASS"
        # Search must be read-only even after the prior isolated writer tests.
        if((Get-FileHash -LiteralPath $wav -Algorithm SHA256).Hash -ne $wavHash){
            throw 'Authenticated Discogger search changed disposable WAVE data.'
        }

        # Extra *real* native Find Releases searches on a single running,
        # disposable host. Close each prior dialog, preserve its SQLite cache,
        # and require automatic tree publication without a QA presenter click.
        # The final repeat exercises the cached-query/artist route.
        $nativeMatrix=@(
            # Original user cases must actually populate the native release tree.
            # Prove both the precise previously failing composite query and the
            # independently observed Maywave workaround, with no artist click.
            @{Id='37332711';Query='artist= Kyau & Albert X Maywave & title= 1000 Years & track= 1000 Years (Dj Version)';Name='kyau-original-composite';MinFields=4},
            @{Id='37332711';Query='q= Maywave & title= 1000 Years';Name='kyau-maywave';MinFields=3},
            @{Id='36397609';Query='artist= Matt Fax & title= Only You';Name='matt-fax-only-you';MinFields=3},
            @{Id='36819730';Query='artist= Matt Fax & title= Till You Let Go';Name='matt-fax-till-you-let-go';MinFields=3},
            # Fifth mandatory missing-release identity: native result must be visible without artist reselect.
            @{Id='35534398';Query='artist= Quivver & title= Consciousness EP';Name='quivver-consciousness-ep';MinFields=3},
            @{Id='2844';Query='artist= Daft Punk & title= One More Time';Name='daft-punk';MinFields=3},
            @{Id='7809';Query='artist= The Chemical Brothers & title= Hey Boy Hey Girl';Name='chemical-brothers';MinFields=3},
            @{Id='32559';Query='artist= Björk & title= Jóga';Name='bjork-utf8';MinFields=3},
            @{Id='294906';Query='artist= Röyksopp & title= Eple';Name='royksopp-utf8';MinFields=3},
            # Stress field order/case, collaborations, UTF-8, remix descriptors,
            # and titles where the release differs from the track.
            @{Id='37332711';Query='track= 1000 Years (Dj Version) & title= 1000 Years & artist= Kyau & Albert X Maywave';Name='kyau-reordered';MinFields=4},
            @{Id='37332711';Query='TITLE = 1000 Years & TRACK = 1000 Years (DJ Version) & ARTIST = Kyau & Albert X Maywave';Name='kyau-uppercase-fields';MinFields=4},
            @{Id='2783977';Query='artist= Above & Beyond feat. Richard Bedford & title= Sun & Moon & track= Sun & Moon (Club Mix)';Name='sun-moon-ampersands';MinFields=4},
            @{Id='32559';Query='TRACK = Jóga (Howie B) & ARTIST = Björk & TITLE = Jóga';Name='bjork-utf8-reordered';MinFields=4},
            @{Id='294906';Query='ARTIST = Röyksopp & TITLE = Eple & TRACK = Eple (Edit)';Name='royksopp-utf8-mix';MinFields=4},
            @{Id='18278686';Query='track= Glue & artist= Bicep & title= Glue EP';Name='bicep-ep-track';MinFields=4},
            @{Id='13584056';Query='title= Born Slippy .NUXX & track= Born Slippy / NUXX (Mix) & artist= Underworld';Name='underworld-punctuation';MinFields=4},
            @{Id='36397609';Query='track= Only You (Extended Mix) & artist= Matt Fax & title= Only You';Name='matt-fax-extended-reordered';MinFields=4},
            @{Id='36819730';Query='title= Till You Let Go & track= Till You Let Go (Extended Mix) & artist= Matt Fax';Name='matt-fax-extended-three-fields';MinFields=4},
            @{Id='18370';Query='artist= Chicane Featuring Maire Brennan & title= Saltwater & track= Saltwater (Original Mix)';Name='chicane-featuring';MinFields=4},
            @{Id='82825';Query='artist= Faithless & title= Insomnia & track= Insomnia (Monster Mix)';Name='faithless-remix';MinFields=4},
            @{Id='2844';Query='artist= Daft Punk & title= One More Time';Name='daft-punk-cache-repeat';MinFields=3}
        )
        $expectedIdFile=Join-Path $dir 'qa-live-expected-id.txt'
        foreach($item in $nativeMatrix){
            if(-not [DiscoggerWindowTest]::PostMessageW($liveDialog,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)){
                throw 'Could not close the preceding native query dialog.'
            }
            for($n=0;$n -lt 35;$n++){
                Start-Sleep -Milliseconds 200
                if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count -eq 0){break}
            }
            if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count){
                throw 'Preceding native query dialog did not close.'
            }
            [IO.File]::WriteAllText($expectedIdFile,[string]$item.Id,[Text.Encoding]::ASCII)
            $launcher=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $liveArgs -PassThru
            if(-not $launcher.WaitForExit(15000) -or $launcher.ExitCode -ne 0){
                throw 'Could not open a fresh native Discogger GUI matrix dialog.'
            }
            $liveDialog=[IntPtr]::Zero
            for($n=0;$n -lt 40;$n++){
                Start-Sleep -Milliseconds 250
                $proc.Refresh()
                if($proc.HasExited){throw 'Host exited before a native GUI matrix query.'}
                $found=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
                if($found.Length -eq 1){$liveDialog=$found[0];break}
            }
            if($liveDialog -eq [IntPtr]::Zero){throw 'Fresh native GUI matrix dialog missing.'}
            $searchEdit=[DiscoggerWindowTest]::GetDlgItem($liveDialog,1083)
            $searchButton=[DiscoggerWindowTest]::GetDlgItem($liveDialog,1086)
            $releaseTree=[DiscoggerWindowTest]::GetDlgItem($liveDialog,1018)
            if($searchEdit -eq [IntPtr]::Zero -or $searchButton -eq [IntPtr]::Zero -or
               $releaseTree -eq [IntPtr]::Zero -or
               [DiscoggerWindowTest]::TreeCount($releaseTree) -ne 0){
                throw 'GUI matrix case did not start with empty native controls.'
            }
            if(Test-Path -LiteralPath $liveProbe){Remove-Item -LiteralPath $liveProbe -Force}
            [DiscoggerWindowTest]::SetEditText($searchEdit,[string]$item.Query)
            if([DiscoggerWindowTest]::EditText($searchEdit) -cne [string]$item.Query){
                throw 'Native GUI matrix query was altered during input.'
            }
            [DiscoggerWindowTest]::ClickButton($searchButton)
            $done=$false
            $matrixCounts=@{}
            for($attempt=0;$attempt -lt 16 -and -not $done;$attempt++){
                Start-Sleep -Seconds 4
                $proc.Refresh()
                if($proc.HasExited){
                    Write-DiscoggerCrashEvidence
                    throw 'Host exited during a native GUI matrix query.'
                }
                if(Test-Path -LiteralPath $liveProbe){Remove-Item -LiteralPath $liveProbe -Force}
                $probeCmd=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA live search probe"' -PassThru
                if(-not $probeCmd.WaitForExit(15000) -or $probeCmd.ExitCode -ne 0){
                    throw 'Native GUI matrix probe dispatch failed.'
                }
                for($n=0;$n -lt 80;$n++){
                    Start-Sleep -Milliseconds 250
                    if(Test-Path -LiteralPath $liveProbe -PathType Leaf){break}
                }
                if(-not (Test-Path -LiteralPath $liveProbe -PathType Leaf)){
                    throw 'Native GUI matrix probe evidence missing.'
                }
                $matrixCounts=@{}
                foreach($line in ((Get-Content -LiteralPath $liveProbe -Raw) -split '\r?\n')){
                    if($line -match '^([A-Z_]+)=(\d+)$'){
                        $matrixCounts[$Matches[1]]=[int]$Matches[2]
                    }
                }
                if($matrixCounts['WORKER_ERROR'] -gt 0 -or $matrixCounts['WORKER_ABORT'] -gt 0){
                    throw 'Native GUI matrix worker failed or aborted.'
                }
                $done=$matrixCounts['WORKER_SUCCESS'] -eq 1
            }
            if(-not $done){throw 'Native GUI matrix search worker did not finish.'}
            if($matrixCounts['QUERY_DETECTED'] -ne 1 -or
               $matrixCounts['QUERY_MINIMAL'] -ne 1 -or
               $matrixCounts['EARLY_EXIT'] -ne 0 -or
               $matrixCounts['PARSED_FIELDS'] -lt [int]$item.MinFields){
                throw "Complex native search parser dropped or rejected requested fields: $($item.Name)"
            }
            if($matrixCounts['BUTTON'] -ne 1 -or $matrixCounts['ROUTED'] -ne 1 -or
                $matrixCounts['WORKER_ADVANCED'] -ne 1 -or
                $matrixCounts['ARTISTS'] -lt 1 -or
                $matrixCounts['KNOWN_RELEASE_MATCHES'] -lt 1){
                throw 'Native GUI matrix did not find the expected public release identity.'
            }
            if($matrixCounts['TREE'] -lt 1 -or
                [DiscoggerWindowTest]::TreeCount($releaseTree) -lt 1){
                throw 'Native GUI matrix did not auto-show results without artist reselection.'
            }
            if((Get-FileHash -LiteralPath $wav -Algorithm SHA256).Hash -ne $wavHash){
                throw 'Native GUI matrix changed disposable audio bytes.'
            }
            Write-Host "DISCOGGER_AUTHENTICATED_NATIVE_PARSE_${Architecture}_$($item.Name)=fields:$($matrixCounts['PARSED_FIELDS']) minimum:$($item.MinFields)"
            Write-Host "DISCOGGER_AUTHENTICATED_NATIVE_QUERY_${Architecture}_$($item.Name)=PASS"
        }
        if(Test-Path -LiteralPath $expectedIdFile){Remove-Item -LiteralPath $expectedIdFile -Force}

        # A fresh negative search must complete without a thrown no-results
        # status or a stale tree from the successful search above.
        if(-not [DiscoggerWindowTest]::PostMessageW($liveDialog,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)){
            throw 'Could not close positive Find Releases before empty-result regression.'
        }
        for($n=0;$n -lt 35;$n++){
            Start-Sleep -Milliseconds 200
            if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count -eq 0){break}
        }
        if(@([DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)).Count){
            throw 'Positive Find Releases remained open before empty-result regression.'
        }
        $emptyLauncher=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList $liveArgs -PassThru
        if(-not $emptyLauncher.WaitForExit(15000) -or $emptyLauncher.ExitCode -ne 0){
            throw 'Could not open independent Find Releases for empty-result regression.'
        }
        $emptyDialog=[IntPtr]::Zero
        for($n=0;$n -lt 40;$n++){
            Start-Sleep -Milliseconds 250
            $proc.Refresh()
            if($proc.HasExited){throw 'Host exited before empty-result regression.'}
            $found=[DiscoggerWindowTest]::FindReleaseDialogs([uint32]$proc.Id)
            if($found.Length -eq 1){$emptyDialog=$found[0];break}
        }
        if($emptyDialog -eq [IntPtr]::Zero){throw 'Independent empty-result dialog not found.'}
        $emptyEdit=[DiscoggerWindowTest]::GetDlgItem($emptyDialog,1083)
        $emptyButton=[DiscoggerWindowTest]::GetDlgItem($emptyDialog,1086)
        $emptyTree=[DiscoggerWindowTest]::GetDlgItem($emptyDialog,1018)
        if($emptyEdit -eq [IntPtr]::Zero -or $emptyButton -eq [IntPtr]::Zero -or
            $emptyTree -eq [IntPtr]::Zero -or [DiscoggerWindowTest]::TreeCount($emptyTree) -ne 0){
            throw 'Independent empty-result controls were not ready.'
        }
        $emptyQuery='artist= DiscoggerQAImpossibleArtist251024 & title= DiscoggerQAImpossibleRelease251024'
        [DiscoggerWindowTest]::SetEditText($emptyEdit,$emptyQuery)
        if([DiscoggerWindowTest]::EditText($emptyEdit) -cne $emptyQuery){
            throw 'Empty-result regression query was not set in the native edit.'
        }
        [DiscoggerWindowTest]::ClickButton($emptyButton)
        Remove-Item -LiteralPath $liveProbe -Force
        $emptyCompleted=$false
        for($attempt=0;$attempt -lt 4 -and -not $emptyCompleted;$attempt++){
            Start-Sleep -Seconds 10
            $proc.Refresh()
            if($proc.HasExited){
                Write-DiscoggerCrashEvidence
                throw 'Host exited while rendering an empty authenticated search.'
            }
            $probeCmd=Start-Process -FilePath $exe -WorkingDirectory $hostDir -ArgumentList '/command:"Discogger QA live search probe"' -PassThru
            if(-not $probeCmd.WaitForExit(15000) -or $probeCmd.ExitCode -ne 0){
                throw 'Could not inspect empty-result native search.'
            }
            for($n=0;$n -lt 80;$n++){
                Start-Sleep -Milliseconds 250
                if(Test-Path -LiteralPath $liveProbe -PathType Leaf){break}
            }
            if(-not (Test-Path -LiteralPath $liveProbe -PathType Leaf)){
                throw 'Empty-result native probe did not produce evidence.'
            }
            $emptyProof=Get-Content -LiteralPath $liveProbe -Raw
            Remove-Item -LiteralPath $liveProbe -Force
            $emptyCounts=@{}
            foreach($line in ($emptyProof -split '\r?\n')){
                if($line -match '^([A-Z_]+)=(\d+)$'){$emptyCounts[$Matches[1]]=[int]$Matches[2]}
            }
            if($emptyCounts['WORKER_ERROR'] -gt 0 -or $emptyCounts['WORKER_ABORT'] -gt 0){
                throw 'Empty-result native search ended in worker error or abort.'
            }
            if($emptyCounts['WORKER_SUCCESS'] -eq 1){
                if($emptyCounts['BUTTON'] -ne 1 -or $emptyCounts['ROUTED'] -ne 1 -or
                   $emptyCounts['WORKER_ADVANCED'] -ne 1 -or $emptyCounts['ARTISTS'] -ne 0 -or
                   $emptyCounts['TREE'] -ne 0 -or $emptyCounts['PARSED_RELEASES'] -ne 0 -or
                   $emptyCounts['PARSED_MASTERS'] -ne 0 -or
                   [DiscoggerWindowTest]::TreeCount($emptyTree) -ne 0){
                    throw 'Empty-result search retained or published unexpected GUI rows.'
                }
                $emptyCompleted=$true
            }
        }
        if(-not $emptyCompleted){throw 'Authenticated empty-result native search did not complete.'}
        Write-Host "DISCOGGER_AUTHENTICATED_EMPTY_RESULT_$Architecture=PASS"
        if((Get-FileHash -LiteralPath $wav -Algorithm SHA256).Hash -ne $wavHash){
            throw 'Empty-result Discogger search changed disposable WAVE data.'
        }
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
