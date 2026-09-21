[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$sdkRoot = Split-Path -Parent $PSScriptRoot
$repositoryRoot = Split-Path -Parent $sdkRoot
$canonical = Join-Path $repositoryRoot 'NewSpells\NewSpellsProviderApi.h'
$published = Join-Path $sdkRoot 'include\NewSpellsProviderApi.h'

if (-not (Test-Path -LiteralPath $canonical -PathType Leaf) -or
    -not (Test-Path -LiteralPath $published -PathType Leaf)) {
    throw 'The canonical or published NewSpellsProviderApi.h is missing.'
}

$canonicalBytes = [IO.File]::ReadAllBytes($canonical)
$publishedBytes = [IO.File]::ReadAllBytes($published)
$identical = [System.Linq.Enumerable]::SequenceEqual(
    [byte[]]$canonicalBytes, [byte[]]$publishedBytes)

if (-not $identical) {
    throw 'SDK header drift detected. Copy NewSpells\NewSpellsProviderApi.h byte-for-byte to sdk\include.'
}

Write-Output 'NewSpellsProviderApi.h is byte-identical in core and SDK.'
