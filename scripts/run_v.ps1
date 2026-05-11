param(
    [switch]$NoBuild,
    [switch]$NoStop,
    [switch]$PrintOnly
)

$ErrorActionPreference = 'Stop'

function Add-PathIfExists {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return
    }
    if (Test-Path -LiteralPath $Path) {
        $env:PATH = "$Path;$env:PATH"
    }
}

function Resolve-Tool {
    param(
        [string]$Name,
        [string[]]$Candidates
    )

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    foreach ($candidate in $Candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    return $null
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..')
$buildDir = Join-Path $repoRoot 'build-mingw'
$serverExe = Join-Path $buildDir 'v_server.exe'
$configTemplate = Join-Path $repoRoot 'config\lan\host4_v_client4.txt'
$configPath = Join-Path $repoRoot 'config\course_config.txt'

Set-Location $repoRoot

$toolDirs = @(
    'E:\Qt\Tools\CMake_64\bin',
    'E:\Qt\Tools\Ninja',
    'E:\Qt\Tools\mingw1120_64\bin',
    'C:\Qt\Tools\CMake_64\bin',
    'C:\Qt\Tools\Ninja',
    'C:\Qt\Tools\mingw1120_64\bin'
)

foreach ($dir in $toolDirs) {
    Add-PathIfExists $dir
}

if (-not (Test-Path -LiteralPath $configTemplate)) {
    throw "Missing Host4 config template: $configTemplate"
}

Write-Host '[run-v] repo root:' $repoRoot
Write-Host '[run-v] applying Host4 config: config\lan\host4_v_client4.txt -> config\course_config.txt'
Copy-Item -LiteralPath $configTemplate -Destination $configPath -Force

if (-not $NoStop) {
    $oldServers = Get-Process -Name v_server -ErrorAction SilentlyContinue
    if ($oldServers) {
        Write-Host '[run-v] stopping old v_server process'
        $oldServers | Stop-Process -Force
        Start-Sleep -Milliseconds 300
    }
}

if (-not $NoBuild) {
    $cmake = Resolve-Tool 'cmake.exe' @(
        'E:\Qt\Tools\CMake_64\bin\cmake.exe',
        'C:\Qt\Tools\CMake_64\bin\cmake.exe'
    )

    if ($cmake) {
        if (-not (Test-Path -LiteralPath $buildDir)) {
            $ninja = Resolve-Tool 'ninja.exe' @(
                'E:\Qt\Tools\Ninja\ninja.exe',
                'C:\Qt\Tools\Ninja\ninja.exe'
            )

            if ($ninja) {
                Write-Host '[run-v] configuring build-mingw with Ninja'
                & $cmake -S $repoRoot -B $buildDir -G Ninja
            }
            else {
                Write-Host '[run-v] configuring build-mingw with default CMake generator'
                & $cmake -S $repoRoot -B $buildDir
            }

            if ($LASTEXITCODE -ne 0) {
                exit $LASTEXITCODE
            }
        }

        Write-Host '[run-v] building v_server'
        & $cmake --build $buildDir --target v_server
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
    elseif (-not (Test-Path -LiteralPath $serverExe)) {
        throw 'cmake.exe was not found and build-mingw\v_server.exe does not exist.'
    }
    else {
        Write-Host '[run-v] cmake.exe not found; using existing build-mingw\v_server.exe'
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

Write-Host '[run-v] starting V in foreground. Press Ctrl+C to stop.'
& $serverExe --serve --config $configPath
exit $LASTEXITCODE
