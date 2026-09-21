param(
    [ValidateSet('CoreOnly', 'Expansion')]
    [string] $Mode = 'Expansion',
    [int] $StartupSeconds = 15,
    [switch] $AuditAnimationCatalog,
    [switch] $RequireNativeProbe,
    [string] $GameDirectory = 'D:\Games\Heroes 3 ERA'
)

$ErrorActionPreference = 'Stop'

$gameExecutable = Join-Path $gameDirectory 'h3era.exe'
$modListPath = Join-Path $gameDirectory 'Mods\list.txt'
$process = $null
$processHandle = [IntPtr]::Zero
$originalModListBytes = $null
$modListChanged = $false

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class NewSpellsStartupProbe
{
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inheritHandle, int processId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadProcessMemory(IntPtr process, IntPtr address,
        byte[] buffer, UIntPtr size, out UIntPtr bytesRead);

    [DllImport("kernel32.dll")]
    public static extern bool CloseHandle(IntPtr handle);

    [DllImport("kernel32.dll")]
    public static extern uint GetACP();
}
'@

function Read-ProcessBytes {
    param(
        [Parameter(Mandatory)] [IntPtr] $Handle,
        [Parameter(Mandatory)] [Int64] $Address,
        [Parameter(Mandatory)] [int] $Count
    )

    $buffer = [byte[]]::new($Count)
    $bytesRead = [UIntPtr]::Zero
    $okay = [NewSpellsStartupProbe]::ReadProcessMemory(
        $Handle, [IntPtr]$Address, $buffer, [UIntPtr]$Count, [ref]$bytesRead)
    if (-not $okay -or $bytesRead.ToUInt64() -ne [uint64]$Count) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "ReadProcessMemory failed at 0x$($Address.ToString('X')): $errorCode."
    }
    return ,$buffer
}

function Read-ProcessCString {
    param(
        [Parameter(Mandatory)] [IntPtr] $Handle,
        [Parameter(Mandatory)] [UInt32] $Address,
        [int] $MaximumLength = 256
    )

    if ($Address -eq 0) {
        return $null
    }

    $buffer = Read-ProcessBytes $Handle $Address $MaximumLength
    $length = [Array]::IndexOf($buffer, [byte]0)
    if ($length -lt 0) {
        $length = $buffer.Length
    }
    return [Text.Encoding]::GetEncoding([int][NewSpellsStartupProbe]::GetACP()).GetString($buffer, 0, $length)
}

try {
    if ($Mode -eq 'CoreOnly') {
        $originalModListBytes = [IO.File]::ReadAllBytes($modListPath)
        $utf8 = [Text.UTF8Encoding]::new($false, $true)
        $originalModListText = $utf8.GetString($originalModListBytes)
        $disabledModListText = [Text.RegularExpressions.Regex]::Replace(
            $originalModListText,
            '(?m)^New Spells Expansion(?=\r?$)',
            '*New Spells Expansion')
        if ($disabledModListText -eq $originalModListText) {
            throw 'Could not find one active New Spells Expansion entry in Mods/list.txt.'
        }
        [IO.File]::WriteAllText($modListPath, $disabledModListText, $utf8)
        $modListChanged = $true
    }

    $process = Start-Process -FilePath $gameExecutable `
        -WorkingDirectory $gameDirectory -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds $StartupSeconds
    $process.Refresh()
    if ($process.HasExited) {
        throw "Game exited early with code $($process.ExitCode)."
    }

    # PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
    $processHandle = [NewSpellsStartupProbe]::OpenProcess(
        0x410, $false, $process.Id)
    if ($processHandle -eq [IntPtr]::Zero) {
        $errorCode = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
        throw "OpenProcess failed: $errorCode."
    }

    $spellCount = [int](Read-ProcessBytes $processHandle 0x402902 1)[0]
    $gameManager = [BitConverter]::ToUInt32(
        (Read-ProcessBytes $processHandle 0x699538 4), 0)
    $spellTable = [BitConverter]::ToUInt32(
        (Read-ProcessBytes $processHandle 0x687FA8 4), 0)
    if ($gameManager -eq 0 -or $spellTable -eq 0) {
        throw 'The live game manager or spell-table pointer is null.'
    }

    $disabled = Read-ProcessBytes $processHandle ($gameManager + 4) $spellCount
    if ($AuditAnimationCatalog) {
        $operands = @(0x43F77E,0x43FB6A,0x4963FB,0x4965CF,0x5A5036,0x5A6B14,0x5A7A74,0x5A962C)
        $tableAddress = [BitConverter]::ToUInt32((Read-ProcessBytes $processHandle $operands[0] 4),0)
        foreach ($operand in $operands) {
            if ([BitConverter]::ToUInt32((Read-ProcessBytes $processHandle $operand 4),0) -ne $tableAddress) { throw 'Animation operands disagree.' }
        }
        $table = Read-ProcessBytes $processHandle $tableAddress (3000 * 12)
        for ($index = 1000; $index -lt 3000; ++$index) {
            $def = Read-ProcessCString $processHandle ([BitConverter]::ToUInt32($table, $index * 12)) 16
            $name = Read-ProcessCString $processHandle ([BitConverter]::ToUInt32($table, $index * 12 + 4)) 16
            $type = [BitConverter]::ToInt32($table, $index * 12 + 8)
            if ($def -ne "anim$index.def" -or $name -ne "anim$index" -or $type -ne $(if ($index -ge 2000) {257} else {1})) { throw "Unexpected animation record $index." }
        }
        [IO.File]::WriteAllBytes((Join-Path $PSScriptRoot '..\work\blizzard-more-anims-baseline.bin'), $table)
        Write-Host 'Read-only catalog audit passed: all 8 operands share a readable 3000-record table; all 2000 reserved records match the source contract. No process memory was written.'
        return
    }
    $builtInSpellIds = @(71, 73, 75) + @(81..95)
    $builtInRecords = foreach ($spellId in $builtInSpellIds) {
        $record = Read-ProcessBytes $processHandle ($spellTable + $spellId * 0x88) 0x88
        $namePointer = [BitConverter]::ToUInt32($record, 0x10)
        $name = Read-ProcessCString $processHandle $namePointer
        $level = [BitConverter]::ToInt32($record, 0x18)
        $flags = [BitConverter]::ToUInt32($record, 0x0C)
        [pscustomobject]@{
            Id = $spellId
            Name = $name
            Level = $level
            Flags = ('0x{0:X8}' -f $flags)
            ValidHeroRecord = -not [string]::IsNullOrEmpty($name) -and
                $level -ge 1 -and $level -le 5 -and ($flags -band 8) -eq 0
        }
    }
    $spell93 = Read-ProcessBytes $processHandle ($spellTable + 93 * 0x88) 0x88
    $spell93Flags = [BitConverter]::ToUInt32($spell93, 0x0C)
    $spell93Name = [BitConverter]::ToUInt32($spell93, 0x10)
    $spell93Level = [BitConverter]::ToInt32($spell93, 0x18)
    $spell96 = Read-ProcessBytes $processHandle ($spellTable + 96 * 0x88) 0x88
    $spell96Name = [BitConverter]::ToUInt32($spell96, 0x10)
    $spell96Level = [BitConverter]::ToInt32($spell96, 0x18)

    $spell97 = Read-ProcessBytes $processHandle ($spellTable + 97 * 0x88) 0x88
    $name97 = Read-ProcessCString $processHandle ([BitConverter]::ToUInt32($spell97, 0x10))
    $level97 = [BitConverter]::ToInt32($spell97, 0x18)
    $animation97 = [BitConverter]::ToInt32($spell97, 8)
    Write-Host "Blizzard: ID 97, name=$name97, level=$level97, animation=$animation97, disabled=$($disabled[97])"
    $spell64 = Read-ProcessBytes $processHandle ($spellTable + 64 * 0x88) 0x88
    $name64 = Read-ProcessCString $processHandle ([BitConverter]::ToUInt32($spell64, 0x10))
    Write-Host "Remove Obstacle: ID 64, name=$name64, level=$([BitConverter]::ToInt32($spell64,0x18))"
    if ($Mode -eq 'Expansion') {
        if ($level97 -ne 5 -or [string]::IsNullOrEmpty($name97) -or $disabled[97] -ne 0 -or $animation97 -le 82) {
            throw 'Blizzard failed to activate with a relocated animation.'
        }
        if ([BitConverter]::ToInt32($spell64,0x18) -ne 2 -or [BitConverter]::ToInt32($spell64,8) -ne 34) {
            throw 'Remove Obstacle was modified by the Blizzard port.'
        }
        $animationTable = [BitConverter]::ToUInt32((Read-ProcessBytes $processHandle 0x43F77E 4),0)
        $animationRecord = Read-ProcessBytes $processHandle ($animationTable + 12 * $animation97) 12
        if ((Read-ProcessCString $processHandle ([BitConverter]::ToUInt32($animationRecord,0))) -ine 'NSEBLIZ.def' -or
            (Read-ProcessCString $processHandle ([BitConverter]::ToUInt32($animationRecord,4))) -ne 'HD.Plugin.H3.NewSpellsExpansion.Blizzard.Area' -or
            [BitConverter]::ToInt32($animationRecord,8) -ne 1) { throw 'Blizzard animation binding is incorrect.' }
    }

    $coreModule = $process.Modules |
        Where-Object ModuleName -IEq 'NewSpells.dll' |
        Select-Object -First 1
    $expansionModule = $process.Modules |
        Where-Object ModuleName -IEq 'NewSpellsExpansion.dll' |
        Select-Object -First 1
    if (-not $coreModule) {
        throw 'NewSpells.dll is not loaded.'
    }

    [pscustomobject]@{
        Mode = $Mode
        Pid = $process.Id
        NewSpellsModule = $coreModule.FileName
        ExpansionModule = if ($expansionModule) { $expansionModule.FileName } else { $null }
        LiveSpellCount = $spellCount
        LiveLastSpellId = $spellCount - 1
        Spell93NamePointer = ('0x{0:X8}' -f $spell93Name)
        Spell93Name = Read-ProcessCString $processHandle $spell93Name
        Spell93Level = $spell93Level
        Spell93Flags = ('0x{0:X8}' -f $spell93Flags)
        Spell93DisabledFlag = if ($spellCount -gt 93) { $disabled[93] } else { $null }
        Spell96NamePointer = ('0x{0:X8}' -f $spell96Name)
        Spell96Level = $spell96Level
        Spell96DisabledFlag = if ($spellCount -gt 96) { $disabled[96] } else { $null }
    } | Format-List
    $builtInRecords | Format-Table -AutoSize

    if (@($builtInRecords | Where-Object { -not $_.ValidHeroRecord }).Count -ne 0) {
        throw 'At least one of the 18 core spell records is incomplete.'
    }

    if ($Mode -eq 'Expansion') {
        if (-not $expansionModule) {
            throw 'NewSpellsExpansion.dll is not loaded.'
        }
        if ($spellCount -ne 98 -or $spell96Name -eq 0 -or
            $spell96Level -ne 3 -or $disabled[96] -ne 0) {
            throw 'Expansion mode did not produce one active level-3 spell at ID 96.'
        }
    }
    else {
        if ($expansionModule) {
            throw 'Core-only mode unexpectedly loaded NewSpellsExpansion.dll.'
        }
        if ($spellCount -ne 96 -or $spell96Name -ne 0 -or $spell96Level -ne 0) {
            throw 'Core-only mode did not leave ID 96 blank outside the live boundary.'
        }
    }
    if ($RequireNativeProbe) {
        $nativeLog = Get-Content -LiteralPath (Join-Path $gameDirectory 'Debug\Era\log.txt') -Raw
        if ($nativeLog -notmatch 'Blizzard native probe: Result\s+PASS: (\d+) checks' -or $nativeLog -match 'FAIL:|FAIL at line|Native exception') {
            throw 'Native Blizzard regression probe did not pass.'
        }
        Write-Host "Native Blizzard regression probe passed."
    }
}
finally {
    try {
        if ($processHandle -ne [IntPtr]::Zero) {
            [NewSpellsStartupProbe]::CloseHandle($processHandle) | Out-Null
        }
        if ($process -and -not $process.HasExited) {
            $process.CloseMainWindow() | Out-Null
            if (-not $process.WaitForExit(8000)) {
                Stop-Process -Id $process.Id
                $process.WaitForExit(5000) | Out-Null
            }
        }
    }
    finally {
        if ($modListChanged) {
            [IO.File]::WriteAllBytes($modListPath, $originalModListBytes)
        }
    }
}
