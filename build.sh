#!/usr/bin/env bash
set -euo pipefail

# --- defaults ---
VCPKG_ROOT_ARG=""
CONFIG="Debug"
BUILD_DIR="build"
BUILD_ALL=false
RUNTIME_ONLY=false
VERTEX_ONLY=false
USRRT=false
DECI3RT=false # this can't be compiled on linux or mac since the runtime library it relies on libraries that aren't available on linux.
NO_TESTS=false
CLEAN=false
CONFIGURE_ONLY=false
MINGW=false
SETCAP=false

# --- colors ---
RED='\033[0;31m'
YELLOW='\033[0;33m'
CYAN='\033[0;36m'
GRAY='\033[0;37m'
GREEN='\033[0;32m'
NC='\033[0m'

usage() {
    cat <<EOF
Usage: ./build.sh [options]

Options:
  --vcpkg-root <path>   Path to vcpkg installation (or set VCPKG_ROOT env var)
  --config <type>       Build configuration: Debug, Release, RelWithDebInfo (default: Debug)
  --build-dir <path>    Build output directory (default: build)
  --build-all           Enable all plugins and components
  --runtime-only        Build runtime plugins only (no Vertex GUI)
  --vertex-only         Build only the Vertex GUI app (plugins off)
  --usrrt               Build the vertexusrrt plugin
  --deci3rt             Build the vertexdeci3rt plugin
  --mingw               Cross-compile for Windows with MinGW-w64
  --no-tests            Disable test builds
  --clean               Clean build directory before configuration
  --configure           Only configure, don't build
  --setcap              After build, grant cap_sys_ptrace + cap_dac_read_search to Vertex (requires sudo; Linux only)
  -h, --help            Show this help message
EOF
    exit 0
}

# --- parse arguments ---
while [[ $# -gt 0 ]]; do
    case "$1" in
        --vcpkg-root)
            VCPKG_ROOT_ARG="$2"
            shift 2
            ;;
        --config)
            CONFIG="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        --build-all)    BUILD_ALL=true;      shift ;;
        --runtime-only) RUNTIME_ONLY=true;   shift ;;
        --vertex-only)  VERTEX_ONLY=true;    shift ;;
        --usrrt)        USRRT=true;          shift ;;
        --deci3rt)      DECI3RT=true;        shift ;;
        --mingw)        MINGW=true;          shift ;;
        --no-tests)     NO_TESTS=true;       shift ;;
        --clean)        CLEAN=true;          shift ;;
        --configure)    CONFIGURE_ONLY=true; shift ;;
        --setcap)       SETCAP=true;         shift ;;
        -h|--help)      usage ;;
        *)
            echo -e "${RED}ERROR: Unknown option: $1${NC}"
            usage
            ;;
    esac
done

# --- validate config ---
case "$CONFIG" in
    Debug|Release|RelWithDebInfo) ;;
    *)
        echo -e "${RED}ERROR: Invalid config '$CONFIG'. Must be Debug, Release, or RelWithDebInfo.${NC}"
        exit 1
        ;;
esac

# --- validate option combinations ---
if $RUNTIME_ONLY && $VERTEX_ONLY; then
    echo -e "${RED}ERROR: --runtime-only and --vertex-only cannot be used together.${NC}"
    exit 1
fi

# --- resolve vcpkg root ---
if [[ -n "$VCPKG_ROOT_ARG" ]]; then
    VCPKG_ROOT="$VCPKG_ROOT_ARG"
elif [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo -e "${RED}ERROR: vcpkg root not specified.${NC}"
    echo -e "${YELLOW}  Usage: ./build.sh --vcpkg-root <path-to-vcpkg>${NC}"
    echo -e "${YELLOW}  Or set the VCPKG_ROOT environment variable.${NC}"
    exit 1
fi

TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
if [[ ! -f "$TOOLCHAIN_FILE" ]]; then
    echo -e "${RED}ERROR: vcpkg toolchain not found at: $TOOLCHAIN_FILE${NC}"
    exit 1
fi

export VCPKG_ROOT

# --- detect compiler ---
COMPILER="GCC"
CC=""
CXX=""

if $MINGW; then
    if command -v x86_64-w64-mingw32-gcc &>/dev/null && command -v x86_64-w64-mingw32-g++ &>/dev/null; then
        CC="$(command -v x86_64-w64-mingw32-gcc)"
        CXX="$(command -v x86_64-w64-mingw32-g++)"
        COMPILER="MinGW"
    else
        echo -e "${RED}ERROR: MinGW toolchain not found. Install x86_64-w64-mingw32-gcc and x86_64-w64-mingw32-g++.${NC}"
        exit 1
    fi
elif command -v clang &>/dev/null && command -v clang++ &>/dev/null; then
    CC="$(command -v clang)"
    CXX="$(command -v clang++)"
    COMPILER="Clang"
elif command -v gcc &>/dev/null && command -v g++ &>/dev/null; then
    CC="$(command -v gcc)"
    CXX="$(command -v g++)"
    COMPILER="GCC"
else
    echo -e "${RED}ERROR: No C/C++ compiler found. Install clang or gcc.${NC}"
    exit 1
fi

# --- check for ninja ---
if ! command -v ninja &>/dev/null; then
    echo -e "${RED}ERROR: Ninja build system not found. Install ninja-build.${NC}"
    exit 1
fi

# --- resolve build options ---
OPT_VERTEX=ON
OPT_VERTEXUSRRT=ON
OPT_VERTEXDECI3RT=OFF
OPT_LIBTEST=ON
OPT_TESTING=ON
OPT_ONLY_RUNTIME=OFF
OPT_ALL=OFF

if $BUILD_ALL; then
    OPT_ALL=ON
    OPT_VERTEXUSRRT=ON
    OPT_VERTEXDECI3RT=ON
    OPT_LIBTEST=ON
    OPT_TESTING=ON
fi

if $RUNTIME_ONLY; then
    OPT_ONLY_RUNTIME=ON
    OPT_VERTEX=OFF
    OPT_TESTING=OFF
fi

if $VERTEX_ONLY; then
    OPT_VERTEXUSRRT=OFF
    OPT_VERTEXDECI3RT=OFF
    OPT_LIBTEST=OFF
fi

if $USRRT;    then OPT_VERTEXUSRRT=ON;    fi
if $DECI3RT;  then OPT_VERTEXDECI3RT=ON;  fi
if $NO_TESTS; then OPT_TESTING=OFF;       fi

if $MINGW; then
    BUILD_DIR="${BUILD_DIR}-mingw"
    OPT_TESTING=OFF
fi

# --- build cmake arguments ---
CMAKE_ARGS=(
    -B "$BUILD_DIR"
    -G "Ninja"
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN_FILE"
    -DCMAKE_BUILD_TYPE="$CONFIG"
    -DCMAKE_C_COMPILER="$CC"
    -DCMAKE_CXX_COMPILER="$CXX"
    -DBUILD_ALL="$OPT_ALL"
    -DBUILD_ONLY_RUNTIME="$OPT_ONLY_RUNTIME"
    -DBUILD_VERTEX="$OPT_VERTEX"
    -DBUILD_VERTEXUSRRT="$OPT_VERTEXUSRRT"
    -DBUILD_VERTEXDECI3RT="$OPT_VERTEXDECI3RT"
    -DBUILD_LIBTEST="$OPT_LIBTEST"
    -DBUILD_TESTING="$OPT_TESTING"
)

if $MINGW; then
    CMAKE_ARGS+=(
        -DVCPKG_TARGET_TRIPLET="x64-mingw-static"
        -DCMAKE_SYSTEM_NAME="Windows"
        -DCMAKE_SYSTEM_PROCESSOR="x86_64"
        -DCMAKE_CROSSCOMPILING="TRUE"
        -DCMAKE_RC_COMPILER="x86_64-w64-mingw32-windres"
        -DVCPKG_APPLOCAL_DEPS=OFF
        -DX_VCPKG_APPLOCAL_DEPS_INSTALL=OFF
    )
fi

if $CLEAN; then
    CMAKE_ARGS+=(--fresh)
fi

# --- clean ---
if $CLEAN && [[ -d "$BUILD_DIR" ]]; then
    echo -e "${CYAN}Cleaning build directory: $BUILD_DIR${NC}"
    rm -rf "$BUILD_DIR"
fi

# --- mingw-specific pre-step: install vcpkg deps and patch broken wxWidgets wrapper ---
# The x64-mingw-static wxwidgets port ships a vcpkg-cmake-wrapper.cmake whose
# Windows branch searches for MSVC-style names (wxbase33u) that don't exist in
# a mingw install (which uses wx_baseu-3.3-Windows). We install the manifest
# first, then replace the wrapper's Windows branch with the unix-mode branch.
if $MINGW; then
    echo -e "${CYAN}Pre-installing vcpkg manifest for mingw...${NC}"
    mkdir -p "$BUILD_DIR"
    "$VCPKG_ROOT/vcpkg" install \
        --triplet=x64-mingw-static \
        --overlay-triplets="$PWD/triplets" \
        --overlay-ports="$PWD/overlay-ports" \
        --x-manifest-root="$PWD" \
        --x-install-root="$BUILD_DIR/vcpkg_installed" \
        --feature-flags=manifests

    WX_WRAPPER="$BUILD_DIR/vcpkg_installed/x64-mingw-static/share/wxwidgets/vcpkg-cmake-wrapper.cmake"
    if [[ -f "$WX_WRAPPER" ]] && ! grep -q "VERTEX_MINGW_PATCH" "$WX_WRAPPER"; then
        echo -e "${CYAN}Patching wxWidgets vcpkg wrapper for mingw...${NC}"
        python3 - "$WX_WRAPPER" <<'PYEOF'
import sys, re, pathlib
path = pathlib.Path(sys.argv[1])
text = path.read_text()
new_win_branch = """# VERTEX_MINGW_PATCH: the upstream Windows branch assumes MSVC-style library
# naming (wxbase33u) which does not exist in x64-mingw-static. For mingw we
# rely on wxWidgetsTargets.cmake (CONFIG mode) to expose wx::base/wx::core
# etc, so we only need to set wxWidgets_LIB_DIR for legacy consumers.
if(WIN32)
    set(wxWidgets_LIB_DIR \"${wxWidgets_ROOT_DIR}/lib\" CACHE INTERNAL \"\")
    set(_vcpkg_wxwidgets_backup_crosscompiling \"${CMAKE_CROSSCOMPILING}\")
    set(CMAKE_CROSSCOMPILING 0)"""
patched, count = re.subn(
    r"if\(WIN32\)\n(?:.*\n)*?    set\(wxWidgets_LIB_DIR \"\$\{wxWidgets_ROOT_DIR\}/lib\" CACHE INTERNAL \"\"\)",
    new_win_branch,
    text,
    count=1,
)
if count != 1:
    sys.exit(f"Could not locate Windows branch in {path}")
path.write_text(patched)
PYEOF
    fi
fi

# --- configure ---
echo -e "${CYAN}Configuring with CMake...${NC}"
echo -e "${GRAY}  vcpkg:    $VCPKG_ROOT${NC}"
echo -e "${GRAY}  config:   $CONFIG${NC}"
echo -e "${GRAY}  compiler: $COMPILER ($CC)${NC}"
if $MINGW; then
    echo -e "${GRAY}  triplet:  x64-mingw-static${NC}"
fi

cmake "${CMAKE_ARGS[@]}"

# --- build ---
if ! $CONFIGURE_ONLY; then
    echo -e "${CYAN}Building...${NC}"
    cmake --build "$BUILD_DIR"
    echo -e "${GREEN}Build complete.${NC}"

    if $SETCAP; then
        if $MINGW; then
            echo -e "${YELLOW}--setcap ignored: not applicable to MinGW cross-build.${NC}"
        elif [[ "$OPT_VERTEX" != "ON" ]]; then
            echo -e "${YELLOW}--setcap ignored: Vertex target not built.${NC}"
        else
            echo -e "${CYAN}Applying capabilities (sudo required)...${NC}"
            if ! sudo -v; then
                echo -e "${RED}ERROR: sudo authentication failed.${NC}"
                exit 1
            fi
            cmake --build "$BUILD_DIR" --target setcap
            echo -e "${GREEN}Capabilities applied.${NC}"
        fi
    fi
fi
