param(
    [string]$LogRoot = '_generated/logs',
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
        return 0
    }

    $removed = 0
    foreach ($pattern in $Patterns) {
        Get-ChildItem -LiteralPath $resolvedPath.Path -Filter $pattern -File |
            ForEach-Object {
                Remove-Item -LiteralPath $_.FullName -Force
                ++$removed
            }
    }
    return $removed
}

function Clear-LogDirectories {
    param(
        [string]$Path,
        [string[]]$Names
    )

    $resolvedPath = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
    if (-not $resolvedPath) {
        return 0
    }

    $removed = 0
    foreach ($name in $Names) {
        $target = Join-Path $resolvedPath.Path $name
        $resolvedTarget = Resolve-Path -LiteralPath $target -ErrorAction SilentlyContinue
        if ($resolvedTarget -and $resolvedTarget.Path.StartsWith($resolvedPath.Path, [System.StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $resolvedTarget.Path -Recurse -Force
            ++$removed
        }
    }
    return $removed
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..')
$resolvedLogRoot = Resolve-RepoPath $LogRoot
$runtimeDir = Join-Path $resolvedLogRoot 'runtime'
$protocolEventsDir = Join-Path $resolvedLogRoot 'protocol_events'

if (-not $NoStop) {
    Stop-RoleProcesses
    Start-Sleep -Milliseconds 300
}

New-Item -ItemType Directory -Force $resolvedLogRoot | Out-Null
New-Item -ItemType Directory -Force $runtimeDir | Out-Null
New-Item -ItemType Directory -Force $protocolEventsDir | Out-Null

$removedFiles = 0
$removedDirs = 0

$removedFiles += Clear-LogFiles $resolvedLogRoot @(
    '*.log',
    'all_services_config.txt',
    'report_revision_text.txt'
)
$removedDirs += Clear-LogDirectories $resolvedLogRoot @('run_plain_game')
$removedFiles += Clear-LogFiles $runtimeDir @('*.out', '*.err')
$removedFiles += Clear-LogFiles $protocolEventsDir @('*.txt')

Write-Host ("Cleared logs under {0}" -f $resolvedLogRoot)
Write-Host ("Removed files: {0}" -f $removedFiles)
Write-Host ("Removed directories: {0}" -f $removedDirs)
