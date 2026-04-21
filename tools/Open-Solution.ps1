param()

$repoRoot = Split-Path -Parent $PSScriptRoot
$solutionPath = Join-Path $repoRoot "STSTest.sln"
$vswherePath = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"

if (-not (Test-Path $vswherePath)) {
    throw "vswhere.exe not found."
}

$devenvPath = & $vswherePath -latest -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property productPath
if (-not $devenvPath) {
    throw "devenv.exe not found."
}

Start-Process -FilePath $devenvPath -ArgumentList $solutionPath
