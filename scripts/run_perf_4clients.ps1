param(
    [string]$BuildDir = '_generated\build-mingw',
    [int]$DurationSeconds = 30,
    [int]$InputHz = 10,
    [string]$OutputRoot = '_generated\perf_runs',
    [switch]$SkipBuild
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoPath {
    param([string]$Path)
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return [System.IO.Path]::GetFullPath($Path)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\$Path"))
}

function Add-ToolPathIfPresent {
    param([string]$Path)
    if ((Test-Path -LiteralPath $Path) -and ($env:PATH -notlike "*$Path*")) {
        $env:PATH = "$Path;$env:PATH"
    }
}

function Assert-Command {
    param([string]$Name)
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command '$Name' was not found in PATH"
    }
    return $command.Source
}

function New-PerfConfig {
    param(
        [string]$Path,
        [int]$AsPort,
        [int]$TgsPort,
        [int]$VPort
    )

@"
C1_ID=0x01
C2_ID=0x02
C3_ID=0x03
C4_ID=0x04
AS_ID=0x11
TGS_ID=0x12
V_ID=0x13
LOCAL_CLIENT_ID=0x01
AS_BIND_IP=127.0.0.1
AS_IP=127.0.0.1
AS_HOST=127.0.0.1
AS_PORT=$AsPort
TGS_BIND_IP=127.0.0.1
TGS_IP=127.0.0.1
TGS_HOST=127.0.0.1
TGS_PORT=$TgsPort
V_BIND_IP=127.0.0.1
V_IP=127.0.0.1
V_HOST=127.0.0.1
V_PORT=$VPort
C1_PASSWORD=123456
C1_KC=0x59ef3db7cb8c8d
C2_PASSWORD=admin123
C2_KC=0x6a73a4ebe9c564
C3_PASSWORD=hehe12345
C3_KC=0x57ef9d5f45ab7b
C4_PASSWORD=&wxh@147
C4_KC=0xec3eb766d59086
KTGS=0x1c24deeecc136e
KV=0x3398481d2a89f6
PK_CA_N=0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3
PK_CA_E=0x10001
SK_CA_D=0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69
"@ | Set-Content -LiteralPath $Path -Encoding ASCII
}

function Start-RoleProcess {
    param(
        [string]$ExePath,
        [string[]]$Arguments,
        [string]$Name,
        [string]$WorkDir
    )

    Start-Process `
        -FilePath $ExePath `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $WorkDir "$Name.out") `
        -RedirectStandardError (Join-Path $WorkDir "$Name.err") `
        -PassThru
}

function Get-ProcessSnapshot {
    param([System.Diagnostics.Process[]]$Processes)
    $rows = @()
    foreach ($process in $Processes) {
        if (-not $process) {
            continue
        }
        $live = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($live) {
            $rows += [pscustomobject]@{
                Name = $live.ProcessName
                Id = $live.Id
                Cpu = [double]$live.CPU
                WorkingSet = [int64]$live.WorkingSet64
            }
        }
    }
    return $rows
}

function Get-FileSnapshot {
    param([string]$Root)
    $rows = @()
    if (-not (Test-Path -LiteralPath $Root)) {
        return $rows
    }
    $rootFull = [System.IO.Path]::GetFullPath($Root).TrimEnd('\', '/')
    $rootPrefix = $rootFull + [System.IO.Path]::DirectorySeparatorChar
    foreach ($file in Get-ChildItem -LiteralPath $Root -Recurse -File) {
        $fileFull = [System.IO.Path]::GetFullPath($file.FullName)
        if ($fileFull.StartsWith($rootPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            $relative = $fileFull.Substring($rootPrefix.Length)
        } else {
            $relative = $fileFull
        }
        $rows += [pscustomobject]@{
            Relative = $relative
            Length = [int64]$file.Length
        }
    }
    return $rows
}

function Format-ProcessDelta {
    param(
        [object[]]$Before,
        [object[]]$After,
        [double]$ElapsedSeconds
    )
    $rows = @()
    foreach ($start in $Before) {
        $end = $After | Where-Object { $_.Id -eq $start.Id } | Select-Object -First 1
        if (-not $end) {
            continue
        }
        $cpuDelta = [Math]::Max(0.0, [double]$end.Cpu - [double]$start.Cpu)
        $rows += [pscustomobject]@{
            Process = $start.Name
            Id = $start.Id
            CpuSeconds = [Math]::Round($cpuDelta, 3)
            OneCorePct = [Math]::Round(($cpuDelta / [Math]::Max(0.001, $ElapsedSeconds)) * 100.0, 1)
            WorkingSetMB = [Math]::Round(([double]$end.WorkingSet / 1MB), 2)
        }
    }
    return $rows
}

function Format-FileDelta {
    param(
        [object[]]$Before,
        [object[]]$After,
        [double]$ElapsedSeconds
    )
    $rows = @()
    foreach ($end in $After) {
        $start = $Before | Where-Object { $_.Relative -eq $end.Relative } | Select-Object -First 1
        $startLength = if ($start) { [int64]$start.Length } else { [int64]0 }
        $delta = [int64]$end.Length - $startLength
        if ($delta -le 0) {
            continue
        }
        $rows += [pscustomobject]@{
            File = $end.Relative
            DeltaKB = [Math]::Round(([double]$delta / 1KB), 2)
            KBps = [Math]::Round((([double]$delta / [Math]::Max(0.001, $ElapsedSeconds)) / 1KB), 2)
            TotalMB = [Math]::Round(([double]$end.Length / 1MB), 2)
        }
    }
    return $rows | Sort-Object DeltaKB -Descending
}

function Stop-RoleProcesses {
    param([System.Diagnostics.Process[]]$Processes)
    foreach ($process in $Processes) {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
}

if ($DurationSeconds -le 0) {
    throw 'DurationSeconds must be positive'
}
if ($InputHz -le 0) {
    throw 'InputHz must be positive'
}

Add-ToolPathIfPresent 'E:\Qt\Tools\CMake_64\bin'
Add-ToolPathIfPresent 'E:\Qt\Tools\Ninja'
Add-ToolPathIfPresent 'E:\Qt\Tools\mingw1120_64\bin'

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$buildDirPath = Resolve-RepoPath $BuildDir
$outputRootPath = Resolve-RepoPath $OutputRoot
New-Item -ItemType Directory -Path $outputRootPath -Force | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$workDir = Join-Path $outputRootPath $stamp
New-Item -ItemType Directory -Path $workDir -Force | Out-Null

$cmakePath = Assert-Command 'cmake'
if (-not $SkipBuild) {
    & $cmakePath --build $buildDirPath --target as_server tgs_server v_server game_load_client
    if ($LASTEXITCODE -ne 0) {
        throw 'Build failed'
    }
}

$asExe = Join-Path $buildDirPath 'as_server.exe'
$tgsExe = Join-Path $buildDirPath 'tgs_server.exe'
$vExe = Join-Path $buildDirPath 'v_server.exe'
$loadExe = Join-Path $buildDirPath 'game_load_client.exe'
foreach ($exe in @($asExe, $tgsExe, $vExe, $loadExe)) {
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Missing executable: $exe"
    }
}

$basePort = Get-Random -Minimum 25000 -Maximum 43000
$configPath = Join-Path $workDir 'course_config.txt'
New-PerfConfig -Path $configPath -AsPort $basePort -TgsPort ($basePort + 1) -VPort ($basePort + 2)

$processes = @()
$loadExitCode = 1
$elapsedSeconds = [double]$DurationSeconds
try {
    $processes += Start-RoleProcess $asExe @('--config', $configPath, '--serve') 'as_server' $workDir
    $processes += Start-RoleProcess $tgsExe @('--config', $configPath, '--serve') 'tgs_server' $workDir
    $processes += Start-RoleProcess $vExe @('--config', $configPath, '--game-auth-encrypted') 'v_server' $workDir
    Start-Sleep -Milliseconds 900

    $beforeProcesses = Get-ProcessSnapshot $processes
    $beforeFiles = Get-FileSnapshot $workDir
    $loadOut = Join-Path $workDir 'game_load_client.out'
    $loadErr = Join-Path $workDir 'game_load_client.err'
    $timer = [System.Diagnostics.Stopwatch]::StartNew()
    $loadProcess = Start-Process `
        -FilePath $loadExe `
        -ArgumentList @($configPath, "$DurationSeconds", "$InputHz") `
        -WorkingDirectory $workDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput $loadOut `
        -RedirectStandardError $loadErr `
        -Wait `
        -PassThru
    $timer.Stop()
    $elapsedSeconds = [Math]::Max(0.001, $timer.Elapsed.TotalSeconds)
    $loadExitCode = $loadProcess.ExitCode

    $afterProcesses = Get-ProcessSnapshot $processes
    Stop-RoleProcesses $processes
    Start-Sleep -Milliseconds 200
    $afterFiles = Get-FileSnapshot $workDir
    $processDelta = Format-ProcessDelta $beforeProcesses $afterProcesses $elapsedSeconds
    $fileDelta = Format-FileDelta $beforeFiles $afterFiles $elapsedSeconds

    $reportPath = Join-Path $workDir 'perf_report.txt'
    $loadOutput = if (Test-Path -LiteralPath $loadOut) {
        Get-Content -LiteralPath $loadOut -Raw
    } else {
        ''
    }
    $loadError = if (Test-Path -LiteralPath $loadErr) {
        Get-Content -LiteralPath $loadErr -Raw
    } else {
        ''
    }
    if ($null -eq $loadOutput) {
        $loadOutput = ''
    }
    if ($null -eq $loadError) {
        $loadError = ''
    }

    $report = New-Object System.Collections.Generic.List[string]
    $report.Add("Four-client encrypted game performance run")
    $report.Add("timestamp=$stamp")
    $report.Add("work_dir=$workDir")
    $report.Add("duration_seconds=$DurationSeconds")
    $report.Add("input_hz=$InputHz")
    $report.Add("actual_elapsed_seconds=$([Math]::Round($elapsedSeconds, 3))")
    $report.Add("as_port=$basePort")
    $report.Add("tgs_port=$($basePort + 1)")
    $report.Add("v_port=$($basePort + 2)")
    $report.Add("")
    $report.Add("[load_client_stdout]")
    $report.Add($loadOutput.Trim())
    if ($loadError.Trim().Length -gt 0) {
        $report.Add("")
        $report.Add("[load_client_stderr]")
        $report.Add($loadError.Trim())
    }
    $report.Add("")
    $report.Add("[process_delta]")
    $report.Add(($processDelta | Format-Table -AutoSize | Out-String).Trim())
    $report.Add("")
    $report.Add("[log_growth]")
    $report.Add(($fileDelta | Select-Object -First 20 | Format-Table -AutoSize | Out-String).Trim())
    $reportText = ($report -join [Environment]::NewLine)
    $reportText | Set-Content -LiteralPath $reportPath -Encoding ASCII
    Write-Host $reportText
    Write-Host ""
    Write-Host "Report: $reportPath"

    if ($loadExitCode -ne 0) {
        throw "game_load_client failed with exit code $loadExitCode"
    }
}
finally {
    Stop-RoleProcesses $processes
}
