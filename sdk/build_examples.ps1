[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$sdkRoot = $PSScriptRoot
& (Join-Path $sdkRoot 'tests\verify_header_sync.ps1')
& (Join-Path $sdkRoot 'tests\validate_example_json.ps1')

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere -PathType Leaf)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$msbuildCandidates = & $vswhere -all -products * `
    -requires Microsoft.Component.MSBuild `
    -find 'MSBuild\**\Bin\MSBuild.exe'
$msbuild = $msbuildCandidates | Where-Object {
    $marker = $_.IndexOf('\MSBuild\', [StringComparison]::OrdinalIgnoreCase)
    if ($marker -lt 0) {
        return $false
    }
    $installationRoot = $_.Substring(0, $marker)
    $vcTargets = Join-Path $installationRoot 'MSBuild\Microsoft\VC'
    return @(Get-ChildItem -LiteralPath $vcTargets -Directory -Recurse `
        -Filter 'v141_xp' -ErrorAction SilentlyContinue).Count -gt 0
} | Select-Object -First 1
if (-not $msbuild) {
    throw 'MSBuild with the v141_xp platform toolset was not found.'
}

& $msbuild (Join-Path $sdkRoot 'NewSpellsProviderSdk.sln') `
    /m:1 /t:Rebuild "/p:Configuration=$Configuration" /p:Platform=Win32
if ($LASTEXITCODE -ne 0) {
    throw "SDK provider build failed with exit code $LASTEXITCODE."
}
