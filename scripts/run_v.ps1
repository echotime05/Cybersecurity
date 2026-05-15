param(
    [string]$Config = 'config\course_config.txt',
    [switch]$NoBuild,
    [switch]$NoStop,
    [switch]$PrintOnly
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
$serverExe = Join-Path $buildDir 'v_server.exe'
$configPath = Resolve-RepoPath $Config

Set-Location $repoRoot

if (-not (Test-Path -LiteralPath $configPath)) {
    throw "Missing config file: $configPath"
}

if (-not $NoStop) {
    $oldServers = Get-Process -Name v_server -ErrorAction SilentlyContinue
    if ($oldServers) {
        Write-Host '[run-v] stopping old v_server process'
        $oldServers | Stop-Process -Force
        Start-Sleep -Milliseconds 300
    }
}

if (-not $NoBuild) {
    Require-Command 'cmake.exe' | Out-Null
    if (-not (Test-Path -LiteralPath $buildDir)) {
        Require-Command 'ninja.exe' | Out-Null
        Write-Host '[run-v] configuring build-mingw with Ninja'
        & cmake -S $repoRoot -B $buildDir -G Ninja
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    Write-Host '[run-v] building v_server'
    & cmake --build $buildDir --target v_server
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if (-not (Test-Path -LiteralPath $serverExe)) {
    throw "Missing executable: $serverExe"
}

Write-Host '[run-v] current V config'
& $serverExe --print-config --config $configPath
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

if ($PrintOnly) {
    exit 0
}

Write-Host '[run-v] starting encrypted V in foreground. Press Ctrl+C to stop.'
& $serverExe --config $configPath --game-auth-encrypted
exit $LASTEXITCODE
