param(
    [string] $GameDirectory = (Join-Path $PSScriptRoot '..\work\era3924'),
    [string] $ProbeBuild = (Join-Path $PSScriptRoot '..\work\ceiling-probe-build'),
    [int] $ExpectedSpellCount = 96,
    [int] $ExpectedDataSpells = 0,
    [int] $StartupSeconds = 15
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sandbox = [IO.Path]::GetFullPath($GameDirectory)
if (-not $sandbox.StartsWith($workspace + '\work\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The startup probe may run only in a workspace game copy.'
}
$backup = Join-Path $workspace ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-ceiling-probe-sandbox')
$entries = @(
    @{Source='NewSpells.dll'; Target='EraPlugins\NewSpells.dll'},
    @{Source='DebugMaps\NewSpells.dbgmap'; Target='DebugMaps\NewSpells.dbgmap'}
)
$modListPath = Join-Path $sandbox 'Mods\list.txt'
$modListBefore = [IO.File]::ReadAllBytes($modListPath)
$logPath = Join-Path $sandbox 'Debug\Era\log.txt'
$process = $null
foreach ($entry in $entries) {
    $entry.Path = Join-Path $sandbox ('Mods\New Spells\' + $entry.Target)
    $entry.Backup = Join-Path $backup $entry.Target
    New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Backup) -Force | Out-Null
    if (Test-Path -LiteralPath $entry.Path) { Copy-Item -LiteralPath $entry.Path -Destination $entry.Backup }
}
try {
    foreach ($entry in $entries) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Path) -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $ProbeBuild $entry.Source) -Destination $entry.Path -Force
    }
    if (Test-Path -LiteralPath $logPath) { Remove-Item -LiteralPath $logPath -Force }
    $process = Start-Process -FilePath (Join-Path $sandbox 'h3era.exe') -WorkingDirectory $sandbox -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds $StartupSeconds
    $process.Refresh()
    # Some installs end the process from a plugin dialog before the main menu. ERA still
    # writes Debug\Era\log.txt then, so the log decides, not the exit code.
    if ($process.HasExited) {
        Write-Warning "Game exited during startup (exit $($process.ExitCode)); reading the dumped log."
    }
    else {
        $module = $process.Modules | Where-Object ModuleName -IEq 'NewSpells.dll' | Select-Object -First 1
        if (-not $module) { throw 'NewSpells.dll is not loaded.' }
        $probeBytes = [IO.File]::ReadAllBytes((Join-Path $ProbeBuild 'NewSpells.dll'))
        $pe = [BitConverter]::ToInt32($probeBytes, 0x3C)
        $imageSize = [BitConverter]::ToInt32($probeBytes, $pe + 24 + 56)
        if ($module.ModuleMemorySize -ne $imageSize) {
            throw "Another NewSpells.dll is loaded (image size $($module.ModuleMemorySize), expected $imageSize); check the mod order in Mods\list.txt."
        }
        # ERA writes Debug\Era\log.txt when the game closes, not while it runs.
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(30000)) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    }
    $log = Get-Content -LiteralPath $logPath -Raw
    $log | Set-Content -LiteralPath (Join-Path $workspace 'work\ceiling-probe-log.txt') -Encoding utf8
    if ($log -match 'Ceiling native probe: Result\s+FAIL[^\r\n]*') { throw "Native probe failed: $($Matches[0])" }
    if ($log -notmatch 'Ceiling native probe: Result\s+PASS: (\d+) checks, spell count (\d+), (\d+) data spell') {
        throw 'Startup log has no native probe result.'
    }
    $checks = [int]$Matches[1]; $count = [int]$Matches[2]; $data = [int]$Matches[3]
    if ($count -ne $ExpectedSpellCount) { throw "Unexpected spell count $count, expected $ExpectedSpellCount." }
    if ($data -ne $ExpectedDataSpells) { throw "Unexpected data spell count $data, expected $ExpectedDataSpells." }
    if ($log -notmatch "Startup\s+Spell count $ExpectedSpellCount") { throw 'Startup log has no matching spell count line.' }
    if ($log -match 'spell bound not hooked|site not hooked|External spell activation|registration of .* was rejected') {
        throw "Startup log reports a hook or activation failure: $($Matches[0])"
    }
    Write-Output "PASS: $checks native checks; spell count $count; $data data spell(s)."
}
finally {
    if ($process -and -not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(5000)) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    }
    foreach ($entry in $entries) {
        if (Test-Path -LiteralPath $entry.Backup) { Copy-Item -LiteralPath $entry.Backup -Destination $entry.Path -Force }
    }
    $modListAfter = [IO.File]::ReadAllBytes($modListPath)
    if ([Convert]::ToBase64String($modListBefore) -ne [Convert]::ToBase64String($modListAfter)) {
        [IO.File]::WriteAllBytes($modListPath, $modListBefore)
        Write-Warning 'Game startup changed Mods/list.txt (crash-loop protection); the file was restored.'
    }
}
