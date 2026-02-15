# Vertex

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

## Building Vertex

Vertex uses CMake and VCPKG for its build system and dependency management.
When first cloning the repository, you'll encounter some errors in headers such as "include/runtime/plugin.hh" that certain headers are not found. This is normal.

Vertex automatically generates parts of the source code and other data and compiles them into the main project.

As of right now, the easiest way to build Vertex is by using CLion.

Under Windows, Vertex is compiled with Clang-Cl but should also work with MSVC. MingW or Clang (not the CL version!) have not been tested.

### Prerequisites

Before building Vertex, ensure you have the following installed:

1. **CMake** (version 3.30 or higher)
   - Download from: https://cmake.org/download/
   - Make sure to add CMake to your system PATH during installation

2. **VCPKG** (Package Manager)
   - Clone VCPKG: `git clone https://github.com/Microsoft/vcpkg.git`
   - Run the bootstrap script: `.\vcpkg\bootstrap-vcpkg.bat`
   - Set the `VCPKG_ROOT` environment variable to your vcpkg installation directory
   - Example: `$env:VCPKG_ROOT = "C:\vcpkg"` (PowerShell) or set it in System Environment Variables

3. **Visual Studio 2022** (or Build Tools for Visual Studio 2022)
   - Download from: https://visualstudio.microsoft.com/downloads/
   - Install with "Desktop development with C++" workload
   - Make sure to include **Clang-Cl** compiler (found under Individual Components → Compilers, build tools, and runtimes → C++ Clang Compiler for Windows)

4. **Git**
   - Download from: https://git-scm.com/download/win

### Building steps

#### Option 1: Building with CLion (Recommended)

1. **Clone the repository**
   ```bash
   git clone https://github.com/PHTNCx64/Vertex.git
   cd Vertex
   ```

2. **Open the project in CLion**
   - Launch CLion and select "Open" from the welcome screen
   - Navigate to the cloned Vertex directory and open it

3. **Configure the toolchain**
   - Go to `File` → `Settings` → `Build, Execution, Deployment` → `Toolchains`
   - Ensure Visual Studio toolchain is configured
   - Set the compiler to **Clang-Cl** (CLion should detect it automatically if installed with Visual Studio)

4. **Configure CMake**
   - Go to `File` → `Settings` → `Build, Execution, Deployment` → `CMake`
   - Ensure the `VCPKG_ROOT` environment variable is set (CLion will use it automatically)
   - CLion will automatically configure CMake and download dependencies via VCPKG
   - Wait for the initial configuration to complete (this may take some time as VCPKG downloads and builds dependencies)

5. **Build the project**
   - Select the build configuration (Debug or Release) from the dropdown in the toolbar
   - Click the hammer icon (Build) or press `Ctrl+F9`
   - The first build will take longer as VCPKG installs all dependencies

6. **Run Vertex**
   - Once the build completes successfully, click the Run button or press `Shift+F10`

**Note:** Initial configuration errors about missing headers (e.g., in `include/runtime/plugin.hh`) are expected and will be resolved automatically during the CMake generation phase, as Vertex generates source code as part of the build process.

#### Option 2: Building from Command Line

1. **Clone the repository**
   ```powershell
   git clone https://github.com/[username]/Vertex.git
   cd Vertex
   ```

2. **Ensure VCPKG_ROOT is set**
   ```powershell
   $env:VCPKG_ROOT = "C:\path\to\vcpkg"  # Adjust to your vcpkg installation path
   ```

3. **Create a build directory**
   ```powershell
   mkdir build
   cd build
   ```

4. **Configure with CMake (using Clang-Cl)**
   ```powershell
   cmake .. -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
   ```
   
   Alternatively, if using Visual Studio generator:
   ```powershell
   cmake .. -G "Visual Studio 17 2022" -A x64 -T ClangCL -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
   ```

5. **Build the project**
   ```powershell
   cmake --build . --config Release
   ```
   
   Or for Debug build:
   ```powershell
   cmake --build . --config Debug
   ```

6. **Run Vertex**
   ```powershell
   .\Release\Vertex.exe
   ```

#### Troubleshooting

- **Missing VCPKG_ROOT**: If CMake can't find VCPKG, make sure the `VCPKG_ROOT` environment variable is set correctly and restart your IDE/terminal
- **Missing dependencies**: VCPKG should automatically install all dependencies listed in `vcpkg.json`. If issues occur, try running `vcpkg install` manually in the project directory
- **Clang-Cl not found**: Ensure Clang-Cl is installed via Visual Studio Installer under Individual Components
- **Generated headers not found**: This is normal before the first build. Run CMake configuration, and the build system will generate these files automatically
- **Long build times**: The first build takes longer because VCPKG needs to download and compile all dependencies (wxWidgets, fmt, etc.). Subsequent builds will be much faster


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

Vertex is licensed under the GNU General Public License version 3 (GPLv3).

### Plugin Exception

While the Vertex runtime is licensed under GPLv3, there's a plugin exception. Please see the PLUGIN_EXCEPTION.txt in src/vertex