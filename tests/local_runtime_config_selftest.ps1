param(
    [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$ErrorActionPreference = 'Stop'

function Require-Contains {
    param(
        [string]$Text,
        [string]$Needle
    )

    if (-not $Text.Contains($Needle)) {
        throw "Expected generated config to contain '$Needle'"
    }
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_local_runtime_config_" + [Guid]::NewGuid().ToString('N'))
$sourceConfig = Join-Path $tmp 'course_config.txt'
$outputConfig = Join-Path $tmp 'runtime\local_all_config.txt'
$scriptPath = Join-Path $RepoRoot 'scripts\new_local_runtime_config.ps1'

New-Item -ItemType Directory -Path $tmp -Force | Out-Null

@"
C1_ID=0x01
C2_ID=0x02
C3_ID=0x03
C4_ID=0x04
AS_ID=0x11
TGS_ID=0x12
V_ID=0x13
LOCAL_CLIENT_ID=0x04
AS_BIND_IP=0.0.0.0
AS_IP=172.27.219.167
AS_HOST=172.27.219.167
AS_PORT=9001
TGS_BIND_IP=0.0.0.0
TGS_IP=172.27.151.1
TGS_HOST=172.27.151.1
TGS_PORT=9002
V_BIND_IP=0.0.0.0
V_IP=172.27.219.167
V_HOST=172.27.219.167
V_PORT=9003
C4_PASSWORD=&wxh@147
KV=0x3398481d2a89f6
"@ | Set-Content -LiteralPath $sourceConfig -Encoding UTF8

try {
    powershell -NoProfile -ExecutionPolicy Bypass -File $scriptPath -SourceConfig $sourceConfig -OutputPath $outputConfig
    if ($LASTEXITCODE -ne 0) {
        throw "new_local_runtime_config.ps1 exited with code $LASTEXITCODE"
    }

    $bytes = [System.IO.File]::ReadAllBytes($outputConfig)
    if ($bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF) {
        throw 'Expected generated config to use UTF-8 without BOM'
    }

    $generated = Get-Content -LiteralPath $outputConfig -Raw -Encoding UTF8
    Require-Contains $generated 'LOCAL_CLIENT_ID=0x04'
    Require-Contains $generated 'AS_BIND_IP=127.0.0.1'
    Require-Contains $generated 'AS_IP=127.0.0.1'
    Require-Contains $generated 'AS_HOST=127.0.0.1'
    Require-Contains $generated 'TGS_BIND_IP=127.0.0.1'
    Require-Contains $generated 'TGS_IP=127.0.0.1'
    Require-Contains $generated 'TGS_HOST=127.0.0.1'
    Require-Contains $generated 'V_BIND_IP=127.0.0.1'
    Require-Contains $generated 'V_IP=127.0.0.1'
    Require-Contains $generated 'V_HOST=127.0.0.1'
    Require-Contains $generated 'AS_PORT=9001'
    Require-Contains $generated 'TGS_PORT=9002'
    Require-Contains $generated 'V_PORT=9003'
    Require-Contains $generated 'C4_PASSWORD=&wxh@147'
    Require-Contains $generated 'KV=0x3398481d2a89f6'
    Require-Contains $generated 'LOG_ROOT=_generated/logs'

    Write-Host 'local_runtime_config_selftest: ok'
}
finally {
    if (Test-Path -LiteralPath $tmp) {
        Remove-Item -LiteralPath $tmp -Recurse -Force
    }
}
