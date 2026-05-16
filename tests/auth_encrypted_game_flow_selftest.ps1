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

function Receive-WebSocketJson {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [int]$TimeoutMs
    )
    $buffer = New-Object byte[] 8192
    $segment = [ArraySegment[byte]]::new($buffer)
    $task = $Socket.ReceiveAsync($segment, [Threading.CancellationToken]::None)
    if (-not $task.Wait($TimeoutMs)) {
        throw "Timed out waiting for websocket message"
    }
    $count = $task.Result.Count
    return [Text.Encoding]::UTF8.GetString($buffer, 0, $count) | ConvertFrom-Json
}

function Send-WebSocketText {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [string]$Text
    )
    $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
    $segment = [ArraySegment[byte]]::new($bytes)
    $Socket.SendAsync($segment, [System.Net.WebSockets.WebSocketMessageType]::Text,
        $true, [Threading.CancellationToken]::None).Wait(5000) | Out-Null
}

function Assert-NoWebSocketMessage {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [int]$TimeoutMs
    )
    $buffer = New-Object byte[] 1024
    $segment = [ArraySegment[byte]]::new($buffer)
    $task = $Socket.ReceiveAsync($segment, [Threading.CancellationToken]::None)
    if ($task.Wait($TimeoutMs)) {
        throw "Received websocket message before join"
    }
}

function Wait-ForMessage {
    param(
        [System.Net.WebSockets.ClientWebSocket]$Socket,
        [scriptblock]$Predicate,
        [string]$Description
    )
    for ($i = 0; $i -lt 10; $i++) {
        $message = Receive-WebSocketJson $Socket 5000
        if (& $Predicate $message) {
            return $message
        }
    }
    throw "Timed out waiting for $Description"
}

function Assert-Contains {
    param(
        [string]$Path,
        [string]$Needle
    )
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing expected log file $Path"
    }
    $text = Get-Content -LiteralPath $Path -Raw
    if (-not $text.Contains($Needle)) {
        throw "Expected $Path to contain '$Needle'"
    }
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("cyber_auth_encrypted_game_" + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null

$basePort = Get-Random -Minimum 21000 -Maximum 43000
$asPort = $basePort
$tgsPort = $basePort + 1
$vPort = $basePort + 2
$uiPort = $basePort + 3
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
$socket = $null
try {
    $processes += Start-RoleProcess 'as_server.exe' @('--config', $config, '--serve', '--max-connections', '3') 'as_server' $tmp
    $processes += Start-RoleProcess 'tgs_server.exe' @('--config', $config, '--serve', '--max-connections', '2') 'tgs_server' $tmp
    $processes += Start-RoleProcess 'v_server.exe' @('--config', $config, '--game-auth-encrypted') 'v_server' $tmp
    $processes += Start-RoleProcess 'client.exe' @('--config', $config, '--game-auth-encrypted', '--ui-port', "$uiPort") 'client' $tmp

    Start-Sleep -Milliseconds 900

    $socket = [System.Net.WebSockets.ClientWebSocket]::new()
    $socket.ConnectAsync([Uri]"ws://127.0.0.1:$uiPort", [Threading.CancellationToken]::None).Wait(5000)

    Send-WebSocketText $socket '{"type":"login","clientId":1,"password":"wrong"}'
    $failed = Wait-ForMessage $socket { param($m) $m.type -eq 'loginState' -and $m.status -eq 'failed' } 'failed loginState'
    if ($failed.type -ne 'loginState' -or $failed.status -ne 'failed') {
        throw "Expected failed loginState for wrong password"
    }

    Send-WebSocketText $socket '{"type":"login","clientId":1,"password":"123456"}'
    $authenticated = Wait-ForMessage $socket { param($m) $m.type -eq 'loginState' -and $m.status -eq 'authenticated' } 'authenticated loginState'
    if ($authenticated.clientId -ne 1) {
        throw "Expected authenticated loginState for Client1"
    }

    Send-WebSocketText $socket '{"type":"move","x":1,"y":0}'
    Assert-NoWebSocketMessage $socket 300
    $socket.Dispose()
    $socket = [System.Net.WebSockets.ClientWebSocket]::new()
    $socket.ConnectAsync([Uri]"ws://127.0.0.1:$uiPort", [Threading.CancellationToken]::None).Wait(5000) | Out-Null

    Send-WebSocketText $socket '{"type":"join"}'
    $joined = Wait-ForMessage $socket { param($m) $m.type -eq 'joinState' -and $m.status -eq 'joined' } 'joined state'
    if ($joined.type -ne 'joinState' -or $joined.status -ne 'joined') {
        throw "Expected joined state"
    }

    $state = Wait-ForMessage $socket { param($m) $m.type -eq 'state' -and $m.state.tanks.Count -gt 0 } 'game state after join'
    if ($state.type -ne 'state') {
        throw "Expected game state after join"
    }

    Start-Sleep -Milliseconds 250
    Assert-Contains (Join-Path $tmp 'logs\client_ack.log') 'APP_NON_REPUDIATION_ACK'
    Assert-Contains (Join-Path $tmp 'logs\client_ack.log') 'packet_hex='
    Assert-Contains (Join-Path $tmp 'logs\v_ack.log') 'APP_NON_REPUDIATION_ACK'
    Assert-Contains (Join-Path $tmp 'logs\v_ack.log') 'packet_hex='

    & (Join-Path $BuildDir 'encrypted_plaintext_rejection_client.exe') $config
    if ($LASTEXITCODE -ne 0) {
        throw "encrypted_plaintext_rejection_client failed"
    }

    Write-Host 'auth_encrypted_game_flow_selftest: ok'
}
finally {
    if ($socket) {
        $socket.Dispose()
    }
    foreach ($process in $processes) {
        if ($process -and -not $process.HasExited) {
            Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
        }
    }
    if (Test-Path -LiteralPath $tmp) {
        Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    }
}
