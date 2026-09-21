[CmdletBinding()]
param(
    [string] $ManifestPath = (Join-Path $PSScriptRoot 'assets\spell-assets.json'),
    [string] $OutputPath = (Join-Path $PSScriptRoot '..\dist\New Spells Expansion\Data\NewSpellsExpansion_png_data.zip')
)

$ErrorActionPreference = 'Stop'
$manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
if ($manifest.formatVersion -ne 1 -or -not $manifest.spells) {
    throw 'Unsupported or empty external-spell asset manifest.'
}

$requiredDefs = @(
    'spells.def',
    'SpellScr.def',
    'SpellBon.def',
    'SpellInt.def'
)
$offsetDefs = @('SpellInt.def')
$expectedSize = @{
    'spells.def' = @(78, 65)
    'SpellScr.def' = @(83, 61)
    'SpellBon.def' = @(58, 64)
    'SpellInt.def' = @(48, 36)
}
$seenSpellIds = @{}

function Get-PngDimensions([string]$Path) {
    $bytes = [IO.File]::ReadAllBytes($Path)
    $signature = [byte[]](137, 80, 78, 71, 13, 10, 26, 10)
    if ($bytes.Length -lt 24) {
        throw "PNG is truncated: $Path"
    }
    for ($index = 0; $index -lt $signature.Length; ++$index) {
        if ($bytes[$index] -ne $signature[$index]) {
            throw "Source image is not a PNG: $Path"
        }
    }
    if ([Text.Encoding]::ASCII.GetString($bytes, 12, 4) -ne 'IHDR') {
        throw "PNG has no leading IHDR chunk: $Path"
    }
    $width = [int](
        ([uint32]$bytes[16] -shl 24) -bor
        ([uint32]$bytes[17] -shl 16) -bor
        ([uint32]$bytes[18] -shl 8) -bor
        [uint32]$bytes[19])
    $height = [int](
        ([uint32]$bytes[20] -shl 24) -bor
        ([uint32]$bytes[21] -shl 16) -bor
        ([uint32]$bytes[22] -shl 8) -bor
        [uint32]$bytes[23])
    return @($width, $height)
}

$stageRoot = Join-Path ([System.IO.Path]::GetTempPath()) ('new-spells-expansion-' + [guid]::NewGuid().ToString('N'))
$dataRoot = Join-Path $stageRoot 'Data\Defs'
New-Item -ItemType Directory -Path $dataRoot -Force | Out-Null

try {
    foreach ($spell in $manifest.spells) {
        $spellId = [int]$spell.spellId
        if ($spellId -lt 96 -or $spellId -gt 126 -or -not $spell.icons) {
            throw 'External spell ID or icon list is invalid.'
        }
        if ($seenSpellIds.ContainsKey($spellId)) {
            throw "Duplicate external spell ID in asset manifest: $spellId"
        }
        $seenSpellIds[$spellId] = $true
        if ([string]$spell.provider -notmatch '^[A-Za-z0-9._-]{1,63}$' -or
            [string]$spell.spellKey -notmatch '^[A-Za-z0-9._-]{1,63}$') {
            throw "Invalid provider identity for external spell $spellId."
        }

        $iconsByDef = @{}
        foreach ($icon in $spell.icons) {
            $defName = [string]$icon.def
            if ($requiredDefs -notcontains $defName -or
                $iconsByDef.ContainsKey($defName)) {
                throw "Unknown or duplicate DEF '$defName' for external spell $spellId."
            }
            $iconsByDef[$defName] = [string]$icon.source
        }
        foreach ($defName in $requiredDefs) {
            if (-not $iconsByDef.ContainsKey($defName)) {
                throw "Missing $defName icon for external spell $spellId."
            }
            $source = Join-Path (Split-Path -Parent $ManifestPath) $iconsByDef[$defName]
            if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
                throw "Missing source image: $source"
            }
            $dimensions = Get-PngDimensions $source
            if ($dimensions[0] -ne $expectedSize[$defName][0] -or
                $dimensions[1] -ne $expectedSize[$defName][1]) {
                throw "$defName icon for spell $spellId must be $($expectedSize[$defName][0])x$($expectedSize[$defName][1]), found $($dimensions[0])x$($dimensions[1])."
            }

            $frame = $spellId + $(if ($offsetDefs -contains $defName) { 1 } else { 0 })
            $defDirectory = Join-Path $dataRoot $defName
            New-Item -ItemType Directory -Path $defDirectory -Force | Out-Null
            Copy-Item -LiteralPath $source -Destination (Join-Path $defDirectory ('0_' + $frame + '.png'))
        }
    }

    $outputDirectory = Split-Path -Parent $OutputPath
    New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null
    if (Test-Path -LiteralPath $OutputPath) {
        Remove-Item -LiteralPath $OutputPath -Force
    }
    Compress-Archive -LiteralPath (Join-Path $stageRoot 'Data') -DestinationPath $OutputPath -CompressionLevel Optimal
}
finally {
    if (Test-Path -LiteralPath $stageRoot) {
        $resolvedStage = [IO.Path]::GetFullPath($stageRoot)
        $resolvedTemp = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
        if (-not $resolvedStage.StartsWith($resolvedTemp, [StringComparison]::OrdinalIgnoreCase) -or
            (Split-Path -Leaf $resolvedStage) -notlike 'new-spells-expansion-*') {
            throw 'Asset staging cleanup escaped the temporary directory.'
        }
        Remove-Item -LiteralPath $stageRoot -Recurse -Force
    }
}

# Animation payloads stay byte-identical to their source DEFs. Store raw LOD
# members to avoid any palette/frame conversion or external packer dependency.
if ($manifest.animations) {
    $members = @()
    $seen = @{}
    foreach ($animation in $manifest.animations) {
        $name = [string]$animation.name
        if ($name -notmatch '^[A-Za-z0-9_]{1,11}\.def$' -or $seen.ContainsKey($name)) {
            throw "Invalid or duplicate animation member: $name"
        }
        $seen[$name] = $true
        $source = Join-Path (Split-Path -Parent $ManifestPath) $animation.source
        $bytes = [IO.File]::ReadAllBytes($source)
        if ($bytes.Length -lt 784) { throw "Truncated DEF: $source" }
        $members += @{ Name = $name; Data = $bytes }
    }
    $stream = [IO.MemoryStream]::new()
    $writer = [IO.BinaryWriter]::new($stream)
    try {
        $header = [byte[]]::new(92)
        [Text.Encoding]::ASCII.GetBytes('LOD').CopyTo($header, 0)
        [BitConverter]::GetBytes([uint32]200).CopyTo($header, 4)
        [BitConverter]::GetBytes([uint32]$members.Count).CopyTo($header, 8)
        $writer.Write($header)
        $offset = 92 + 32 * $members.Count
        foreach ($member in $members) {
            $name = [byte[]]::new(16)
            [Text.Encoding]::ASCII.GetBytes($member.Name).CopyTo($name, 0)
            $writer.Write($name)
            $writer.Write([uint32]$offset)
            $writer.Write([uint32]$member.Data.Length)
            $writer.Write([uint32]0)
            $writer.Write([uint32]0)
            $offset += $member.Data.Length
        }
        foreach ($member in $members) { $writer.Write([byte[]]$member.Data) }
        $writer.Flush()
        [IO.File]::WriteAllBytes((Join-Path (Split-Path -Parent $OutputPath) 'NewSpellsExpansion.pac'), $stream.ToArray())
    }
    finally { $writer.Dispose(); $stream.Dispose() }
}
