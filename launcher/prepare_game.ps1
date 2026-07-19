param(
  [switch]$PrepareOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Root = [System.IO.Path]::GetFullPath((Split-Path -Parent $MyInvocation.MyCommand.Path))
$GameData = Join-Path $Root 'game_data'
$UserData = Join-Path $Root 'user_data'
$Extracting = Join-Path $Root 'game_data.extracting'
$Extractor = Join-Path $Root 'tools\extract-xiso\extract-xiso.exe'
$Executable = Join-Path $Root 'saidaioujou_recomp_tu1.exe'
$Redist = Join-Path $Root 'prerequisites\VC_redist.x64.exe'

$ExpectedTitleUpdateHash = '0A893048C761BA6CF8ED14637F326726BC616B5183CA1EE52DE02FC12B46B215'

$BaseFiles = @(
  @{ Name = 'default.xex';  Hash = '709EB9FD2E4446E5754461616693B3BBB8B78FEF5AA613FFCDE6D643DA0133F5' },
  @{ Name = 'CA022100.bin'; Hash = 'A8454BB24A0C0F0A0E5C89C8885041DEE6411E2C688874A5E2A57966174939BE' },
  @{ Name = 'CA022110.bin'; Hash = '4C08B646140E4887AC2036208195E99DFDB4AE32FC7D1571D589B6657791BC75' }
)

$PatchFiles = @(
  @{ Name = 'CA022100.binp'; Offset = 53248;  Length = 126976; Hash = '830EE751922483115F735F442A23B0AB203468DE17E509DA6ED08B25BE25DC4F' },
  @{ Name = 'CA022110.binp'; Offset = 180224; Length = 118784; Hash = 'B0FD71B07A0DFAE8E834A08A2EF7BD064B44492A7B50854B2C9245AB48266C00' },
  @{ Name = 'default.xexp';  Offset = 299008; Length = 299008; Hash = '79C6958470749931040BA2B68AED268A64CDAEED03F788EEAFFDABD45BA7EBBB' }
)

function Get-FileSha256([string]$Path) {
  return (Get-FileHash -Algorithm SHA256 -LiteralPath $Path).Hash.ToUpperInvariant()
}

function Get-BytesSha256([byte[]]$Bytes) {
  $sha = [System.Security.Cryptography.SHA256]::Create()
  try {
    return ([System.BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-', '')
  }
  finally {
    $sha.Dispose()
  }
}

function Test-ExpectedFiles([string]$Directory, [object[]]$Entries) {
  foreach ($entry in $Entries) {
    $path = Join-Path $Directory $entry.Name
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
      return $false
    }
    if ((Get-FileSha256 $path) -ne $entry.Hash) {
      return $false
    }
  }
  return $true
}

function Remove-ExtractionTemporaryDirectory {
  if (-not (Test-Path -LiteralPath $Extracting)) {
    return
  }
  $expected = [System.IO.Path]::GetFullPath((Join-Path $Root 'game_data.extracting'))
  $actual = [System.IO.Path]::GetFullPath($Extracting)
  if ($actual -ne $expected -or -not $actual.StartsWith($Root + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Won't remove unexpected path: $actual"
  }
  Remove-Item -LiteralPath $actual -Recurse -Force
}

try {
  Write-Host 'SDOJ recomp setup' -ForegroundColor Cyan
  Write-Host ''

  if (-not (Test-Path -LiteralPath $Extractor -PathType Leaf)) {
    throw 'extract-xiso.exe is missing. Re-extract the release and try again.'
  }
  if (-not (Test-Path -LiteralPath $Executable -PathType Leaf)) {
    throw 'The recomp EXE is missing. Re-extract the release and try again.'
  }

  $baseReady = Test-ExpectedFiles $GameData $BaseFiles
  if ($baseReady) {
    Write-Host 'game_data looks good. Skipping ISO extraction.' -ForegroundColor Green
  }
  else {
    if (Test-Path -LiteralPath $GameData) {
      $existingItems = @(Get-ChildItem -LiteralPath $GameData -Force -ErrorAction SilentlyContinue)
      if ($existingItems.Count -ne 0) {
        throw 'game_data is incomplete or from the wrong ISO. Delete it and try again.'
      }
      Remove-Item -LiteralPath $GameData -Force
    }

    $isoFiles = @(Get-ChildItem -LiteralPath $Root -File -Filter '*.iso')
    if ($isoFiles.Count -eq 0) {
      throw 'No ISO found. Put your SDOJ ISO next to launch.cmd and try again.'
    }
    if ($isoFiles.Count -gt 1) {
      throw 'Found more than one ISO. Leave only the SDOJ ISO here and try again.'
    }

    Remove-ExtractionTemporaryDirectory
    New-Item -ItemType Directory -Path $Extracting | Out-Null

    Write-Host "Extracting $($isoFiles[0].Name). This only happens on the first launch..." -ForegroundColor Yellow
    & $Extractor '-q' '-x' '-s' '-d' $Extracting $isoFiles[0].FullName
    if ($LASTEXITCODE -ne 0) {
      throw "ISO extraction failed (error $LASTEXITCODE)."
    }
    if (-not (Test-ExpectedFiles $Extracting $BaseFiles)) {
      throw "That ISO doesn't match the supported SDOJ dump."
    }

    Move-Item -LiteralPath $Extracting -Destination $GameData
    $baseReady = $true
    Write-Host 'ISO extracted and verified.' -ForegroundColor Green
  }

  $patchesReady = Test-ExpectedFiles $GameData $PatchFiles
  if ($patchesReady) {
    Write-Host 'TU1 is already set up.' -ForegroundColor Green
  }
  else {
    $tuCandidates = @(Get-ChildItem -LiteralPath $Root -File | Where-Object {
      $_.Name -like 'TU_*' -or $_.Length -eq 598016
    })
    $titleUpdate = $null
    foreach ($candidate in $tuCandidates) {
      if ((Get-FileSha256 $candidate.FullName) -eq $ExpectedTitleUpdateHash) {
        $titleUpdate = $candidate
        break
      }
    }
    if ($null -eq $titleUpdate) {
      throw 'TU1 file not found. Put the TU_11LK1V7... file next to launch.cmd and try again.'
    }

    Write-Host "Installing TU1 from $($titleUpdate.Name)..." -ForegroundColor Yellow
    [byte[]]$titleUpdateBytes = [System.IO.File]::ReadAllBytes($titleUpdate.FullName)
    if ($titleUpdateBytes.Length -ne 598016) {
      throw 'That TU1 file is the wrong size.'
    }

    foreach ($entry in $PatchFiles) {
      [byte[]]$payload = New-Object byte[] ([int]$entry.Length)
      [System.Array]::Copy($titleUpdateBytes, [int]$entry.Offset, $payload, 0, [int]$entry.Length)
      if ((Get-BytesSha256 $payload) -ne $entry.Hash) {
        throw "TU1 check failed for $($entry.Name)."
      }
      [System.IO.File]::WriteAllBytes((Join-Path $GameData $entry.Name), $payload)
    }
    if (-not (Test-ExpectedFiles $GameData $PatchFiles)) {
      throw 'TU1 setup failed its final check.'
    }
    Write-Host 'TU1 installed.' -ForegroundColor Green
  }

  New-Item -ItemType Directory -Force -Path $UserData | Out-Null

  $requiredRuntimeFiles = @(
    (Join-Path $env:WINDIR 'System32\MSVCP140.dll'),
    (Join-Path $env:WINDIR 'System32\MSVCP140_ATOMIC_WAIT.dll'),
    (Join-Path $env:WINDIR 'System32\VCRUNTIME140.dll'),
    (Join-Path $env:WINDIR 'System32\VCRUNTIME140_1.dll')
  )
  $runtimeMissing = @($requiredRuntimeFiles | Where-Object { -not (Test-Path -LiteralPath $_) })
  if ($runtimeMissing.Count -ne 0) {
    if (-not (Test-Path -LiteralPath $Redist -PathType Leaf)) {
      throw 'The Visual C++ runtime is missing, and so is its installer. Re-extract the release.'
    }
    Write-Host 'Installing the Visual C++ runtime...' -ForegroundColor Yellow
    $redistProcess = Start-Process -FilePath $Redist -ArgumentList '/install /quiet /norestart' -Wait -PassThru
    if ($redistProcess.ExitCode -notin @(0, 1638, 3010)) {
      throw "Visual C++ runtime setup failed (error $($redistProcess.ExitCode))."
    }
  }


  exit 0
}
catch {
  Write-Host ''
  Write-Host ('error: ' + $_.Exception.Message) -ForegroundColor Red
  exit 1
}
