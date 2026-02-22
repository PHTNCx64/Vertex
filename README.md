# Vertex

### NOTE:

1. **There are some outdated informations in the docs that were removed throughout the development.**
2. **The code base is in some parts inconsistent, specifically there is some discrepancy between Vertex and Vertex User Mode Runtime**
3. **Not everything in the API SDK is in use in the runtime and certain things are subject to change or removed**

<img width="786" height="855" alt="grafik" src="https://github.com/user-attachments/assets/7575e5cb-6b73-4e7c-8d56-c7dc86efd102" /> <img width="2554" height="1402" alt="grafik" src="https://github.com/user-attachments/assets/2869e6e4-06a5-4095-8ac1-897f1fb614ab" />



Vertex is a software that is focused on dynamic reverse engineering.

It is designed to be quite portable and flexible through the Plugin API that comes with it.

It's primarily written in C++ and uses wxWidgets as its frontend. The Plugin API is written to be C ABI compatible and allows plugin developers to write modifications or extensions to the software in any language that can interface with C such as C++, Rust, Pascal, Go, Swift etc.

## What it aims to be

Vertex aims to be a healthy FOSS competitor in the field of dynamic reverse engineering software, and to be a viable alternative to the currently available options.

Another **extremely** important aspect of Vertex is the learning opportunity that it brings to me as the developer and to you as the user. The goal shouldn't just be to have fancy software available, but also to have a good learning experience for everyone involved.

## Current Status - WIP

Vertex is currently in the early stages of development, and it takes a lot of time and effort to develop a software like this, so please be patient and understanding.

Also, Vertex is currently only available for Windows, but Linux and macOS support is planned for the future, but I can't give any ETA on that right now.

I also want to emphasize that it's not possible for me to work full-time on this project, so the development process is not going to be very fast, but I will try my best to make good progress and to keep you updated on the project.

Bugs are expected, and I can't test everything myself, so if you find any bugs or have any suggestions, please don't hesitate to report them or to contribute to the project.

## Features right now

- Fast memory scanner
- Very basic debugger (watchpoints, register view, thread listing, module listing). **It's still very buggy and incomplete and the debugger is self-made**
- Plugin API (early stages, prone to change and break, but it's already usable for some basic plugins)

## Complexity

Since Vertex also aims for platform support, it is designed to be as portable as possible on both Host Runtime and Plugin side.

This can ultimately slow down development and make it more complex, but I believe that it's worth it in the long run to have a software that can be used by a wider audience and that can be extended by a wider range of developers.

## Future Plans for now

- Improve the debugger and make it more stable and feature-rich
- Pointer Scanner
- Project structures
- Support for Linux specifically
- Scripting support
- Extend and stabilize the API
- Code base consistency and robustness improvements

## Building Vertex

Vertex uses CMake, Ninja, and VCPKG for its build system and dependency management. A PowerShell build script (`build.ps1`) is provided to automate the entire process.

Under Windows, Vertex is compiled with Clang-Cl when available, falling back to MSVC automatically.

### Prerequisites

Before building Vertex, ensure you have the following installed:

1. **Git**
   - Download from: https://git-scm.com/download/win

2. **CMake** (version 3.30 or higher)
   - Download from: https://cmake.org/download/
   - Make sure to add CMake to your system PATH during installation

3. **Ninja** (build system)
   - Download from: https://github.com/nicknisi/ninja/releases
   - Add `ninja.exe` to your system PATH

4. **VCPKG** (Package Manager)
   - Clone VCPKG: `git clone https://github.com/Microsoft/vcpkg.git`
   - Run the bootstrap script: `.\vcpkg\bootstrap-vcpkg.bat`
   - Set the `VCPKG_ROOT` environment variable to your vcpkg installation directory
   - Example: `$env:VCPKG_ROOT = "C:\vcpkg"` (PowerShell) or set it in System Environment Variables

5. **Visual Studio 2022** (or Build Tools for Visual Studio 2022)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Install with the **"Desktop development with C++"** workload
   - Optionally include **Clang-Cl** compiler for preferred compilation (found under Individual Components → Compilers, build tools, and runtimes → C++ Clang Compiler for Windows). If not installed, MSVC will be used instead.

### Building with `build.ps1`

1. **Clone the repository**
   ```powershell
   git clone https://github.com/PHTNCx64/Vertex.git
   cd Vertex
   ```

2. **Run the build script**
   ```powershell
   ./build.ps1
   ```
   This will automatically detect your MSVC environment, locate Clang-Cl (or fall back to MSVC), configure CMake with Ninja, and build the project in Debug mode.

#### Build script options

| Parameter       | Description                                           | Default   |
|-----------------|-------------------------------------------------------|-----------|
| `-VcpkgRoot`    | Path to vcpkg (uses `VCPKG_ROOT` env var if omitted)  | auto      |
| `-Config`       | Build configuration: `Debug`, `Release`, or `RelWithDebInfo` | `Debug`   |
| `-BuildDir`     | Output build directory                                | `build`   |
| `-BuildAll`     | Build all targets                                     | off       |
| `-RuntimeOnly`  | Build only the runtime (no plugins)                   | off       |
| `-Usrrt`        | Build the vertexusrrt plugin                          | on (via CMake) |
| `-Deci3rt`      | Build the Deci3 runtime target                        | off       |
| `-NoTests`      | Disable building tests                                | off       |
| `-Clean`        | Remove the build directory before building            | off       |
| `-Configure`    | Run CMake configuration only, without building        | off       |

#### Examples

```powershell
# Release build
./build.ps1 -Config Release

# Clean rebuild
./build.ps1 -Clean

# Configure only (no build)
./build.ps1 -Configure

# Release build without tests
./build.ps1 -Config Release -NoTests

# Specify vcpkg path explicitly
./build.ps1 -VcpkgRoot "C:\vcpkg"

# Build all targets in release
./build.ps1 -Config Release -BuildAll
```

### Building with CLion

1. Open the cloned Vertex directory in CLion
2. Go to `File` → `Settings` → `Build, Execution, Deployment` → `Toolchains` and ensure Visual Studio is configured
3. Go to `File` → `Settings` → `Build, Execution, Deployment` → `CMake` and verify the `VCPKG_ROOT` environment variable is set
4. CLion will automatically configure CMake and download dependencies via VCPKG
5. Select the build configuration (Debug or Release) and build with `Ctrl+F9`

**Note:** Initial configuration errors about missing headers (e.g., in `include/runtime/plugin.hh`) are expected and will be resolved automatically during the CMake generation phase, as Vertex generates source code as part of the build process.

#### Troubleshooting

- **Missing VCPKG_ROOT**: Make sure the `VCPKG_ROOT` environment variable is set correctly and restart your terminal
- **Missing dependencies**: VCPKG should automatically install all dependencies listed in `vcpkg.json`. If issues occur, try running `vcpkg install` manually in the project directory
- **Clang-Cl not found**: The build script will fall back to MSVC automatically. To use Clang-Cl, install it via Visual Studio Installer under Individual Components
- **Generated headers not found**: This is normal before the first build. Run CMake configuration, and the build system will generate these files automatically
- **Long build times**: The first build takes longer because VCPKG needs to download and compile all dependencies (wxWidgets, fmt, etc.). Subsequent builds will be much faster
- **Ninja not found**: Ensure `ninja.exe` is on your system PATH


## Support and Contributions

### Financial

Currently, I don't accept any financial support. Maybe in the future I'll set up some financial support options if people want to help through this way, but right now? Nah.

### Code Contributions

Everyone's welcome to help Vertex with code contributions, and I appreciate anyone who's willing to help in this aspect. You can contribute through Pull Requests.

### Translations

Vertex has support for translations and this aspect of support and contribution is also really appreciated.

Through this, Vertex becomes more accessible to a wider audience and helps non-English speakers to use Vertex and learn from it. If you want to contribute with translations, you can fork the repository and make a pull request with your translations. I'll review the changes and merge them if they are good.

### Community

Unsurprisingly enough, Vertex has a discord server where you can join and ask questions, report bugs, share your plugins, or just chat with other people who are interested in reverse engineering and in Vertex in general. You can join the server through this invite link: https://discord.gg/BJYcT6waVx

## License

Vertex (src/vertex and include/vertex) is licensed under the GNU General Public License version 3 (GPLv3).
Vertex User Mode Runtime is licensed Lesser GNU General Public License version 3 LGPLv3
Vertex SDK is licensed under Apache 2.0 License

### Plugin Exception

While the Vertex runtime is licensed under GPLv3, there's a plugin exception. Please see the PLUGIN_EXCEPTION.txt in src/vertex
