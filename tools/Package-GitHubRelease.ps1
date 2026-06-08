param(
    [switch]$Rebuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildScript = Join-Path $PSScriptRoot "Build-Solution.ps1"
$buildOutputDir = Join-Path $repoRoot "x64\Release"
$trackedReleaseDir = Join-Path $repoRoot "dist\x64-Release"
$stagingRoot = Join-Path $repoRoot "dist\github-release"
$bundleName = "Blitz-of-Card-win64"
$bundleDir = Join-Path $stagingRoot $bundleName
$zipPath = Join-Path $repoRoot ("dist\" + $bundleName + ".zip")
$hashPath = Join-Path $repoRoot ("dist\" + $bundleName + ".sha256.txt")

if ($Rebuild) {
    & $buildScript -Configuration Release -Platform x64
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

$requiredPaths = @(
    (Join-Path $buildOutputDir "Blitz of Card.exe"),
    (Join-Path $buildOutputDir "Assets"),
    (Join-Path $trackedReleaseDir "README.txt"),
    (Join-Path $trackedReleaseDir "Run-Fullscreen.bat"),
    (Join-Path $trackedReleaseDir "Run-Windowed.bat")
)

foreach ($path in $requiredPaths) {
    if (-not (Test-Path $path)) {
        throw "Required path not found: $path"
    }
}

New-Item -ItemType Directory -Force -Path $trackedReleaseDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $trackedReleaseDir "Assets") | Out-Null

$nestedTrackedAssetsDir = Join-Path $trackedReleaseDir "Assets\Assets"
if (Test-Path $nestedTrackedAssetsDir) {
    Remove-Item -LiteralPath $nestedTrackedAssetsDir -Recurse -Force
}

Copy-Item -LiteralPath (Join-Path $buildOutputDir "Blitz of Card.exe") `
    -Destination (Join-Path $trackedReleaseDir "Blitz of Card.exe") `
    -Force
Copy-Item -Path (Join-Path $buildOutputDir "Assets\*") `
    -Destination (Join-Path $trackedReleaseDir "Assets") `
    -Recurse `
    -Force

$trackedPdbPath = Join-Path $trackedReleaseDir "Blitz of Card.pdb"
if (Test-Path $trackedPdbPath) {
    Remove-Item -LiteralPath $trackedPdbPath -Force
}

if (Test-Path $bundleDir) {
    Remove-Item -LiteralPath $bundleDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $bundleDir "Assets") | Out-Null

Copy-Item -LiteralPath (Join-Path $trackedReleaseDir "Blitz of Card.exe") `
    -Destination (Join-Path $bundleDir "Blitz of Card.exe") `
    -Force
Copy-Item -LiteralPath (Join-Path $trackedReleaseDir "README.txt") `
    -Destination (Join-Path $bundleDir "README.txt") `
    -Force
Copy-Item -LiteralPath (Join-Path $trackedReleaseDir "Run-Fullscreen.bat") `
    -Destination (Join-Path $bundleDir "Run-Fullscreen.bat") `
    -Force
Copy-Item -LiteralPath (Join-Path $trackedReleaseDir "Run-Windowed.bat") `
    -Destination (Join-Path $bundleDir "Run-Windowed.bat") `
    -Force
Copy-Item -Path (Join-Path $trackedReleaseDir "Assets\*") `
    -Destination (Join-Path $bundleDir "Assets") `
    -Recurse `
    -Force

if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Compress-Archive -Path $bundleDir -DestinationPath $zipPath -Force

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zipPath).Hash
Set-Content -Path $hashPath -Value ($hash + " *" + $bundleName + ".zip") -Encoding ascii

Write-Host ("Release folder synced: " + $trackedReleaseDir)
Write-Host ("GitHub release bundle: " + $bundleDir)
Write-Host ("Zip package: " + $zipPath)
Write-Host ("SHA256: " + $hashPath)
