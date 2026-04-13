$ErrorActionPreference = "Stop"

# --- defaults ---
$VcpkgRoot = $null
$Config = "Debug"
$BuildDir = "build"
$BuildAll = $false
$RuntimeOnly = $false
$VertexOnly = $false
$Usrrt = $false
$Deci3rt = $false
$NoTests = $false
$Clean = $false
$ConfigureOnly = $false

function Show-Usage {
    @"
Usage: ./build.ps1 [options]

Options:
  --vcpkg-root <path>   Path to vcpkg installation (or set VCPKG_ROOT env var)
  --config <type>       Build configuration: Debug, Release, RelWithDebInfo (default: Debug)
  --build-dir <path>    Build output directory (default: build)
  --build-all           Enable all plugins and components
  --runtime-only        Build runtime plugins only (no Vertex GUI)
  --vertex-only         Build only the Vertex GUI app (plugins off)
  --usrrt               Build the vertexusrrt plugin
  --deci3rt             Build the vertexdeci3rt plugin
  --no-tests            Disable test builds
  --clean               Clean build directory before configuration
  --configure           Only configure, don't build
  -h, --help            Show this help message
"@ | Write-Host
}

function Get-OptionValue {
    param(
        [string[]]$AllArgs,
        [ref]$Index,
        [string]$OptionName
    )

    if (($Index.Value + 1) -ge $AllArgs.Count) {
        Write-Host "ERROR: Missing value for $OptionName." -ForegroundColor Red
        exit 1
    }

    $Index.Value++
    return $AllArgs[$Index.Value]
}

# --- parse arguments ---
for ($i = 0; $i -lt $args.Count; $i++) {
    $arg = $args[$i]
    switch ($arg) {
        "--vcpkg-root" { $VcpkgRoot = Get-OptionValue -AllArgs $args -Index ([ref]$i) -OptionName $arg; continue }
        "--config" { $Config = Get-OptionValue -AllArgs $args -Index ([ref]$i) -OptionName $arg; continue }
        "--build-dir" { $BuildDir = Get-OptionValue -AllArgs $args -Index ([ref]$i) -OptionName $arg; continue }
        "--build-all" { $BuildAll = $true; continue }
        "--runtime-only" { $RuntimeOnly = $true; continue }
        "--vertex-only" { $VertexOnly = $true; continue }
        "--usrrt" { $Usrrt = $true; continue }
        "--deci3rt" { $Deci3rt = $true; continue }
        "--no-tests" { $NoTests = $true; continue }
        "--clean" { $Clean = $true; continue }
        "--configure" { $ConfigureOnly = $true; continue }
        "--help" { Show-Usage; exit 0 }
        "-h" { Show-Usage; exit 0 }

        default {
            Write-Host "ERROR: Unknown option: $arg" -ForegroundColor Red
            Show-Usage
            exit 1
        }
    }
}

switch ($Config) {
    "Debug" {}
    "Release" {}
    "RelWithDebInfo" {}
    default {
        Write-Host "ERROR: Invalid config '$Config'. Must be Debug, Release, or RelWithDebInfo." -ForegroundColor Red
        exit 1
    }
}

if ($RuntimeOnly -and $VertexOnly) {
    Write-Host "ERROR: --runtime-only and --vertex-only cannot be used together." -ForegroundColor Red
    exit 1
}

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
        Write-Host "  Usage: ./build.ps1 --vcpkg-root <path-to-vcpkg>" -ForegroundColor Yellow
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

# --- resolve build options ---
$optVertex        = "ON"
$optVertexusrrt   = "ON"
$optVertexdeci3rt = "OFF"
$optLibtest       = "ON"
$optTesting       = "ON"
$optOnlyRuntime   = "OFF"
$optAll           = "OFF"

if ($BuildAll) {
    $optAll           = "ON"
    $optVertexusrrt   = "ON"
    $optVertexdeci3rt = "ON"
    $optLibtest       = "ON"
    $optTesting       = "ON"
}

if ($RuntimeOnly) {
    $optOnlyRuntime = "ON"
    $optVertex      = "OFF"
    $optTesting     = "OFF"
}

if ($VertexOnly) {
    $optVertexusrrt   = "OFF"
    $optVertexdeci3rt = "OFF"
    $optLibtest       = "OFF"
}

if ($Usrrt)   { $optVertexusrrt   = "ON" }
if ($Deci3rt) { $optVertexdeci3rt = "ON" }
if ($NoTests) { $optTesting       = "OFF" }

# --- build cmake arguments ---
$cmakeArgs = @(
    "-B", $BuildDir,
    "-G", "Ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchainFile",
    "-DCMAKE_BUILD_TYPE=$Config",
    "-DBUILD_ALL=$optAll",
    "-DBUILD_ONLY_RUNTIME=$optOnlyRuntime",
    "-DBUILD_VERTEX=$optVertex",
    "-DBUILD_VERTEXUSRRT=$optVertexusrrt",
    "-DBUILD_VERTEXDECI3RT=$optVertexdeci3rt",
    "-DBUILD_LIBTEST=$optLibtest",
    "-DBUILD_TESTING=$optTesting"
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

if (-not $ConfigureOnly) {
    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Host "Build complete." -ForegroundColor Green
}
