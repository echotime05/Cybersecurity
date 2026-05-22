param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

function Wait-ForMonitorPort {
    param(
        [int]$Port,
        [int]$TimeoutMs = 5000
    )

    $deadline = [DateTime]::UtcNow.AddMilliseconds($TimeoutMs)
    while ([DateTime]::UtcNow -lt $deadline) {
        $listener = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
        if ($listener) {
            return
        }
        Start-Sleep -Milliseconds 100
    }

    throw "Timed out waiting for monitor to listen on port $Port"
}

$port = Get-Random -Minimum 21000 -Maximum 43000
$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_run_monitor_" + [Guid]::NewGuid().ToString('N'))
$eventsDir = Join-Path $tmp 'protocol_events'
$scriptPath = Join-Path $RepoRoot 'scripts\run_monitor.ps1'

New-Item -ItemType Directory -Path $eventsDir -Force | Out-Null

$process = $null
try {
    $process = Start-Process powershell `
        -ArgumentList @(
            '-NoProfile',
            '-ExecutionPolicy', 'Bypass',
            '-File', $scriptPath,
            '-NoBuild',
            '-NoStop',
            '-UiPort', "$port",
            '-EventsDir', $eventsDir
        ) `
        -WorkingDirectory $RepoRoot `
        -WindowStyle Hidden `
        -PassThru

    Wait-ForMonitorPort -Port $port

    if ($process.HasExited) {
        throw "run_monitor.ps1 exited early with code $($process.ExitCode)"
    }

    Write-Host 'run_monitor_selftest: ok'
}
finally {
    if ($process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path -LiteralPath $tmp) {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
}
