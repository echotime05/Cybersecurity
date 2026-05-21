param(
    [string]$SourceConfig = 'config\course_config.txt',
    [string]$OutputPath = '_generated\logs\runtime\local_all_config.txt'
)

$ErrorActionPreference = 'Stop'

function Resolve-RepoPath {
    param([string]$Path)

    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $repoRoot $Path
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir '..')
$sourcePath = Resolve-RepoPath $SourceConfig
$resolvedOutputPath = Resolve-RepoPath $OutputPath

if (-not (Test-Path -LiteralPath $sourcePath)) {
    throw "Missing source config: $sourcePath"
}

$replacements = [ordered]@{
    'LOG_ROOT'    = '_generated/logs'
    'AS_BIND_IP'  = '127.0.0.1'
    'AS_IP'       = '127.0.0.1'
    'AS_HOST'     = '127.0.0.1'
    'TGS_BIND_IP' = '127.0.0.1'
    'TGS_IP'      = '127.0.0.1'
    'TGS_HOST'    = '127.0.0.1'
    'V_BIND_IP'   = '127.0.0.1'
    'V_IP'        = '127.0.0.1'
    'V_HOST'      = '127.0.0.1'
}

$seenKeys = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
$sourceLines = Get-Content -LiteralPath $sourcePath -Encoding UTF8
$outputLines = foreach ($line in $sourceLines) {
    if ($line -match '^([^#=]+)=(.*)$') {
        $key = $Matches[1].Trim()
        if ($replacements.Contains($key)) {
            $null = $seenKeys.Add($key)
            "$key=$($replacements[$key])"
            continue
        }
    }
    $line
}

foreach ($key in $replacements.Keys) {
    if (-not $seenKeys.Contains($key)) {
        $outputLines += "$key=$($replacements[$key])"
    }
}

$outputDir = Split-Path -Parent $resolvedOutputPath
if (-not [string]::IsNullOrWhiteSpace($outputDir)) {
    New-Item -ItemType Directory -Force $outputDir | Out-Null
}

[System.IO.File]::WriteAllLines(
    $resolvedOutputPath,
    $outputLines,
    [System.Text.UTF8Encoding]::new($false)
)
Write-Host ("Wrote local runtime config: {0}" -f $resolvedOutputPath)
