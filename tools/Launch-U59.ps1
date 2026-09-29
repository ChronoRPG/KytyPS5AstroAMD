param(
    [string]$RuntimeDirectory,
    [switch]$PrintSettings
)
$ErrorActionPreference = 'Stop'
$settings = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'u59-preset.json') -Raw | ConvertFrom-Json

if ($PrintSettings) { $settings | ConvertTo-Json; return }
if (-not $RuntimeDirectory) {
    # The download places this script beside launcher.exe; the source checkout keeps it in tools/.
    $RuntimeDirectory = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'launcher.exe')) {
        $PSScriptRoot
    } else {
        Join-Path $PSScriptRoot '../_Build/windows/install'
    }
}
$runtime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
$launcher = Join-Path $runtime 'launcher.exe'
if (-not (Test-Path -LiteralPath $launcher -PathType Leaf)) { throw "Launcher not found: $launcher" }

# The first U59 ZIP bundled a local Kyty.ini, which takes priority over QSettings' shared
# C:/ProgramData/Kyty/Kyty.ini. Preserve it before launching from an extracted release.
if ([System.IO.Path]::GetFullPath($runtime) -eq [System.IO.Path]::GetFullPath($PSScriptRoot)) {
    $localSettings = Join-Path $runtime 'Kyty.ini'
    if (Test-Path -LiteralPath $localSettings -PathType Leaf) {
        $backup = Join-Path $runtime ("Kyty.ini.release-local-{0}-{1}.bak" -f `
            (Get-Date -Format 'yyyyMMdd-HHmmss'), [guid]::NewGuid().ToString('N').Substring(0, 8))
        Move-Item -LiteralPath $localSettings -Destination $backup -ErrorAction Stop
        Write-Output "Backed up release-local settings to $backup"
    }
}

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
Write-Output "Started U59 launcher PID $($process.Id) with the no-diagnostics preset."
