param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

function Require-PathMissing {
    param([string]$Path)

    if (Test-Path -LiteralPath $Path) {
        throw "Expected path to be removed: $Path"
    }
}

function Require-PathExists {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Expected path to exist: $Path"
    }
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_clear_logs_" + [Guid]::NewGuid().ToString('N'))
$logRoot = Join-Path $tmp '_generated\logs'
$runtimeDir = Join-Path $logRoot 'runtime'
$protocolEventsDir = Join-Path $logRoot 'protocol_events'
$plainGameDir = Join-Path $logRoot 'run_plain_game'
$outsideFile = Join-Path $tmp 'keep.txt'
$insideUntouched = Join-Path $logRoot 'keep.bin'

New-Item -ItemType Directory -Path $runtimeDir -Force | Out-Null
New-Item -ItemType Directory -Path $protocolEventsDir -Force | Out-Null
New-Item -ItemType Directory -Path $plainGameDir -Force | Out-Null

Set-Content -LiteralPath (Join-Path $logRoot 'as.log') -Value 'log' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $logRoot 'all_services_config.txt') -Value 'config' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $logRoot 'report_revision_text.txt') -Value 'report' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $runtimeDir 'client.out') -Value 'stdout' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $runtimeDir 'client.err') -Value 'stderr' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $protocolEventsDir 'client_01.txt') -Value 'event' -Encoding ASCII
Set-Content -LiteralPath (Join-Path $plainGameDir 'state.txt') -Value 'state' -Encoding ASCII
Set-Content -LiteralPath $outsideFile -Value 'outside' -Encoding ASCII
Set-Content -LiteralPath $insideUntouched -Value 'keep' -Encoding ASCII

$scriptPath = Join-Path $RepoRoot 'scripts\clear_logs.ps1'

try {
    powershell -NoProfile -ExecutionPolicy Bypass -File $scriptPath -LogRoot $logRoot
    if ($LASTEXITCODE -ne 0) {
        throw "clear_logs.ps1 exited with code $LASTEXITCODE"
    }

    Require-PathMissing (Join-Path $logRoot 'as.log')
    Require-PathMissing (Join-Path $logRoot 'all_services_config.txt')
    Require-PathMissing (Join-Path $logRoot 'report_revision_text.txt')
    Require-PathMissing (Join-Path $runtimeDir 'client.out')
    Require-PathMissing (Join-Path $runtimeDir 'client.err')
    Require-PathMissing (Join-Path $protocolEventsDir 'client_01.txt')
    Require-PathMissing $plainGameDir

    Require-PathExists $logRoot
    Require-PathExists $runtimeDir
    Require-PathExists $protocolEventsDir
    Require-PathExists $outsideFile
    Require-PathExists $insideUntouched

    Write-Host 'clear_logs_selftest: ok'
}
finally {
    if (Test-Path -LiteralPath $tmp) {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
}
