param(
  [string]$PacPath,
  [string]$GoldenTouchSpellIntPng,
  [string]$SpellBonDefPath,
  [string]$PngZipPath
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.IO.Compression.FileSystem

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $PacPath) {
  $PacPath = Join-Path $repoRoot 'dist\New Spells\Data\NewSpells.pac'
}
if (-not $GoldenTouchSpellIntPng) {
  $GoldenTouchSpellIntPng = Join-Path $repoRoot 'assets\golden-touch\SpellInt-96.png'
}
if (-not $SpellBonDefPath) {
  $SpellBonDefPath = Join-Path $repoRoot 'assets\core-defs\SpellBon.def'
}
if (-not $PngZipPath) {
  $PngZipPath = Join-Path $repoRoot 'dist\New Spells\Data\NewSpells_png_data.zip'
}

$PacPath = [IO.Path]::GetFullPath($PacPath)
$GoldenTouchSpellIntPng = [IO.Path]::GetFullPath($GoldenTouchSpellIntPng)
$SpellBonDefPath = [IO.Path]::GetFullPath($SpellBonDefPath)
$PngZipPath = [IO.Path]::GetFullPath($PngZipPath)

$MaxSupportedSpellId = 126

function Get-Int32 {
  param([byte[]]$Data, [int]$Offset)
  return [BitConverter]::ToInt32($Data, $Offset)
}

function Set-Int32 {
  param([byte[]]$Data, [int]$Offset, [int]$Value)
  $encoded = [BitConverter]::GetBytes($Value)
  [Array]::Copy($encoded, 0, $Data, $Offset, 4)
}

function Get-ByteSlice {
  param([byte[]]$Data, [int]$Offset, [int]$Length)
  $result = [byte[]]::new($Length)
  [Array]::Copy($Data, $Offset, $result, 0, $Length)
  return ,$result
}

function Expand-Zlib {
  param([byte[]]$Data)
  $sourceStream = [IO.MemoryStream]::new($Data)
  $zlibStream = [IO.Compression.ZLibStream]::new(
    $sourceStream,
    [IO.Compression.CompressionMode]::Decompress
  )
  $outputStream = [IO.MemoryStream]::new()
  try {
    $zlibStream.CopyTo($outputStream)
    return ,$outputStream.ToArray()
  }
  finally {
    $outputStream.Dispose()
    $zlibStream.Dispose()
    $sourceStream.Dispose()
  }
}

function Compress-Zlib {
  param([byte[]]$Data)
  $outputStream = [IO.MemoryStream]::new()
  $zlibStream = [IO.Compression.ZLibStream]::new(
    $outputStream,
    [IO.Compression.CompressionLevel]::Optimal,
    $true
  )
  try {
    $zlibStream.Write($Data, 0, $Data.Length)
  }
  finally {
    $zlibStream.Dispose()
  }

  try {
    return ,$outputStream.ToArray()
  }
  finally {
    $outputStream.Dispose()
  }
}

function Read-LodArchive {
  param([string]$Path)

  $data = [IO.File]::ReadAllBytes($Path)
  if ($data.Length -lt 92 -or [Text.Encoding]::ASCII.GetString($data, 0, 3) -ne 'LOD') {
    throw "Not a Heroes III LOD/PAC archive: $Path"
  }

  $entryCount = Get-Int32 $data 8
  if ($entryCount -lt 0 -or 92 + 32 * $entryCount -gt $data.Length) {
    throw "Invalid LOD/PAC directory: $Path"
  }

  $entries = @()
  for ($index = 0; $index -lt $entryCount; $index++) {
    $entryOffset = 92 + 32 * $index
    $name = [Text.Encoding]::ASCII.GetString($data, $entryOffset, 16).Trim([char]0)
    $offset = Get-Int32 $data ($entryOffset + 16)
    $fullSize = Get-Int32 $data ($entryOffset + 20)
    $type = Get-Int32 $data ($entryOffset + 24)
    $packedSize = Get-Int32 $data ($entryOffset + 28)
    $storedSize = if ($packedSize -gt 0) { $packedSize } else { $fullSize }
    if ($offset -lt 0 -or $storedSize -lt 0 -or $offset + $storedSize -gt $data.Length) {
      throw "Invalid PAC entry '$name' in $Path"
    }

    $entries += [PSCustomObject]@{
      Name = $name
      FullSize = $fullSize
      Type = $type
      PackedSize = $packedSize
      Blob = Get-ByteSlice $data $offset $storedSize
    }
  }

  return [PSCustomObject]@{
    Header = Get-ByteSlice $data 0 92
    Directory = Get-ByteSlice $data 92 ($entryCount * 32)
    Entries = $entries
  }
}

function Write-LodArchive {
  param($Archive, [string]$Path)

  $header = Get-ByteSlice $Archive.Header 0 $Archive.Header.Length
  Set-Int32 $header 8 $Archive.Entries.Count

  $directory = [byte[]]::new(32 * $Archive.Entries.Count)
  [Array]::Copy(
    $Archive.Directory,
    0,
    $directory,
    0,
    [Math]::Min($Archive.Directory.Length, $directory.Length)
  )
  $nextOffset = 92 + $directory.Length

  for ($index = 0; $index -lt $Archive.Entries.Count; $index++) {
    $entry = $Archive.Entries[$index]
    $entryOffset = 32 * $index
    $nameBytes = [Text.Encoding]::ASCII.GetBytes([string]$entry.Name)
    if ($nameBytes.Length -gt 15) {
      throw "PAC entry name is too long: $($entry.Name)"
    }

    [Array]::Clear($directory, $entryOffset, 16)
    [Array]::Copy($nameBytes, 0, $directory, $entryOffset, $nameBytes.Length)
    Set-Int32 $directory ($entryOffset + 16) $nextOffset
    Set-Int32 $directory ($entryOffset + 20) $entry.FullSize
    Set-Int32 $directory ($entryOffset + 24) $entry.Type
    Set-Int32 $directory ($entryOffset + 28) $entry.PackedSize
    $nextOffset += $entry.Blob.Length
  }

  $outputStream = [IO.MemoryStream]::new()
  try {
    $outputStream.Write($header, 0, $header.Length)
    $outputStream.Write($directory, 0, $directory.Length)
    foreach ($entry in $Archive.Entries) {
      $outputStream.Write($entry.Blob, 0, $entry.Blob.Length)
    }
    [IO.File]::WriteAllBytes($Path, $outputStream.ToArray())
  }
  finally {
    $outputStream.Dispose()
  }
}

function Get-DefFrame {
  param([byte[]]$DefData, [int]$FrameOffset)
  $dataSize = Get-Int32 $DefData $FrameOffset
  if ($dataSize -lt 0 -or $FrameOffset + 32 + $dataSize -gt $DefData.Length) {
    throw 'Invalid DEF frame range.'
  }
  return ,(Get-ByteSlice $DefData $FrameOffset (32 + $dataSize))
}

function Get-SingleGroupDefLayout {
  param([byte[]]$DefData, [string]$DefName)

  $type = Get-Int32 $DefData 0
  $width = Get-Int32 $DefData 4
  $height = Get-Int32 $DefData 8
  $groupCount = Get-Int32 $DefData 12
  if ($type -ne 71 -or $groupCount -ne 1) {
    throw "$DefName is not the expected single-group sprite DEF."
  }

  $groupOffset = 16 + 256 * 3
  $groupId = Get-Int32 $DefData $groupOffset
  $frameCount = Get-Int32 $DefData ($groupOffset + 4)
  if ($groupId -ne 0 -or $frameCount -le 0) {
    throw "$DefName has an unexpected group layout."
  }

  $nameOffset = $groupOffset + 16
  $offsetTable = $nameOffset + 13 * $frameCount
  $names = @()
  $frames = @()
  for ($index = 0; $index -lt $frameCount; $index++) {
    $names += [Text.Encoding]::ASCII.GetString(
      $DefData,
      $nameOffset + 13 * $index,
      13
    ).Trim([char]0)
    $frameOffset = Get-Int32 $DefData ($offsetTable + 4 * $index)
    $frames += ,(Get-DefFrame $DefData $frameOffset)
  }

  return [PSCustomObject]@{
    Width = $width
    Height = $height
    GroupOffset = $groupOffset
    Names = $names
    Frames = $frames
  }
}

function Build-SingleGroupDef {
  param(
    [byte[]]$OriginalDef,
    $Layout,
    [string[]]$Names,
    [object[]]$Frames
  )

  if ($Names.Count -ne $Frames.Count) {
    throw 'DEF name/frame counts differ.'
  }

  $prefixLength = $Layout.GroupOffset + 16
  $prefix = Get-ByteSlice $OriginalDef 0 $prefixLength
  Set-Int32 $prefix ($Layout.GroupOffset + 4) $Frames.Count

  $nameData = [byte[]]::new(13 * $Frames.Count)
  for ($index = 0; $index -lt $Names.Count; $index++) {
    $nameBytes = [Text.Encoding]::ASCII.GetBytes($Names[$index])
    if ($nameBytes.Length -gt 13) {
      throw "DEF frame name is too long: $($Names[$index])"
    }
    [Array]::Copy($nameBytes, 0, $nameData, 13 * $index, $nameBytes.Length)
  }

  $offsetData = [byte[]]::new(4 * $Frames.Count)
  $nextFrameOffset = $prefix.Length + $nameData.Length + $offsetData.Length
  for ($index = 0; $index -lt $Frames.Count; $index++) {
    Set-Int32 $offsetData (4 * $index) $nextFrameOffset
    $nextFrameOffset += $Frames[$index].Length
  }

  $outputStream = [IO.MemoryStream]::new()
  try {
    $outputStream.Write($prefix, 0, $prefix.Length)
    $outputStream.Write($nameData, 0, $nameData.Length)
    $outputStream.Write($offsetData, 0, $offsetData.Length)
    foreach ($frame in $Frames) {
      $outputStream.Write($frame, 0, $frame.Length)
    }
    return ,$outputStream.ToArray()
  }
  finally {
    $outputStream.Dispose()
  }
}

function Get-PaletteColors {
  param([byte[]]$DefData)
  $colors = [Drawing.Color[]]::new(256)
  for ($index = 0; $index -lt 256; $index++) {
    $offset = 16 + 3 * $index
    $colors[$index] = [Drawing.Color]::FromArgb(
      255,
      $DefData[$offset],
      $DefData[$offset + 1],
      $DefData[$offset + 2]
    )
  }
  return $colors
}

function Find-NearestPaletteIndex {
  param(
    [Drawing.Color]$Color,
    [Drawing.Color[]]$Palette,
    [hashtable]$Cache
  )

  $key = ($Color.R -shl 16) -bor ($Color.G -shl 8) -bor $Color.B
  if ($Cache.ContainsKey($key)) {
    return [int]$Cache[$key]
  }

  $bestIndex = 8
  $bestDistance = [long]::MaxValue
  for ($index = 8; $index -lt $Palette.Length; $index++) {
    $red = [int]$Color.R - [int]$Palette[$index].R
    $green = [int]$Color.G - [int]$Palette[$index].G
    $blue = [int]$Color.B - [int]$Palette[$index].B
    $distance = 3L * $red * $red + 6L * $green * $green + 2L * $blue * $blue
    if ($distance -lt $bestDistance) {
      $bestDistance = $distance
      $bestIndex = $index
    }
  }

  $Cache[$key] = $bestIndex
  return $bestIndex
}

function New-LiteralDefFrame {
  param([byte[]]$Indices, [int]$Width, [int]$Height)

  $rowTableSize = 4 * $Height
  $encodedRowSize = 2 + $Width
  $dataSize = $rowTableSize + $Height * $encodedRowSize
  $frame = [byte[]]::new(32 + $dataSize)

  Set-Int32 $frame 0 $dataSize
  Set-Int32 $frame 4 1
  Set-Int32 $frame 8 $Width
  Set-Int32 $frame 12 $Height
  Set-Int32 $frame 16 $Width
  Set-Int32 $frame 20 $Height
  Set-Int32 $frame 24 0
  Set-Int32 $frame 28 0

  for ($row = 0; $row -lt $Height; $row++) {
    $rowOffset = $rowTableSize + $row * $encodedRowSize
    Set-Int32 $frame (32 + 4 * $row) $rowOffset
    $cursor = 32 + $rowOffset
    $frame[$cursor] = 255
    $frame[$cursor + 1] = [byte]($Width - 1)
    [Array]::Copy($Indices, $row * $Width, $frame, $cursor + 2, $Width)
  }

  return ,$frame
}

function New-PngDefFrame {
  param(
    [string]$PngPath,
    [byte[]]$DefData,
    [int]$ExpectedWidth,
    [int]$ExpectedHeight
  )

  $bitmap = [Drawing.Bitmap]::new($PngPath)
  try {
    if ($bitmap.Width -ne $ExpectedWidth -or $bitmap.Height -ne $ExpectedHeight) {
      throw "Expected ${ExpectedWidth}x${ExpectedHeight} PNG: $PngPath"
    }

    $palette = Get-PaletteColors $DefData
    $cache = @{}
    $indices = [byte[]]::new($bitmap.Width * $bitmap.Height)
    for ($y = 0; $y -lt $bitmap.Height; $y++) {
      for ($x = 0; $x -lt $bitmap.Width; $x++) {
        $color = $bitmap.GetPixel($x, $y)
        $indices[$y * $bitmap.Width + $x] = if ($color.A -lt 128) {
          0
        }
        else {
          [byte](Find-NearestPaletteIndex $color $palette $cache)
        }
      }
    }

    return ,(New-LiteralDefFrame $indices $bitmap.Width $bitmap.Height)
  }
  finally {
    $bitmap.Dispose()
  }
}

function New-CarrierDefFrame {
  param([int]$FullWidth, [int]$FullHeight)

  # ERA substitutes the true-color PNG while retaining this advertised canvas.
  $frame = [byte[]]::new(33)
  Set-Int32 $frame 0 1
  Set-Int32 $frame 4 0
  Set-Int32 $frame 8 $FullWidth
  Set-Int32 $frame 12 $FullHeight
  Set-Int32 $frame 16 1
  Set-Int32 $frame 20 1
  Set-Int32 $frame 24 0
  Set-Int32 $frame 28 0
  $frame[32] = 0
  return ,$frame
}

function Get-ReserveFrameName {
  param([string]$DefName, [int]$SpellId)
  switch ($DefName.ToLowerInvariant()) {
    'spells.def' { return "Sp${SpellId}Res.1u2" }
    'spellbon.def' { return "Sb${SpellId}Res.bon" }
    'spellint.def' { return "Si${SpellId}Res.tud" }
    'spellscr.def' { return "Sm${SpellId}Res.b2g" }
    default { throw "Unsupported DEF: $DefName" }
  }
}

function Write-PngZip {
  param([string]$Path)

  $parent = Split-Path -Parent $Path
  [IO.Directory]::CreateDirectory($parent) | Out-Null
  $tempPath = "$Path.tmp"
  if (Test-Path -LiteralPath $tempPath) {
    Remove-Item -LiteralPath $tempPath -Force
  }

  $items = @(
    [PSCustomObject]@{
      Name = 'Data/Defs/SpellInt.def/0_96.png'
      Source = $GoldenTouchSpellIntPng
    }
  )

  $fileStream = [IO.FileStream]::new(
    $tempPath,
    [IO.FileMode]::CreateNew,
    [IO.FileAccess]::ReadWrite,
    [IO.FileShare]::None
  )
  $zip = [IO.Compression.ZipArchive]::new(
    $fileStream,
    [IO.Compression.ZipArchiveMode]::Create,
    $false
  )
  try {
    foreach ($item in $items) {
      if (-not (Test-Path -LiteralPath $item.Source)) {
        throw "Missing PNG source: $($item.Source)"
      }
      $entry = $zip.CreateEntry($item.Name, [IO.Compression.CompressionLevel]::NoCompression)
      $entryStream = $entry.Open()
      $sourceStream = [IO.File]::OpenRead($item.Source)
      try {
        $sourceStream.CopyTo($entryStream)
      }
      finally {
        $sourceStream.Dispose()
        $entryStream.Dispose()
      }
    }
  }
  finally {
    $zip.Dispose()
    $fileStream.Dispose()
  }

  [IO.File]::Copy($tempPath, $Path, $true)
  Remove-Item -LiteralPath $tempPath -Force
}

foreach ($requiredPath in @($PacPath, $GoldenTouchSpellIntPng, $SpellBonDefPath)) {
  if (-not (Test-Path -LiteralPath $requiredPath)) {
    throw "Required graphics input not found: $requiredPath"
  }
}

$archive = Read-LodArchive $PacPath
$spellBonEntry = $archive.Entries |
  Where-Object Name -IEq 'SpellBon.def' |
  Select-Object -First 1
if ($null -eq $spellBonEntry) {
  $spellBonData = [IO.File]::ReadAllBytes($SpellBonDefPath)
  $spellBonLayout = Get-SingleGroupDefLayout $spellBonData 'SpellBon.def'
  if ($spellBonLayout.Width -ne 58 -or
      $spellBonLayout.Height -ne 64 -or
      $spellBonLayout.Frames.Count -ne 70) {
    throw 'The baseline SpellBon.def must be the stock 58x64, 70-frame DEF.'
  }

  $compressedSpellBon = Compress-Zlib $spellBonData
  $archive.Entries += [PSCustomObject]@{
    Name = 'SpellBon.def'
    FullSize = $spellBonData.Length
    Type = 71
    PackedSize = $compressedSpellBon.Length
    Blob = $compressedSpellBon
  }
}

$expectedCounts = @{
  'spells.def' = $MaxSupportedSpellId + 1
  'SpellBon.def' = $MaxSupportedSpellId + 1
  # SpellInt has one leading frame-index offset for the appended spell range.
  'SpellInt.def' = $MaxSupportedSpellId + 2
  # SpellScr keeps its final All Spells frame after every spell-ID frame.
  'SpellScr.def' = $MaxSupportedSpellId + 2
}

foreach ($defName in @('spells.def', 'SpellBon.def', 'SpellInt.def', 'SpellScr.def')) {
  $entry = $archive.Entries |
    Where-Object Name -IEq $defName |
    Select-Object -First 1
  if ($null -eq $entry) {
    throw "$defName is missing from $PacPath"
  }

  $defData = if ($entry.PackedSize -gt 0) {
    Expand-Zlib $entry.Blob
  }
  else {
    $entry.Blob
  }
  $layout = Get-SingleGroupDefLayout $defData $defName
  $names = @($layout.Names)
  $frames = @($layout.Frames)
  $carrier = New-CarrierDefFrame $layout.Width $layout.Height

  switch ($defName) {
    'spells.def' {
      if ($frames.Count -eq 96) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName 96
      }
      elseif ($frames.Count -eq 114 -or
              $frames.Count -eq $expectedCounts[$defName]) {
        $frames[96] = $carrier
        $names[96] = Get-ReserveFrameName $defName 96
      }
      else {
        throw "Expected 96, 114, or $($expectedCounts[$defName]) frames in $defName, found $($frames.Count)."
      }

      for ($spellId = $frames.Count; $spellId -le $MaxSupportedSpellId; $spellId++) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName $spellId
      }
    }

    'SpellBon.def' {
      if ($frames.Count -ne 70 -and
          $frames.Count -ne $expectedCounts[$defName]) {
        throw "Expected 70 or $($expectedCounts[$defName]) frames in $defName, found $($frames.Count)."
      }

      for ($spellId = $frames.Count; $spellId -le $MaxSupportedSpellId; $spellId++) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName $spellId
      }

      # External slots remain transparent in the core package. Provider PNG
      # overrides supply visible campaign-bonus artwork for active spells.
      for ($spellId = 96; $spellId -le $MaxSupportedSpellId; $spellId++) {
        $frames[$spellId] = $carrier
        $names[$spellId] = Get-ReserveFrameName $defName $spellId
      }
    }

    'SpellInt.def' {
      if ($frames.Count -ne 97 -and
          $frames.Count -ne 115 -and
          $frames.Count -ne $expectedCounts[$defName]) {
        throw "Expected 97, 115, or $($expectedCounts[$defName]) frames in $defName, found $($frames.Count)."
      }
      $frames[96] = New-PngDefFrame $GoldenTouchSpellIntPng $defData 48 36
      $names[96] = 'Si95Gold.tud'
      if ($frames.Count -eq 97) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName 96
      }
      else {
        $frames[97] = $carrier
        $names[97] = Get-ReserveFrameName $defName 96
      }

      # Frame index is spell ID + 1 in the appended SpellInt range.
      for ($spellId = $frames.Count - 1; $spellId -le $MaxSupportedSpellId; $spellId++) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName $spellId
      }
    }

    'SpellScr.def' {
      if ($frames.Count -ne 97 -and
          $frames.Count -ne 115 -and
          $frames.Count -ne $expectedCounts[$defName]) {
        throw "Expected 97, 115, or $($expectedCounts[$defName]) frames in $defName, found $($frames.Count)."
      }

      $allSpellsFrame = $frames[-1]
      $allSpellsName = $names[-1]
      $frames = @($frames[0..($frames.Count - 2)])
      $names = @($names[0..($names.Count - 2)])

      if ($frames.Count -eq 96) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName 96
      }
      else {
        $frames[96] = $carrier
        $names[96] = Get-ReserveFrameName $defName 96
      }

      for ($spellId = $frames.Count; $spellId -le $MaxSupportedSpellId; $spellId++) {
        $frames += ,$carrier
        $names += Get-ReserveFrameName $defName $spellId
      }

      $frames += ,$allSpellsFrame
      $names += $allSpellsName
    }
  }

  if ($frames.Count -ne $expectedCounts[$defName]) {
    throw "$defName rebuild produced $($frames.Count) frames."
  }

  $rebuiltDef = Build-SingleGroupDef $defData $layout $names $frames
  $compressedDef = Compress-Zlib $rebuiltDef
  $entry.FullSize = $rebuiltDef.Length
  $entry.PackedSize = $compressedDef.Length
  $entry.Blob = $compressedDef
}

$pacTempPath = "$PacPath.tmp"
if (Test-Path -LiteralPath $pacTempPath) {
  Remove-Item -LiteralPath $pacTempPath -Force
}
Write-LodArchive $archive $pacTempPath

$verificationArchive = Read-LodArchive $pacTempPath
$verificationResults = @()
foreach ($defName in @('spells.def', 'SpellBon.def', 'SpellInt.def', 'SpellScr.def')) {
  $entry = $verificationArchive.Entries |
    Where-Object Name -IEq $defName |
    Select-Object -First 1
  $defData = Expand-Zlib $entry.Blob
  $layout = Get-SingleGroupDefLayout $defData $defName
  if ($layout.Frames.Count -ne $expectedCounts[$defName]) {
    throw "$defName verification found $($layout.Frames.Count) frames."
  }

  $carrierIndex = if ($defName -eq 'SpellInt.def') { 97 } else { 96 }
  $carrierFrame = $layout.Frames[$carrierIndex]
  if ($carrierFrame.Length -ne 33 -or
      (Get-Int32 $carrierFrame 0) -ne 1 -or
      (Get-Int32 $carrierFrame 4) -ne 0 -or
      (Get-Int32 $carrierFrame 8) -ne $layout.Width -or
      (Get-Int32 $carrierFrame 12) -ne $layout.Height -or
      (Get-Int32 $carrierFrame 16) -ne 1 -or
      (Get-Int32 $carrierFrame 20) -ne 1 -or
      $carrierFrame[32] -ne 0 -or
      $layout.Names[$carrierIndex] -ne (Get-ReserveFrameName $defName 96)) {
    throw "$defName does not contain the expected transparent ID-96 carrier."
  }

  if ($defName -eq 'SpellBon.def') {
    for ($spellId = 96; $spellId -le $MaxSupportedSpellId; $spellId++) {
      $reservedFrame = $layout.Frames[$spellId]
      if ($reservedFrame.Length -ne 33 -or
          (Get-Int32 $reservedFrame 4) -ne 0 -or
          $reservedFrame[32] -ne 0 -or
          $layout.Names[$spellId] -ne (Get-ReserveFrameName $defName $spellId)) {
        throw "SpellBon.def frame $spellId is not the expected transparent carrier."
      }
    }
  }

  $verificationResults += [PSCustomObject]@{
    Def = $defName
    Frames = $layout.Frames.Count
    FullBytes = $entry.FullSize
    PackedBytes = $entry.PackedSize
  }
}

[IO.File]::Copy($pacTempPath, $PacPath, $true)
Remove-Item -LiteralPath $pacTempPath -Force
Write-PngZip $PngZipPath

$zip = [IO.Compression.ZipFile]::OpenRead($PngZipPath)
try {
  if ($zip.Entries.Count -ne 1) {
    throw "Expected one PNG override, found $($zip.Entries.Count)."
  }
}
finally {
  $zip.Dispose()
}

$verificationResults
[PSCustomObject]@{
  Pac = $PacPath
  PngZip = $PngZipPath
  ExtendedDefs = 4
  ReservedSpellFrames = 31
  HighestReservedSpellId = $MaxSupportedSpellId
  GoldenTouchSpellIntFrame = 96
}
