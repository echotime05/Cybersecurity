$ErrorActionPreference = 'Stop'

$names = @('as_server', 'tgs_server', 'v_server', 'client')
$stopped = 0

foreach ($name in $names) {
    $processes = Get-Process -Name $name -ErrorAction SilentlyContinue
    foreach ($process in $processes) {
        Stop-Process -Id $process.Id -Force
        Write-Host ("Stopped {0} pid={1}" -f $process.ProcessName, $process.Id)
        ++$stopped
    }
}

if ($stopped -eq 0) {
    Write-Host 'No local C++ role processes were running.'
}
