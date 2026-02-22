param(
    [string]$VcpkgRoot,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",
    [string]$BuildDir = "build",
    [switch]$BuildAll,
    [switch]$RuntimeOnly,
    [switch]$Usrrt,
    [switch]$Deci3rt,
    [switch]$NoTests,
    [switch]$Clean,
    [switch]$Configure
)

$ErrorActionPreference = "Stop"

function Import-VsEnvironment {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        Write-Host "ERROR: Visual Studio not found. Install Visual Studio with C++ workload." -ForegroundColor Red
        exit 1
    }

    $vsPath = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) {
        Write-Host "ERROR: No Visual Studio installation with C++ tools found." -ForegroundColor Red
        exit 1
    }

    $vcvarsall = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
    if (-not (Test-Path $vcvarsall)) {
        Write-Host "ERROR: vcvarsall.bat not found at: $vcvarsall" -ForegroundColor Red
        exit 1
    }

    Write-Host "Loading MSVC environment from: $vsPath" -ForegroundColor Cyan
    $output = cmd /c "`"$vcvarsall`" x64 >nul 2>&1 && set"
    foreach ($line in $output) {
        if ($line -match "^([^=]+)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
}

if (-not $env:VSINSTALLDIR) {
    Import-VsEnvironment
}

if (-not $VcpkgRoot) {
    $VcpkgRoot = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "User")
    if (-not $VcpkgRoot) {
        $VcpkgRoot = [System.Environment]::GetEnvironmentVariable("VCPKG_ROOT", "Machine")
    }
    if (-not $VcpkgRoot) {
        Write-Host "ERROR: vcpkg root not specified." -ForegroundColor Red
        Write-Host "  Usage: ./build.ps1 -VcpkgRoot <path-to-vcpkg>" -ForegroundColor Yellow
        Write-Host "  Or set the VCPKG_ROOT environment variable in Windows." -ForegroundColor Yellow
        exit 1
    }
}

$toolchainFile = Join-Path $VcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path $toolchainFile)) {
    Write-Host "ERROR: vcpkg toolchain not found at: $toolchainFile" -ForegroundColor Red
    exit 1
}

$env:VCPKG_ROOT = $VcpkgRoot

$compiler = "MSVC"
$clangClPath = $null

$clangCl = Get-Command clang-cl -ErrorAction SilentlyContinue
if ($clangCl) {
    $clangClPath = $clangCl.Source
    $compiler = "ClangCL"
} else {
    $vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
        -latest -property installationPath 2>$null
    if ($vsPath) {
        $candidate = Join-Path $vsPath "VC\Tools\Llvm\x64\bin\clang-cl.exe"
        if (Test-Path $candidate) {
            $clangClPath = $candidate
            $compiler = "ClangCL"
        }
    }
}

$cmakeArgs = @(
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DCMAKE_BUILD_TYPE=$Config"
)

if ($Clean) {
    $cmakeArgs += "--fresh"
}

if ($compiler -eq "ClangCL") {
    Write-Host "Using Clang-CL: $clangClPath" -ForegroundColor Cyan
    $cmakeArgs += "-DCMAKE_C_COMPILER=$clangClPath"
    $cmakeArgs += "-DCMAKE_CXX_COMPILER=$clangClPath"
} else {
    Write-Host "Clang-CL not found, falling back to MSVC" -ForegroundColor Yellow
}

if ($BuildAll)    { $cmakeArgs += "-DBUILD_ALL=ON" }
if ($RuntimeOnly) { $cmakeArgs += "-DBUILD_ONLY_RUNTIME=ON" }
if ($Usrrt)       { $cmakeArgs += "-DBUILD_VERTEXUSRRT=ON" }
if ($Deci3rt)     { $cmakeArgs += "-DBUILD_VERTEXDECI3RT=ON" }
if ($NoTests)     { $cmakeArgs += "-DBUILD_TESTING=OFF" }

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Cleaning build directory: $BuildDir" -ForegroundColor Cyan
    Remove-Item -Recurse -Force $BuildDir
}

Write-Host "Configuring with CMake..." -ForegroundColor Cyan
Write-Host "  vcpkg:    $VcpkgRoot" -ForegroundColor Gray
Write-Host "  config:   $Config" -ForegroundColor Gray
Write-Host "  compiler: $compiler" -ForegroundColor Gray
cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $Configure) {
    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Build complete." -ForegroundColor Green
}
