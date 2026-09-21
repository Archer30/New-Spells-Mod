param([string] $GameDirectory = 'D:\Games\Heroes 3 ERA')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (Get-Process h3era -ErrorAction SilentlyContinue) { throw 'Close Heroes III before deploying.' }
$backup = Join-Path $root ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-before-final-blizzard-deployment')
$entries = @()
foreach ($file in (Get-ChildItem -LiteralPath (Join-Path $root 'dist\New Spells Expansion') -File -Recurse)) {
    $relative = $file.FullName.Substring((Join-Path $root 'dist\New Spells Expansion').Length + 1)
    if ($file.Extension -in @('.map','.pdb') -or $file.Name -like '*probe*') { throw "Unshippable file: $relative" }
    $entries += @{Mod='New Spells Expansion'; Relative=$relative; Source=$file.FullName}
}
foreach ($relative in @('EraPlugins\NewSpells.dll','DebugMaps\NewSpells.dbgmap','README.md','mod.json')) {
    $entries += @{Mod='New Spells'; Relative=$relative; Source=(Join-Path $root "dist\New Spells\$relative")}
}
foreach ($entry in $entries) {
    $modRoot = [IO.Path]::GetFullPath((Join-Path $GameDirectory "Mods\$($entry.Mod)"))
    $entry.Target = [IO.Path]::GetFullPath((Join-Path $modRoot $entry.Relative))
    if (-not $entry.Target.StartsWith($modRoot + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Deployment escaped the named mod.' }
    $entry.Backup = Join-Path $backup "$($entry.Mod)\$($entry.Relative)"
    $entry.Existed = Test-Path -LiteralPath $entry.Target
    if ($entry.Existed) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Backup) -Force | Out-Null
        Copy-Item -LiteralPath $entry.Target -Destination $entry.Backup
    }
}
$passed = $false
try {
    foreach ($entry in $entries) {
        New-Item -ItemType Directory -Path (Split-Path -Parent $entry.Target) -Force | Out-Null
        Copy-Item -LiteralPath $entry.Source -Destination $entry.Target -Force
        if ([Convert]::ToBase64String([IO.File]::ReadAllBytes($entry.Source)) -cne
            [Convert]::ToBase64String([IO.File]::ReadAllBytes($entry.Target))) { throw "Byte comparison failed: $($entry.Target)" }
    }
    $startupLog = Join-Path $root 'work\blizzard-deployed-startup.txt'
    & (Join-Path $root 'tests\ProbeBlizzardStartup.ps1') -GameDirectory $GameDirectory -StartupSeconds 10 *> $startupLog
    $passed = $true
}
finally {
    if (-not $passed) {
        foreach ($entry in $entries) {
            if ($entry.Existed) { Copy-Item -LiteralPath $entry.Backup -Destination $entry.Target -Force }
            elseif (Test-Path -LiteralPath $entry.Target) { Remove-Item -LiteralPath $entry.Target -Force }
        }
    }
}
$report = @("Verified deployment: $($entries.Count) files; every file compared byte-for-byte.", "Backup: $backup")
$report | Set-Content -LiteralPath (Join-Path $root 'work\blizzard-deployment-result.txt') -Encoding utf8
$report | Write-Output
