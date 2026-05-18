param(
    [string]$Config = 'config\course_config.txt',
    [uint16]$UiPort = 7001,
    [uint16]$MonitorPort = 7010,
    [switch]$NoBuild,
    [switch]$NoStop,
    [switch]$KeepLogs
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

function Add-ToolPathIfPresent {
    param([string]$Path)

    if ((Test-Path -LiteralPath $Path) -and ($env:PATH -notlike "*$Path*")) {
        $env:PATH = "$Path;$env:PATH"
    }
}

function Stop-RoleProcesses {
    $names = @('as_server', 'tgs_server', 'v_server', 'client', 'monitor')
    foreach ($name in $names) {
        $processes = Get-Process -Name $name -ErrorAction SilentlyContinue
        if ($processes) {
            $processes | Stop-Process -Force
        }
    }
}

function Clear-LogFiles {
    param(
        [string]$Path,
        [string[]]$Patterns
    )

    $resolvedPath = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $resolvedPath) {
        return
    }
    foreach ($pattern in $Patterns) {
        Get-ChildItem -LiteralPath $resolvedPath.Path -Filter $pattern -File |
            ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
            }
    }
}

function Clear-LogDirectories {
    param(
        [string]$Path,
        [string[]]$Names
    )

    $resolvedPath = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $resolvedPath) {
        return
    }
    foreach ($name in $Names) {
        $target = Join-Path $resolvedPath.Path $name
        $resolvedTarget = Resolve-Path -LiteralPath $target -ErrorAction SilentlyContinue
        if ($resolvedTarget -and $resolvedTarget.Path.StartsWith($resolvedPath.Path, [System.StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $resolvedTarget.Path -Recurse -Force
        }
    }
}

function Start-RoleProcess {
    param(
        [string]$ExeName,
        [string[]]$Arguments,
        [string]$LogName
    )

    $exe = Join-Path $buildDir $ExeName
    if (-not (Test-Path -LiteralPath $exe)) {
        throw "Missing executable: $exe"
    }

    Start-Process `
        -FilePath $exe `
        -ArgumentList $Arguments `
        -WorkingDirectory $repoRoot `
        -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $runtimeDir "$LogName.out") `
        -RedirectStandardError (Join-Path $runtimeDir "$LogName.err") `
        -PassThru
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..')
$generatedRoot = Join-Path $repoRoot '_generated'
$buildDir = Join-Path $generatedRoot 'build-mingw'
$logRoot = Join-Path $generatedRoot 'logs'
$runtimeDir = Join-Path $logRoot 'runtime'
$protocolEventsDir = Join-Path $logRoot 'protocol_events'
$configPath = Resolve-RepoPath $Config

Set-Location $repoRoot
Add-ToolPathIfPresent 'E:\Qt\Tools\CMake_64\bin'
Add-ToolPathIfPresent 'E:\Qt\Tools\Ninja'
Add-ToolPathIfPresent 'E:\Qt\Tools\mingw1120_64\bin'
New-Item -ItemType Directory -Force $generatedRoot | Out-Null
New-Item -ItemType Directory -Force $logRoot | Out-Null
New-Item -ItemType Directory -Force $runtimeDir | Out-Null
New-Item -ItemType Directory -Force $protocolEventsDir | Out-Null

if (-not (Test-Path -LiteralPath $configPath)) {
    throw "Missing config file: $configPath"
}

if (-not $NoBuild) {
    Require-Command 'cmake.exe' | Out-Null
    if (-not (Test-Path -LiteralPath $buildDir)) {
        Require-Command 'ninja.exe' | Out-Null
        & cmake -S $repoRoot -B $buildDir -G Ninja
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    & cmake --build $buildDir --target as_server tgs_server v_server client monitor
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not $NoStop) {
    Stop-RoleProcesses
    Start-Sleep -Milliseconds 300
}

if (-not $NoStop -and -not $KeepLogs) {
    Clear-LogFiles $logRoot @(
        'as.log',
        'tgs.log',
        'v_game.log',
        'client_game.log',
        'v_ack.log',
        'client_ack.log'
    )
    Clear-LogFiles $logRoot @(
        'v.log',
        'client_0*.log',
        '*_plain_game.log',
        '*_server.out',
        '*_server.err',
        'all_services_config.txt',
        'report_revision_text.txt'
    )
    Clear-LogDirectories $logRoot @('run_plain_game')
    Clear-LogFiles $runtimeDir @('*.out', '*.err')
    Clear-LogFiles $protocolEventsDir @('*.txt')
}

$processes = @()
$processes += Start-RoleProcess 'as_server.exe' @('--config', $configPath, '--serve') 'as_server'
$processes += Start-RoleProcess 'tgs_server.exe' @('--config', $configPath, '--serve') 'tgs_server'
$processes += Start-RoleProcess 'v_server.exe' @('--config', $configPath, '--game-auth-encrypted') 'v_server'
$processes += Start-RoleProcess 'client.exe' @('--config', $configPath, '--game-auth-encrypted', '--ui-port', "$UiPort") 'client'
$processes += Start-RoleProcess 'monitor.exe' @('--ui-port', "$MonitorPort", '--events-dir', $protocolEventsDir) 'monitor'

Start-Sleep -Milliseconds 700

Write-Host 'Started local encrypted tank backend:'
foreach ($process in $processes) {
    Write-Host ("  {0} pid={1}" -f $process.ProcessName, $process.Id)
}
Write-Host ("Client bridge: ws://127.0.0.1:{0}" -f $UiPort)
Write-Host ("Protocol monitor: ws://127.0.0.1:{0}" -f $MonitorPort)
Write-Host ("Open UI after starting web-ui: http://127.0.0.1:5173/?client=ws://127.0.0.1:{0}&monitor=ws://127.0.0.1:{1}" -f $UiPort, $MonitorPort)
Write-Host 'Logs: _generated\logs\runtime\*.out and _generated\logs\runtime\*.err'
