[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$sdkRoot = Split-Path -Parent $PSScriptRoot
$knownSpellFlagMask = 0x001FFFFF
$nativeAdventureAiFlag = 0x00100000
$allProviderCapabilities = 0x00000FFF

function Get-JsonProperty([object]$Object, [string]$Name, [string]$Label) {
    if ($null -eq $Object) {
        throw "$Label is missing."
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        throw "$Label.$Name is missing."
    }
    return $property.Value
}

function Require-Integer(
    [object]$Value,
    [int]$Minimum,
    [int]$Maximum,
    [string]$Label
) {
    $number = 0
    if ($null -eq $Value -or
        -not [int]::TryParse(
            [string]$Value,
            [Globalization.NumberStyles]::Integer,
            [Globalization.CultureInfo]::InvariantCulture,
            [ref]$number) -or
        $number -lt $Minimum -or $number -gt $Maximum) {
        throw "$Label must be an integer from $Minimum through $Maximum."
    }
    return $number
}

function Require-Text([object]$Value, [string]$Label) {
    if ($null -eq $Value -or [string]::IsNullOrEmpty([string]$Value)) {
        throw "$Label must be nonempty."
    }
    return [string]$Value
}

$samples = @(
    @{
        Path = 'examples\MinimalAdventureProvider\MinimalAdventureProvider.NewSpells.json'
        Id = 97
        Provider = 'HD.Plugin.H3.NewSpellsSdkExample'
        SpellKey = 'ExampleAdventure'
        Kind = 'adventure'
        Capabilities = 1
    },
    @{
        Path = 'tests\AllCallbacksProvider\AllCallbacksProvider.NewSpells.json'
        Id = 123
        Provider = 'HD.Plugin.H3.NewSpellsSdkTest'
        SpellKey = 'TargetedTimedStatus'
        Kind = 'combat'
        CombatTargetMode = 'targeted'
        Capabilities = 2046
    },
    @{
        Path = 'tests\AllCallbacksProvider\AllCallbacksProvider.NewSpells.json'
        Id = 124
        Provider = 'HD.Plugin.H3.NewSpellsSdkTest'
        SpellKey = 'AreaDamage'
        Kind = 'combat'
        CombatTargetMode = 'area'
        Capabilities = 1926
    },
    @{
        Path = 'tests\AllCallbacksProvider\AllCallbacksProvider.NewSpells.json'
        Id = 125
        Provider = 'HD.Plugin.H3.NewSpellsSdkTest'
        SpellKey = 'GlobalHybrid'
        Kind = 'hybrid'
        CombatTargetMode = 'global'
        Capabilities = 3207
    },
    @{
        Path = 'tests\AllCallbacksProvider\AllCallbacksProvider.NewSpells.json'
        Id = 126
        Provider = 'HD.Plugin.H3.NewSpellsSdkTest'
        SpellKey = 'Summon'
        Kind = 'combat'
        CombatTargetMode = 'summon'
        Capabilities = 1926
    }
)

foreach ($sample in $samples) {
    $path = Join-Path $sdkRoot $sample.Path
    $document = Get-Content -Raw -LiteralPath $path | ConvertFrom-Json
    $id = [string]$sample.Id
    $spell = Get-JsonProperty $document.era.spells $id 'era.spells'
    $declaration = Get-JsonProperty $document.NewSpells.ExternalSpells $id `
        'NewSpells.ExternalSpells'

    $null = Require-Integer $spell.type -128 127 "era.spells.$id.type"
    $null = Require-Text $spell.soundName "era.spells.$id.soundName"
    $null = Require-Integer $spell.animationIndex 0 82 `
        "era.spells.$id.animationIndex"
    $null = Require-Text $spell.name "era.spells.$id.name"
    $null = Require-Text $spell.shortName "era.spells.$id.shortName"
    $null = Require-Integer $spell.level 1 5 "era.spells.$id.level"
    $school = Require-Integer $spell.school 0 15 "era.spells.$id.school"
    $flags = Require-Integer $spell.flags 0 ([int]::MaxValue) `
        "era.spells.$id.flags"
    if (($flags -band $knownSpellFlagMask) -ne $flags) {
        throw "era.spells.$id.flags contains bits outside the known native mask."
    }
    if (($flags -band $nativeAdventureAiFlag) -ne 0) {
        throw "era.spells.$id.flags must not use SF_AI_ADVENTUREMAP; adventure AI is callback-owned."
    }
    $null = Require-Integer $spell.spEffect -1000000 1000000 `
        "era.spells.$id.spEffect"

    foreach ($mastery in 0..3) {
        $key = [string]$mastery
        $null = Require-Integer `
            (Get-JsonProperty $spell.manaCost $key "era.spells.$id.manaCost") `
            0 32767 "era.spells.$id.manaCost.$key"
        $null = Require-Integer `
            (Get-JsonProperty $spell.baseValue $key "era.spells.$id.baseValue") `
            -1000000 1000000 "era.spells.$id.baseValue.$key"
        $null = Require-Integer `
            (Get-JsonProperty $spell.aiValue $key "era.spells.$id.aiValue") `
            0 1000000000 "era.spells.$id.aiValue.$key"
        $null = Require-Text `
            (Get-JsonProperty $spell.description $key `
                "era.spells.$id.description") `
            "era.spells.$id.description.$key"
    }

    foreach ($town in 0..8) {
        $key = [string]$town
        $null = Require-Integer `
            (Get-JsonProperty $spell.chanceToGet $key `
                "era.spells.$id.chanceToGet") `
            0 100 "era.spells.$id.chanceToGet.$key"
    }

    $provider = Require-Text $declaration.provider `
        "NewSpells.ExternalSpells.$id.provider"
    $spellKey = Require-Text $declaration.spellKey `
        "NewSpells.ExternalSpells.$id.spellKey"
    if ($provider -notmatch '^[A-Za-z0-9._-]+$' -or
        $spellKey -notmatch '^[A-Za-z0-9._-]+$' -or
        $provider.Length -gt 63 -or $spellKey.Length -gt 63 -or
        $provider -ne $sample.Provider -or $spellKey -ne $sample.SpellKey) {
        throw "External spell $id has an invalid or mismatched identity."
    }

    if ([string]$declaration.kind -ne $sample.Kind) {
        throw "External spell $id has a mismatched kind."
    }
    $null = Require-Integer $declaration.editorVisible 0 1 `
        "NewSpells.ExternalSpells.$id.editorVisible"
    $capabilitiesValue = Get-JsonProperty $declaration 'capabilities' `
        "NewSpells.ExternalSpells.$id"
    $capabilities = Require-Integer $capabilitiesValue 1 `
        $allProviderCapabilities `
        "NewSpells.ExternalSpells.$id.capabilities"
    if ($capabilities -ne $sample.Capabilities) {
        throw "External spell $id has a mismatched capability mask."
    }

    $hasAdventureCast = ($capabilities -band 0x001) -ne 0
    $hasCombatCast = ($capabilities -band 0x004) -ne 0
    $combatDomainCapabilities = 0x7FE
    $capabilityKindMatches = switch ($sample.Kind) {
        'adventure' {
            $hasAdventureCast -and
                (($capabilities -band $combatDomainCapabilities) -eq 0)
        }
        'combat' { $hasCombatCast -and -not $hasAdventureCast }
        'hybrid' { $hasAdventureCast -and $hasCombatCast }
        default { $false }
    }
    if (-not $capabilityKindMatches -or
        (($capabilities -band 0x002) -ne 0 -and -not $hasCombatCast) -or
        (($capabilities -band 0x400) -ne 0 -and -not $hasCombatCast) -or
        (($capabilities -band 0x800) -ne 0 -and -not $hasAdventureCast)) {
        throw "External spell $id capabilities do not match kind '$($sample.Kind)'."
    }

    $isAdventure = ($flags -band 2) -ne 0
    $isCombat = ($flags -band 1) -ne 0
    $kindMatches = switch ($sample.Kind) {
        'adventure' { $isAdventure -and -not $isCombat }
        'combat' { $isCombat -and -not $isAdventure }
        'hybrid' { $isAdventure -and $isCombat }
        default { $false }
    }
    if (-not $kindMatches) {
        throw "External spell $id flags do not match kind '$($sample.Kind)'."
    }

    $singleTarget = ($flags -band 0x10) -ne 0
    $areaTarget = ($flags -band (0x80 -bor 0x10000)) -ne 0
    $summonsCreatures = ($flags -band 0x80000) -ne 0
    $inferredTargetMode = if ($singleTarget) {
        'targeted'
    } elseif ($areaTarget) {
        'area'
    } elseif ($summonsCreatures) {
        'summon'
    } else {
        'global'
    }
    $targetModeProperty = $declaration.PSObject.Properties['combatTargetMode']
    if ($sample.Kind -eq 'adventure') {
        if ($null -ne $targetModeProperty) {
            throw "Adventure-only external spell $id must not declare combatTargetMode."
        }
    } else {
        $selectedTargetMode = $inferredTargetMode
        if ($null -ne $targetModeProperty) {
            $selectedTargetMode = Require-Text $targetModeProperty.Value `
                "NewSpells.ExternalSpells.$id.combatTargetMode"
            if ($selectedTargetMode -ne $inferredTargetMode) {
                throw "External spell $id has a combatTargetMode inconsistent with its flags."
            }
        }
        if ($sample.CombatTargetMode -and
            $selectedTargetMode -ne $sample.CombatTargetMode) {
            throw "External spell $id has a mismatched combatTargetMode."
        }
        $targetShapeMatches = switch ($selectedTargetMode) {
            'targeted' { $singleTarget -and -not $areaTarget -and -not $summonsCreatures }
            'area' { -not $singleTarget -and $areaTarget -and -not $summonsCreatures }
            'global' { -not $singleTarget -and -not $areaTarget -and -not $summonsCreatures }
            'summon' { -not $singleTarget -and -not $areaTarget -and $summonsCreatures }
            default { $false }
        }
        if (-not $targetShapeMatches) {
            throw "External spell $id has conflicting native target flags."
        }
    }

    $animationDefProperty = $declaration.PSObject.Properties['animationDef']
    $animationNameProperty = $declaration.PSObject.Properties['animationName']
    $animationTypeProperty = $declaration.PSObject.Properties['animationType']
    $animationKeyProperty = $declaration.PSObject.Properties['animationKey']
    $hasCustomAnimation = $null -ne $animationDefProperty -or
        $null -ne $animationNameProperty -or $null -ne $animationTypeProperty
    if ($hasCustomAnimation) {
        if ($null -eq $animationDefProperty -or $null -eq $animationNameProperty -or
            $null -eq $animationTypeProperty) {
            throw "External spell $id must declare the complete custom-animation tuple."
        }
        $animationDef = Require-Text $animationDefProperty.Value `
            "NewSpells.ExternalSpells.$id.animationDef"
        $animationName = Require-Text $animationNameProperty.Value `
            "NewSpells.ExternalSpells.$id.animationName"
        $null = Require-Integer $animationTypeProperty.Value 0 65535 `
            "NewSpells.ExternalSpells.$id.animationType"
        if ($animationDef -notmatch '^[A-Za-z0-9._-]+$' -or
            $animationName -notmatch '^[A-Za-z0-9._-]+$' -or
            $animationDef.Length -gt 63 -or $animationName.Length -gt 63) {
            throw "External spell $id has an invalid custom-animation identity."
        }
        $expectedAnimationKey = "$provider.$spellKey.$animationName"
        if ($expectedAnimationKey.Length -gt 191 -or
            ($null -ne $animationKeyProperty -and
             [string]$animationKeyProperty.Value -ne $expectedAnimationKey)) {
            throw "External spell $id has a mismatched custom animationKey."
        }
    } elseif ($null -ne $animationKeyProperty) {
        $animationKey = Require-Text $animationKeyProperty.Value `
            "NewSpells.ExternalSpells.$id.animationKey"
        if ($animationKey -notmatch '^[A-Za-z0-9._-]+$' -or
            $animationKey.Length -gt 191 -or
            -not $animationKey.StartsWith("$provider.")) {
            throw "External spell $id has an invalid namespaced animationKey."
        }
    }
}

Write-Output 'SDK example JSON records satisfy the core external-spell schema.'
