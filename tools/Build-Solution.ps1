param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x64", "Win32")]
    [string]$Platform = "x64",

    [switch]$UseDevenv
)

$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $repoRoot "STSTest.sln"
$vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswherePath)) {
    throw "vswhere.exe not found."
}

$installPath = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath
if (-not $installPath) {
    throw "Visual Studio installation not found."
}

if ($UseDevenv) {
    $toolPath = Join-Path $installPath "Common7\IDE\devenv.com"
    if (-not (Test-Path $toolPath)) {
        throw "devenv.com not found."
    }

    & $toolPath $solutionPath /Build "$Configuration|$Platform"
    exit $LASTEXITCODE
}

$toolPath = Join-Path $installPath "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $toolPath)) {
    throw "MSBuild.exe not found."
}

& $toolPath $solutionPath "/t:Build" "/p:Configuration=$Configuration;Platform=$Platform" "/m"
exit $LASTEXITCODE
