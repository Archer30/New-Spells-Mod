param(
    [string] $GameDirectory = (Join-Path $PSScriptRoot '..\work\blizzard-runtime-sandbox'),
    [string] $ProbeBuild = (Join-Path $PSScriptRoot '..\work\bmg-native-build')
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sandbox = [IO.Path]::GetFullPath($GameDirectory)
if (-not $sandbox.StartsWith($workspace + '\work\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The destructive startup probe may run only in a workspace game copy.'
}
$backup = Join-Path $workspace ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-bmg-probe-sandbox')
$entries = @(
    @{Source='NewSpells.dll'; Target='EraPlugins\NewSpells.dll'},
    @{Source='DebugMaps\NewSpells.dbgmap'; Target='DebugMaps\NewSpells.dbgmap'}
)
$process = $null
foreach ($entry in $entries) {
    $entry.Path = Join-Path $sandbox ('Mods\New Spells\' + $entry.Target)
    $entry.Backup = Join-Path $backup $entry.Target
    New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Backup) -Force | Out-Null
    Copy-Item -LiteralPath $entry.Path -Destination $entry.Backup
}
try {
    foreach ($entry in $entries) {
        Copy-Item -LiteralPath (Join-Path $ProbeBuild $entry.Source) -Destination $entry.Path -Force
    }
    $process = Start-Process -FilePath (Join-Path $sandbox 'h3era.exe') -WorkingDirectory $sandbox -WindowStyle Hidden -PassThru
    if (-not $process.WaitForExit(45000)) { throw 'BM:G native probe timed out.' }
    $process.Refresh()
    $log = Get-Content -LiteralPath (Join-Path $sandbox 'Debug\Era\log.txt') -Raw
    $log | Set-Content -LiteralPath (Join-Path $workspace 'work\bmg-native-probe-log.txt') -Encoding utf8
    if ($process.ExitCode -ne 0 -or $log -notmatch 'BMG native probe: Result\s+PASS: (\d+) checks') {
        Write-Output $log
        throw "BM:G native probe failed (exit $($process.ExitCode))."
    }
    Write-Output "BM:G native probe passed: $($Matches[1]) checks."
}
finally {
    if ($process -and -not $process.HasExited) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    foreach ($entry in $entries) { Copy-Item -LiteralPath $entry.Backup -Destination $entry.Path -Force }
}
