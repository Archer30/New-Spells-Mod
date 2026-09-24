param(
    [string] $GameDirectory = (Join-Path $PSScriptRoot '..\work\era3924'),
    [string] $ProbeBuild = (Join-Path $PSScriptRoot '..\work\ceiling-probe-build'),
    [int] $StartupSeconds = 15
)
$ErrorActionPreference = 'Stop'
$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$sandbox = [IO.Path]::GetFullPath($GameDirectory)
if (-not $sandbox.StartsWith($workspace + '\work\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Regression fixtures may run only in a workspace game copy.'
}
$fixtureName = 'New Spells Ceiling Fixture'
$fixtureRoot = Join-Path $sandbox ('Mods\' + $fixtureName)
if (Test-Path -LiteralPath $fixtureRoot) {
    throw "The temporary fixture directory already exists: $fixtureRoot"
}
foreach ($required in @('h3era.exe', 'Mods\list.txt', 'Mods\WoG', 'Mods\New Spells')) {
    if (-not (Test-Path -LiteralPath (Join-Path $sandbox $required))) {
        throw "The disposable game copy is missing $required"
    }
}
foreach ($required in @('NewSpells.dll', 'DebugMaps\NewSpells.dbgmap')) {
    if (-not (Test-Path -LiteralPath (Join-Path $ProbeBuild $required))) {
        throw "Build the ceiling probe first; missing $required"
    }
}
$backup = Join-Path $workspace ('backups\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-ceiling-fixture')
New-Item -ItemType Directory -Path $backup | Out-Null
$modList = Join-Path $sandbox 'Mods\list.txt'
$modListBefore = [IO.File]::ReadAllBytes($modList)
[IO.File]::WriteAllBytes((Join-Path $backup 'list.txt'), $modListBefore)
try {
    # Use the checked-in sample's real spell metadata and animation resources.
    Copy-Item -LiteralPath (Join-Path $workspace 'examples\New Spells Sample Pack') -Destination $fixtureRoot -Recurse
    $jsonPath = Join-Path $fixtureRoot 'Lang\SamplePack.NewSpells.json'
    $data = Get-Content -LiteralPath $jsonPath -Raw | ConvertFrom-Json
    $spell199 = $data.era.spells.'150' | ConvertTo-Json -Depth 20 | ConvertFrom-Json
    $spell199.name = 'Ceiling regression'
    $spell199.shortName = 'Ceiling regression'
    $data.era.spells | Add-Member -MemberType NoteProperty -Name '199' -Value $spell199
    $definition199 = $data.NewSpells.DataSpells.'150' | ConvertTo-Json -Depth 20 | ConvertFrom-Json
    $definition199.spellKey = 'CeilingRegression199'
    $data.NewSpells.DataSpells | Add-Member -MemberType NoteProperty -Name '199' -Value $definition199
    $data.NewSpells | Add-Member -MemberType NoteProperty -Name 'Config' -Value ([pscustomobject]@{MaxSpellId='199'})
    $data.NewSpells | Add-Member -MemberType NoteProperty -Name 'Test' -Value ([pscustomobject]@{CeilingFixture='1'})
    [IO.File]::WriteAllText($jsonPath, ($data | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))

    # A fixed mod list prevents unrelated providers from changing test coverage.
    $mods = @('WoG')
    if (Test-Path -LiteralPath (Join-Path $sandbox 'Mods\Era Erm Framework')) { $mods += 'Era Erm Framework' }
    $mods += @('New Spells', $fixtureName)
    [IO.File]::WriteAllText($modList, (($mods -join "`r`n") + "`r`n"), [Text.UTF8Encoding]::new($false))
    & (Join-Path $PSScriptRoot 'ProbeSpellCeiling.ps1') -GameDirectory $sandbox -ProbeBuild $ProbeBuild `
        -ExpectedSpellCount 200 -ExpectedDataSpells 2 -StartupSeconds $StartupSeconds
    $log = Get-Content -LiteralPath (Join-Path $workspace 'work\ceiling-probe-log.txt') -Raw
    if ($log -notmatch 'Coverage\s+2 active enchantment\(s\) tested \(required regression fixture\)') {
        throw 'The DLL did not run the required regression fixture; rebuild the ceiling probe.'
    }
}
finally {
    [IO.File]::WriteAllBytes($modList, $modListBefore)
    if (Test-Path -LiteralPath $fixtureRoot) {
        $resolvedFixture = (Resolve-Path -LiteralPath $fixtureRoot).ProviderPath
        $archivedFixture = [IO.Path]::GetFullPath((Join-Path $backup 'Fixture'))
        if (-not $resolvedFixture.StartsWith($sandbox + '\Mods\', [StringComparison]::OrdinalIgnoreCase) -or
            -not $archivedFixture.StartsWith($workspace + '\backups\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Fixture archive paths escaped the workspace.'
        }
        Move-Item -LiteralPath $resolvedFixture -Destination $archivedFixture
    }
    Write-Output "Restored the mod list; fixture configuration archived in $backup."
}
