param(
    [uint16]$UiPort = 7010,
    [string]$EventsDir = '_generated\logs\protocol_events',
    [switch]$NoBuild,
    [switch]$NoStop
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $repoRoot $Path
}

function Require-Command {
    param([string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "$Name was not found on PATH"
    }
    return $command.Source
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..')
$buildDir = Join-Path $repoRoot 'build-mingw'
$monitorExeBase = 'monitor.exe'
$eventsDirPath = Resolve-RepoPath $EventsDir

function Resolve-MonitorExe {
    $candidate = Join-Path $buildDir $monitorExeBase
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }
    foreach ($configName in @('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')) {
        $candidate = Join-Path $buildDir (Join-Path $configName $monitorExeBase)
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return (Join-Path $buildDir $monitorExeBase)
}

Set-Location $repoRoot

New-Item -ItemType Directory -Force $eventsDirPath | Out-Null

if (-not $NoStop) {
    $oldMonitors = Get-Process -Name monitor -ErrorAction SilentlyContinue
    if ($oldMonitors) {
        Write-Host '[run-monitor] stopping old monitor process'
        $oldMonitors | Stop-Process -Force
        Start-Sleep -Milliseconds 300
    }
}

if (-not $NoBuild) {
    Require-Command 'cmake.exe' | Out-Null
    if (-not (Test-Path -LiteralPath $buildDir)) {
        Require-Command 'ninja.exe' | Out-Null
        Write-Host '[run-monitor] configuring build-mingw with Ninja'
        & cmake -S $repoRoot -B $buildDir -G Ninja
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Write-Host '[run-monitor] building monitor'
    & cmake --build $buildDir --config Debug --target monitor
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$monitorExe = Resolve-MonitorExe
if (-not (Test-Path -LiteralPath $monitorExe)) {
    throw "Missing executable: $monitorExe"
}

Write-Host ("[run-monitor] events dir: {0}" -f $eventsDirPath)
Write-Host ("[run-monitor] starting protocol monitor on ws://127.0.0.1:{0}" -f $UiPort)
& $monitorExe --ui-port "$UiPort" --events-dir $eventsDirPath
exit $LASTEXITCODE
