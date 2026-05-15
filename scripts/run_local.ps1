param(
    [string]$Config = 'config\course_config.txt',
    [uint16]$UiPort = 7001,
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

function Stop-RoleProcesses {
    $names = @('as_server', 'tgs_server', 'v_server', 'client')
    foreach ($name in $names) {
        $processes = Get-Process -Name $name -ErrorAction SilentlyContinue
        if ($processes) {
            $processes | Stop-Process -Force
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
$buildDir = Join-Path $repoRoot 'build-mingw'
$runtimeDir = Join-Path $repoRoot 'logs\runtime'
$configPath = Resolve-RepoPath $Config

Set-Location $repoRoot
New-Item -ItemType Directory -Force $runtimeDir | Out-Null

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

    & cmake --build $buildDir --target as_server tgs_server v_server client
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not $NoStop) {
    Stop-RoleProcesses
    Start-Sleep -Milliseconds 300
}

$processes = @()
$processes += Start-RoleProcess 'as_server.exe' @('--config', $configPath, '--serve') 'as_server'
$processes += Start-RoleProcess 'tgs_server.exe' @('--config', $configPath, '--serve') 'tgs_server'
$processes += Start-RoleProcess 'v_server.exe' @('--config', $configPath, '--game-auth-encrypted') 'v_server'
$processes += Start-RoleProcess 'client.exe' @('--config', $configPath, '--game-auth-encrypted', '--ui-port', "$UiPort") 'client'

Start-Sleep -Milliseconds 700

Write-Host 'Started local encrypted tank backend:'
foreach ($process in $processes) {
    Write-Host ("  {0} pid={1}" -f $process.ProcessName, $process.Id)
}
Write-Host ("Client bridge: ws://127.0.0.1:{0}" -f $UiPort)
Write-Host ("Open UI after starting web-ui: http://127.0.0.1:5173/?client=ws://127.0.0.1:{0}" -f $UiPort)
Write-Host 'Logs: logs\runtime\*.out and logs\runtime\*.err'
