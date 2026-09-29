param(
    [string]$RuntimeDirectory = (Join-Path $PSScriptRoot '../_Build/windows/install'),
    [switch]$PrintSettings
)
$ErrorActionPreference = 'Stop'
$settings = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'u59-preset.json') -Raw | ConvertFrom-Json
if ($PrintSettings) { $settings | ConvertTo-Json; return }
$runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$launcher = Join-Path $runtime 'launcher.exe'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw "Launcher not found: $launcher" }
$start = New-Object System.Diagnostics.ProcessStartInfo
$start.FileName = $launcher
$start.WorkingDirectory = $runtime
$start.UseShellExecute = $false
# Isolate the child from inherited experimental settings; leave the parent intact.
foreach ($key in @($start.EnvironmentVariables.Keys)) {
    if ($key -like 'KYTY_*' -or $key -like 'TRACY_*') { $start.EnvironmentVariables.Remove($key) }
}
foreach ($setting in $settings.PSObject.Properties) {
    $start.EnvironmentVariables[$setting.Name] = [string]$setting.Value
}
$process = [System.Diagnostics.Process]::Start($start)
Write-Output "Started launcher PID $($process.Id). Configure tracing and validation in the launcher before starting a game."
