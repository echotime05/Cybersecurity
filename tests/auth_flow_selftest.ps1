param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir
)

$ErrorActionPreference = 'Stop'

function Start-RoleProcess {
    param(
        [string]$ExeName,
        [string[]]$Arguments,
        [string]$Name,
        [string]$WorkDir
    )

    Start-Process `
        -FilePath (Join-Path $BuildDir $ExeName) `
        -ArgumentList $Arguments `
        -WorkingDirectory $WorkDir `
        -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $WorkDir "$Name.out") `
        -RedirectStandardError (Join-Path $WorkDir "$Name.err") `
        -PassThru
}

function Assert-Contains {
    param(
        [string]$Path,
        [string]$Pattern
    )

    if (-not (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) {
        throw "Expected '$Pattern' in $Path"
    }
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_auth_flow_" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null

$basePort = Get-Random -Minimum 21000 -Maximum 45000
$asPort = $basePort
$tgsPort = $basePort + 1
$vPort = $basePort + 2
$config = Join-Path $tmp 'course_config.txt'

@"
C1_ID=0x01
C2_ID=0x02
C3_ID=0x03
C4_ID=0x04
AS_ID=0x11
TGS_ID=0x12
V_ID=0x13
LOCAL_CLIENT_ID=0x01
AS_BIND_IP=127.0.0.1
AS_IP=127.0.0.1
AS_HOST=127.0.0.1
AS_PORT=$asPort
TGS_BIND_IP=127.0.0.1
TGS_IP=127.0.0.1
TGS_HOST=127.0.0.1
TGS_PORT=$tgsPort
V_BIND_IP=127.0.0.1
V_IP=127.0.0.1
V_HOST=127.0.0.1
V_PORT=$vPort
C1_PASSWORD=123456
C1_KC=0x59ef3db7cb8c8d
C2_PASSWORD=admin123
C2_KC=0x6a73a4ebe9c564
C3_PASSWORD=hehe12345
C3_KC=0x57ef9d5f45ab7b
C4_PASSWORD=&wxh@147
C4_KC=0xec3eb766d59086
KTGS=0x1c24deeecc136e
KV=0x3398481d2a89f6
PK_CA_N=0xACE9A881930A29215BA7306E49654BB851F86EC32FE4A8D2FF516D4FB937E8A3
PK_CA_E=0x10001
SK_CA_D=0xA1A84610F63E7E9BA04B9BBCD043B2D891C75316A7AC70BEC7C3CEB1477AFB69
"@ | Set-Content -LiteralPath $config -Encoding ASCII

$processes = @()
try {
    $processes += Start-RoleProcess 'as_server.exe' @('--config', $config, '--serve', '--max-connections', '1') 'as_server' $tmp
    $processes += Start-RoleProcess 'tgs_server.exe' @('--config', $config, '--serve', '--max-connections', '1') 'tgs_server' $tmp
    $processes += Start-RoleProcess 'v_server.exe' @('--config', $config, '--serve', '--max-connections', '3') 'v_server' $tmp

    Start-Sleep -Milliseconds 700

    Push-Location $tmp
    try {
        & (Join-Path $BuildDir 'client.exe') --config $config --auth-test
        if ($LASTEXITCODE -ne 0) {
            throw "client --auth-test failed with exit code $LASTEXITCODE"
        }
    }
    finally {
        Pop-Location
    }

    foreach ($process in $processes) {
        if (-not $process.WaitForExit(10000)) {
            throw "Process $($process.Id) did not exit"
        }
        $process.Refresh()
        if ($null -ne $process.ExitCode -and $process.ExitCode -ne 0) {
            throw "Process $($process.Id) exited with code $($process.ExitCode)"
        }
    }

    Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] AS_OK'
    Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] TGS_OK'
    Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] V_AUTH_OK'
    Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'AUTH_STATE] AUTH_DONE'
    Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'APP_NON_REPUDIATION] GAME_JOIN_REQ_SIGNED'
    Assert-Contains (Join-Path $tmp 'logs\client_01.log') 'APP_NON_REPUDIATION] APP_ACK_VERIFIED'
    Assert-Contains (Join-Path $tmp 'logs\v.log') 'APP_NON_REPUDIATION] GAME_JOIN_REQ_VERIFIED'
    Assert-Contains (Join-Path $tmp 'logs\v.log') 'APP_NON_REPUDIATION] APP_ACK_SIGNED'

    Write-Host 'auth_flow_selftest: ok'
}
finally {
    foreach ($process in $processes) {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $tmp) {
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}
