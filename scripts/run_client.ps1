param(
    [switch]$NoBuild,
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$RemainingArgs
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
$clientExeBase = 'client.exe'

function Resolve-ClientExe {
    $candidate = Join-Path $buildDir $clientExeBase
    if (Test-Path -LiteralPath $candidate) {
        return $candidate
    }
    $multiConfigPaths = @('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')
    foreach ($config in $multiConfigPaths) {
        $candidate = Join-Path $buildDir (Join-Path $config $clientExeBase)
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

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

if (-not $NoBuild) {
    $cmake = Resolve-Tool 'cmake.exe' @(
        'E:\Qt\Tools\CMake_64\bin\cmake.exe',
        'C:\Qt\Tools\CMake_64\bin\cmake.exe'
    )

    if ($cmake) {
        if (-not (Test-Path -LiteralPath $buildDir)) {
            Write-Host '[run-client] configuring build directory'
            & $cmake -S $repoRoot -B $buildDir
        }

        Write-Host '[run-client] building client'
        & $cmake --build $buildDir --target client
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}

$clientExe = Resolve-ClientExe
if (-not $clientExe) {
    throw "Missing executable: $clientExeBase not found in $buildDir or its subdirectories."
}

Write-Host "[run-client] starting client: $clientExe"
& $clientExe $RemainingArgs
exit $LASTEXITCODE
