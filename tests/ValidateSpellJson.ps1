$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
$langRoot = Join-Path $root 'dist\New Spells\Lang'
$basePath = Join-Path $langRoot 'NewSpells.json'
$localePaths = @(
    (Join-Path $langRoot 'ru\NewSpells.json'),
    (Join-Path $langRoot 'cn\NewSpells.json')
)
$expansionLangRoot = Join-Path $root 'dist\New Spells Expansion\Lang'
$expansionJsonName = 'NewSpellsExpansion.NewSpells.json'
$expansionBasePath = Join-Path $expansionLangRoot $expansionJsonName
$expansionLocalePaths = @(
    (Join-Path $expansionLangRoot "ru\$expansionJsonName"),
    (Join-Path $expansionLangRoot "cn\$expansionJsonName")
)
$expectedIds = @(71, 73, 75, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95)
$scalarFields = @('type', 'soundName', 'flags', 'name', 'shortName', 'level', 'school', 'spEffect')
$arrayFields = @(
    @{ Name = 'manaCost'; Count = 4 },
    @{ Name = 'baseValue'; Count = 4 },
    @{ Name = 'chanceToGet'; Count = 9 },
    @{ Name = 'aiValue'; Count = 4 },
    @{ Name = 'description'; Count = 4 }
)
$logicalAnimations = @{
    81 = 'Fear'
    82 = 'DeathCloud'
    83 = 'DeathBlow'
    90 = 'Toughness'
    91 = 'Claws'
    92 = 'Incineration'
    94 = 'HourOfPower'
    95 = 'GoldenTouch'
}

function Require($condition, [string] $message) {
    if (-not $condition) {
        throw $message
    }
}

function Read-Json([string] $path) {
    Require (Test-Path -LiteralPath $path) "Missing JSON file: $path"
    # Windows PowerShell 5.1 treats BOM-less UTF-8 as the active ANSI code
    # page unless encoding is explicit.  The shipped Russian/Chinese overlays
    # are UTF-8, so make the validator behave identically in pwsh and the
    # Windows PowerShell version commonly used with ERA tooling.
    return Get-Content -LiteralPath $path -Raw -Encoding UTF8 | ConvertFrom-Json
}

$baseRaw = Get-Content -LiteralPath $basePath -Raw -Encoding UTF8
$base = $baseRaw | ConvertFrom-Json
Require ((Split-Path -Leaf $basePath) -ne (Split-Path -Leaf $expansionBasePath)) `
    'Core and provider JSON files must not share one VFS-relative filename.'
Require ((Split-Path -Leaf $expansionBasePath) -like '*NewSpells.json') `
    'Provider JSON filename must use the *NewSpells.json discovery suffix.'
$actualIds = @($base.era.spells.PSObject.Properties.Name | ForEach-Object { [int] $_ })
Require (($actualIds -join ',') -eq ($expectedIds -join ',')) 'Base JSON spell IDs differ from the supported list.'
Require ([int] $base.NewSpells.Config.MaxSpellId -eq 95) 'NewSpells.Config.MaxSpellId must be 95 for the core package.'

foreach ($id in $expectedIds) {
    $spell = $base.era.spells.PSObject.Properties[[string] $id].Value
    foreach ($field in $scalarFields) {
        $property = $spell.PSObject.Properties[$field]
        Require ($null -ne $property) "Spell $id is missing $field."
        Require ($property.Value -is [string]) "Spell $id field $field must be a JSON string."
        Require ($property.Value.Length -gt 0) "Spell $id field $field is empty."
    }

    Require ([int] $spell.level -ge 1 -and [int] $spell.level -le 5) "Spell $id has an invalid hero-spell level."
    Require (([int] $spell.flags -band 8) -eq 0) "Spell $id unexpectedly carries the native creature-spell flag."

    foreach ($arrayField in $arrayFields) {
        $container = $spell.PSObject.Properties[$arrayField.Name].Value
        Require ($null -ne $container) "Spell $id is missing $($arrayField.Name)."
        for ($index = 0; $index -lt $arrayField.Count; ++$index) {
            $element = $container.PSObject.Properties[[string] $index]
            Require ($null -ne $element) "Spell $id is missing $($arrayField.Name).$index."
            Require ($element.Value -is [string]) "Spell $id field $($arrayField.Name).$index must be a JSON string."
        }
    }

    $animationKey = $base.NewSpells.Spells.PSObject.Properties[[string] $id].Value.animationKey
    if ($logicalAnimations.ContainsKey($id)) {
        Require ($animationKey -eq $logicalAnimations[$id]) "Spell $id has the wrong logical animation key."
        Require ($null -eq $spell.PSObject.Properties['animationIndex']) "Spell $id must not pin a relocated animationIndex."
    }
    else {
        Require ($null -ne $spell.PSObject.Properties['animationIndex']) "Spell $id is missing a stable animationIndex."
    }
}

Require ($base.NewSpells.Spells.'88'.requiresBattleBetweenCasts -eq '1') 'Mobility private behavior is missing.'
Require ($base.NewSpells.Spells.'92'.permanentCasualties -eq '1') 'Incineration private behavior is missing.'
Require ($base.NewSpells.Spells.'93'.temporarySpeedPenalty -eq '1') 'Explosion private behavior is missing.'
Require ($base.NewSpells.Spells.'94'.maxDurationRounds -eq '3') 'Hour of Power duration cap is missing.'
Require ($baseRaw -notmatch 'Enabled Spells|<Enabled>|UI\.QueueFix|CumulativeUnicornAura') 'Obsolete INI-era controls remain in the base JSON.'
Require ($null -eq $base.NewSpells.PSObject.Properties['ExternalSpells']) 'Core JSON must not declare external spell slots.'

foreach ($localePath in $localePaths) {
    $locale = Read-Json $localePath
    $localeIds = @($locale.era.spells.PSObject.Properties.Name | ForEach-Object { [int] $_ })
    Require (($localeIds -join ',') -eq ($expectedIds -join ',')) "Locale spell IDs differ in $localePath."
    foreach ($id in $expectedIds) {
        $spell = $locale.era.spells.PSObject.Properties[[string] $id].Value
        Require ($spell.name -is [string] -and $spell.name.Length -gt 0) "Locale name is missing for spell $id in $localePath."
        Require ($spell.shortName -is [string] -and $spell.shortName.Length -gt 0) "Locale shortName is missing for spell $id in $localePath."
    }
}

$expansion = Read-Json $expansionBasePath
$expansionIds = @($expansion.era.spells.PSObject.Properties.Name | ForEach-Object { [int] $_ })
Require (($expansionIds -join ',') -eq '96,97') 'New Spells Expansion must define spell IDs 96 and 97.'
$reinforcements = $expansion.era.spells.'96'
foreach ($field in $scalarFields) {
    $property = $reinforcements.PSObject.Properties[$field]
    Require ($null -ne $property -and $property.Value -is [string] -and $property.Value.Length -gt 0) "Expansion spell 96 is missing $field."
}
foreach ($arrayField in $arrayFields) {
    $container = $reinforcements.PSObject.Properties[$arrayField.Name].Value
    Require ($null -ne $container) "Expansion spell 96 is missing $($arrayField.Name)."
    for ($index = 0; $index -lt $arrayField.Count; ++$index) {
        $element = $container.PSObject.Properties[[string] $index]
        Require ($null -ne $element -and $element.Value -is [string]) "Expansion spell 96 is missing $($arrayField.Name).$index."
    }
}
$declaration = $expansion.NewSpells.ExternalSpells.'96'
Require ($declaration.provider -eq 'HD.Plugin.H3.NewSpellsExpansion') 'Expansion provider identity is wrong.'
Require ($declaration.spellKey -eq 'Reinforcements') 'Expansion spell key is wrong.'
Require ($declaration.kind -eq 'adventure') 'Reinforcements must be an adventure spell.'
Require ($declaration.editorVisible -eq '1') 'Reinforcements must be visible in the map editor.'
Require ([int] $declaration.capabilities -eq 1) 'Reinforcements must declare only the adventure-cast capability.'
Require ($null -ne $expansion.NewSpellsExpansion.Spells.Reinforcements) 'Expansion mechanics/localization namespace is missing.'

foreach ($localePath in $expansionLocalePaths) {
    $locale = Read-Json $localePath
    $spell = $locale.era.spells.'96'
    Require ($spell.name -is [string] -and $spell.name.Length -gt 0) "Expansion locale name is missing in $localePath."
    Require ($spell.shortName -is [string] -and $spell.shortName.Length -gt 0) "Expansion locale shortName is missing in $localePath."
    Require ($null -ne $locale.NewSpellsExpansion.Spells.Reinforcements) "Expansion locale UI text is missing in $localePath."
}

$blizzard = $expansion.era.spells.'97'
Require ($blizzard.animationIndex -eq '34') 'Blizzard must supply the required native fallback animation index; the owned animation overrides it at activation.'
foreach ($field in $scalarFields) {
    Require ($blizzard.PSObject.Properties[$field].Value -is [string]) "Blizzard is missing $field."
}
foreach ($arrayField in $arrayFields) {
    $container = $blizzard.PSObject.Properties[$arrayField.Name].Value
    for ($index = 0; $index -lt $arrayField.Count; ++$index) {
        Require ($container.PSObject.Properties[[string]$index].Value -is [string]) "Blizzard is missing $($arrayField.Name).$index."
    }
}
Require ($blizzard.level -eq '5' -and $blizzard.school -eq '4' -and $blizzard.spEffect -eq '20') 'Blizzard level/school/power are incorrect.'
Require (($blizzard.manaCost.PSObject.Properties.Value -join ',') -eq '25,20,20,20') 'Blizzard mana costs changed.'
Require (($blizzard.baseValue.PSObject.Properties.Value -join ',') -eq '30,30,60,120') 'Blizzard damage bonuses changed.'
Require ($blizzard.type -eq '0' -and $blizzard.flags -eq '66177') 'Blizzard must retain neutral Inferno area targeting.'
$blizzardDeclaration = $expansion.NewSpells.ExternalSpells.'97'
Require ($blizzardDeclaration.provider -eq $declaration.provider -and $blizzardDeclaration.spellKey -eq 'Blizzard') 'Blizzard identity differs from its native descriptor.'
Require ($blizzardDeclaration.capabilities -eq '2046' -and $blizzardDeclaration.combatTargetMode -eq 'area') 'Blizzard combat capabilities are incorrect.'
Require ($blizzardDeclaration.animationDef -eq 'NSEBLIZ.def' -and $blizzardDeclaration.animationType -eq '1') 'Blizzard animation is incorrect.'
Require ($blizzardDeclaration.animationKey -eq 'HD.Plugin.H3.NewSpellsExpansion.Blizzard.Area') 'Blizzard animation key is not namespaced.'
foreach ($localePath in $expansionLocalePaths) {
    $locale = Read-Json $localePath
    Require ($locale.era.spells.'97'.name.Length -gt 0) "Blizzard translation missing: $localePath"
    for ($mastery = 0; $mastery -lt 4; ++$mastery) {
        Require ($locale.era.spells.'97'.description.PSObject.Properties[[string]$mastery].Value.Length -gt 0) "Blizzard description missing: $localePath mastery $mastery"
    }
}
Write-Host 'Validated 18 core spells, expansion spells 96/97, and all locale overlays.' 
