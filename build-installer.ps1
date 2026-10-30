param(
    [string]$BuildDir = "out/build/simplecamptz-windows-x64",
    [ValidateSet("Debug", "RelWithDebInfo", "Release", "MinSizeRel")]
    [string]$Configuration = "RelWithDebInfo"
)

$ErrorActionPreference = "Stop"
$projectRoot = $PSScriptRoot
if (-not $projectRoot) {
    $projectRoot = (Get-Location).Path
}

if (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $projectRoot $BuildDir
}
$BuildDir = [System.IO.Path]::GetFullPath($BuildDir)
$stageDir = Join-Path $projectRoot "stage"
$pluginId = "simplecamptz"
$stagedDll = Join-Path $stageDir "$pluginId\bin\64bit\$pluginId.dll"
$stagedLocale = Join-Path $stageDir "$pluginId\data\locale\en-US.ini"
$installerScript = Join-Path $projectRoot "installer.iss"

if (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    throw "CMake build directory not found or not configured: $BuildDir"
}

$cmake = Get-Command "cmake.exe" -ErrorAction SilentlyContinue
if (-not $cmake) {
    $cmake = Get-Command "cmake" -ErrorAction SilentlyContinue
}
if (-not $cmake) {
    throw "CMake was not found on PATH. Open a Visual Studio developer PowerShell or add CMake to PATH."
}

& $cmake.Source --install $BuildDir --prefix $stageDir --config $Configuration
if ($LASTEXITCODE -ne 0) {
    throw "CMake install staging failed with exit code $LASTEXITCODE."
}

if (-not (Test-Path $stagedDll)) {
    throw "Staged plugin DLL not found: $stagedDll"
}
if (-not (Test-Path $stagedLocale)) {
    throw "Staged locale file not found: $stagedLocale"
}

$compilerCommand = Get-Command "ISCC.exe" -ErrorAction SilentlyContinue
$compilerCandidates = @(
    $compilerCommand.Source,
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 6\ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 6\ISCC.exe"),
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 6\ISCC.exe"),
    (Join-Path ${env:ProgramFiles(x86)} "Inno Setup 7\ISCC.exe"),
    (Join-Path $env:ProgramFiles "Inno Setup 7\ISCC.exe"),
    (Join-Path $env:LOCALAPPDATA "Programs\Inno Setup 7\ISCC.exe")
) | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique

if (-not $compilerCandidates) {
    throw "Inno Setup Compiler (ISCC.exe) was not found. Install Inno Setup, then run this script again."
}

$compiler = $compilerCandidates | Select-Object -First 1
& $compiler $installerScript
if ($LASTEXITCODE -ne 0) {
    throw "Inno Setup failed with exit code $LASTEXITCODE."
}

$pluginVersion = (Get-Content (Join-Path $projectRoot "buildspec.json") -Raw | ConvertFrom-Json).version
$installerPath = Join-Path $projectRoot "dist\SimpleCamPTZ-Setup-$pluginVersion-x64.exe"
Write-Output "Installer created: $installerPath"
