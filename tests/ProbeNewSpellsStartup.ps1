param(
    [string] $GameDirectory = 'D:\Games\Heroes 3 ERA',
    [int] $ExpectedSpellCount = 96,
    [int] $StartupSeconds = 12,
    [string] $ReleaseMapPath = (Join-Path $PSScriptRoot "..\build\core\obj\NewSpells.map")
)
$ErrorActionPreference = 'Stop'
$process = $null
$handle = [IntPtr]::Zero
$modListPath = Join-Path $GameDirectory 'Mods\list.txt'
$modListBefore = [IO.File]::ReadAllBytes($modListPath)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class BmgStartupRead {
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)]
    public static extern bool ReadProcessMemory(IntPtr process, IntPtr address,
        byte[] buffer, UIntPtr size, out UIntPtr read);
    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);
}
'@
function Read-LiveBytes([long] $Address, [int] $Count) {
    $bytes = [byte[]]::new($Count)
    $read = [UIntPtr]::Zero
    if (-not [BmgStartupRead]::ReadProcessMemory($handle, [IntPtr]$Address,
        $bytes, [UIntPtr]$Count, [ref]$read) -or $read.ToUInt64() -ne $Count) {
        throw "Cannot read live address $($Address.ToString('X'))."
    }
    return ,$bytes
}
try {
    $process = Start-Process -FilePath (Join-Path $GameDirectory 'h3era.exe') `
        -WorkingDirectory $GameDirectory -WindowStyle Hidden -PassThru
    Start-Sleep -Seconds $StartupSeconds
    $process.Refresh()
    if ($process.HasExited) { throw 'Game exited before startup validation.' }
    $handle = [BmgStartupRead]::OpenProcess(0x410, $false, $process.Id)
    if ($handle -eq [IntPtr]::Zero) { throw 'Cannot open game for read-only validation.' }
    $module = $process.Modules | Where-Object ModuleName -IEq 'NewSpells.dll' | Select-Object -First 1
    $expected = [IO.Path]::GetFullPath((Join-Path $GameDirectory 'Mods\New Spells\EraPlugins\NewSpells.dll'))
    if (-not $module) { throw 'New Spells module is missing.' }
    Write-Output "Loaded module: $($module.FileName) at $($module.BaseAddress.ToInt64().ToString('X'))."
    # ERA loads through a virtual EraPlugins path; Windows need not report
    # the physical Mods/New Spells path. Verify mapped handler bytes instead.
    $disk = [IO.File]::ReadAllBytes($expected)
    $pe = [BitConverter]::ToInt32($disk, 0x3C)
    $optional = $pe + 24
    $preferred = [BitConverter]::ToUInt32($disk, $optional + 28)
    $delta = $module.BaseAddress.ToInt64() - $preferred
    $sectionCount = [BitConverter]::ToUInt16($disk, $pe + 6)
    $sectionTable = $optional + [BitConverter]::ToUInt16($disk, $pe + 20)
    $mapped = [byte[]]::new([BitConverter]::ToInt32($disk, $optional + 56))
    for ($i = 0; $i -lt $sectionCount; ++$i) {
        $section = $sectionTable + $i * 40
        $rva = [BitConverter]::ToInt32($disk, $section + 12)
        $size = [BitConverter]::ToInt32($disk, $section + 16)
        $offset = [BitConverter]::ToInt32($disk, $section + 20)
        [Array]::Copy($disk, $offset, $mapped, $rva, $size)
    }
    $reloc = [BitConverter]::ToInt32($disk, $optional + 136)
    $relocEnd = $reloc + [BitConverter]::ToInt32($disk, $optional + 140)
    while ($reloc -lt $relocEnd) {
        $page = [BitConverter]::ToInt32($mapped, $reloc)
        $size = [BitConverter]::ToInt32($mapped, $reloc + 4)
        if ($size -lt 8) { throw 'Invalid relocation block.' }
        for ($i = 8; $i -lt $size; $i += 2) {
            $entry = [BitConverter]::ToUInt16($mapped, $reloc + $i)
            if (($entry -shr 12) -eq 3) {
                $rva = $page + ($entry -band 0xFFF)
                $value = ([long][BitConverter]::ToUInt32($mapped, $rva) + $delta) -band 0xFFFFFFFFL
                [Array]::Copy([BitConverter]::GetBytes([uint32]$value), 0, $mapped, $rva, 4)
            }
        }
        $reloc += $size
    }
    $linkMap = Get-Content -LiteralPath $ReleaseMapPath -Raw
    if ($linkMap -notmatch '\?ermBattleSpellInfluence\S+\s+([0-9a-fA-F]{8})\s') {
        throw 'Release linker map lacks the handler.'
    }
    $handler = [Convert]::ToInt64($Matches[1], 16) - $preferred
    $live = Read-LiveBytes ($module.BaseAddress.ToInt64() + $handler) 256
    for ($i = 0; $i -lt $live.Length; ++$i) {
        if ($live[$i] -ne $mapped[$handler + $i]) { throw 'Loaded handler differs from the deployed release.' }
    }
    Write-Output 'Loaded BM:G handler matches the deployed release after PE relocation.'
    $hook = Read-LiveBytes 0x75F334 5
    if ($hook[0] -ne 0xE9) { throw 'BM:G LoHook is not installed.' }
    $table = [BitConverter]::ToUInt32((Read-LiveBytes 0x687FA8 4), 0)
    foreach ($id in (@(71,73,75) + @(81..95))) {
        $record = Read-LiveBytes ($table + $id * 0x88) 0x88
        $name = [BitConverter]::ToUInt32($record, 0x10)
        $level = [BitConverter]::ToInt32($record, 0x18)
        if ($name -eq 0 -or (Read-LiveBytes $name 1)[0] -eq 0 -or $level -lt 1 -or $level -gt 5) {
            throw "Incomplete built-in spell record $id."
        }
    }
    # ERA writes Debug\Era\log.txt when the game closes, not while it runs.
    [BmgStartupRead]::CloseHandle($handle) | Out-Null
    $handle = [IntPtr]::Zero
    $process.CloseMainWindow() | Out-Null
    if (-not $process.WaitForExit(30000)) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    $startupLog = Get-Content -LiteralPath (Join-Path $GameDirectory 'Debug\Era\log.txt') -Raw
    if ($startupLog -notmatch 'Spell count (\d+)') { throw 'Startup log has no spell count.' }
    $count = [int]$Matches[1]
    if ($count -ne $ExpectedSpellCount) { throw "Unexpected spell count $count." }
    $log = Get-Content -LiteralPath (Join-Path $GameDirectory 'Debug\Era\log.txt') -Raw
    if ($log -match 'NewSpells: Native ERM support|BMG native probe') {
        throw 'Production startup reported an ERM profile failure or contains a test probe.'
    }
    Write-Output "PASS: correct core DLL loaded; $count spells; 18 built-in records valid; BM:G hook installed."
}
finally {
    if ($handle -ne [IntPtr]::Zero) { [BmgStartupRead]::CloseHandle($handle) | Out-Null }
    if ($process -and -not $process.HasExited) {
        $process.CloseMainWindow() | Out-Null
        if (-not $process.WaitForExit(5000)) { Stop-Process -Id $process.Id; $process.WaitForExit() }
    }
    $modListAfter = [IO.File]::ReadAllBytes($modListPath)
    if ([Convert]::ToBase64String($modListBefore) -ne [Convert]::ToBase64String($modListAfter)) {
        throw 'Game startup changed Mods/list.txt; review its crash-loop protection before continuing.'
    }
    Write-Output 'Mods/list.txt is byte-for-byte unchanged.'
}
