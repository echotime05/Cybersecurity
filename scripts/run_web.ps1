param(
    [string]$BindHost = '127.0.0.1',
    [uint16]$Port = 5173,
    [switch]$NoInstall
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..')
$webRoot = Join-Path $repoRoot 'web-ui'

if (-not (Get-Command npm.cmd -ErrorAction SilentlyContinue) -and
    -not (Get-Command npm -ErrorAction SilentlyContinue)) {
    throw 'npm was not found on PATH'
}

Set-Location $webRoot

if (-not $NoInstall -and -not (Test-Path -LiteralPath (Join-Path $webRoot 'node_modules'))) {
    npm ci
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

npm run dev -- --host $BindHost --port $Port
exit $LASTEXITCODE
