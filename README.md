# NPET communication FW

[![CI](https://github.com/MattStav/NPET_communication_FW/actions/workflows/testing_pipeline.yml/badge.svg)](https://github.com/MattStav/NPET_communication_FW/actions/workflows/testing_pipeline.yml)![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=c%2B%2B&logoColor=white)
![CMake](https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white)
![vcpkg](https://img.shields.io/badge/dependencies-vcpkg-0078D4)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20MSYS2-blue)
![Python Bindings](https://img.shields.io/badge/python-bindings-3776AB?logo=python&logoColor=white)

---

This firmware facilitates control and monitoring of 
NPET (New Pico Event Timer) devices developed at CTU FNSPE.
Use this project with the [NPET Data Processing](https://github.com/MattStav/NPET_data_processing) 
to gather and process data from NPET.

This FW uses CLI.
Launch the compiled executable and follow on-screen instructions,
see [the manual](./MANUAL.md) for more details.
The manual is also compiled into the executable and can be accessed therein, either:
1. In the main menu (this requires NPET to be connected to your device)
2. Using the `manual` command line argument when launching the executable.

For instruction to the Python library, see [here](#importing-the-fw-python-library)

---

## Development Setup Instructions
Below are the step-by-step instructions to set up the environment,
configure the project, and build it successfully.

### Prerequisites

- **MSYS2**: Download and install MSYS2 from [https://www.msys2.org/](https://www.msys2.org/).

### 1. Install and Configure MSYS2

#### 1. Open the MSYS2 terminal. Update the package database and core system packages via:
```
pacman -Syu
```
Close and reopen the terminal, then run the update command again!
#### 2. Install all the required MSYS2 tools with command:
```
pacman -S base-devel
mingw-w64-x86_64-toolchain
mingw-w64-x86_64-cmake
mingw-w64-x86_64-make
mingw-w64-x86_64-python
mingw-w64-x86_64-python-pip
mingw-w64-x86_64-pybind11
mingw-w64-x86_64-ninja
```
Use _shift + insert_ to paste the command into the MSYS2 terminal.
#### 3. Add the MSYS2 directories to your system's PATH
Do this via `System Properties` > `Environment Variables` > `Path`. \
Paths to add (might differ on your device):
- `C:\msys64\mingw64\bin`
- `C:\msys64\usr\bin`
#### 4. Verify the installation in a terminal
It is necessary to open a fresh terminal window!
``` shell
gcc --version
g++ --version
cmake --version
make --version
   ```
### 2. Clone the Repository
Clone the project repository to your local machine:
``` shell
git clone https://github.com/MattStav/NPET_communication_FW.git
```
or use an IDE (such as CLion) to handle the cloning process.

### 3. Install External Dependencies
External dependencies are defined in `vcpkg.json` and managed via vcpkg.
CMake will automatically detect and link the installed dependencies during the configuration.
It is important that the configuration runs with the following args:

- Path to vcpkg toolchain file, this arg depends on your vcpkg installation
  but can be something like:
  `-DCMAKE_TOOLCHAIN_FILE=C:\Users\[username]\.vcpkg-clion\vcpkg\scripts\buildsystems\vcpkg.cmake`
- Selection of the vcpkg target triplet, which depends on your system,
  but can be something like: `-DVCPKG_TARGET_TRIPLET=x64-mingw-static`.
  __The triplet should be static!__
  This is so the program can be built into a standalone executable.

### 4. Build the project
When using the IDE built-in build function,
it is necessary to set toolset to `msys64\mingw64`.
When building from the terminal, follow these steps:

1. Create a build directory `mkdir build` and navigate into it: `cd build`
2. Run CMake to generate the build files:
   ```shell
   cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=[path] -DVCPKG_TARGET_TRIPLET=x64-mingw-static
   ```
3. Build the project:
   ```shell
   cmake --build .
   ```
### 5. Run the executable `NPET_comm_FW_CLI`

## Importing the FW Python library
__WARNING: The Python library is compiled for MinGW Python!__
Make sure to use the MinGW Python interpreter when importing the library in your Python script.
To import the built Python library in your Python script,
place the `.pyd` library file within your project directory
(or add its directory to PYTHONPATH) and import it within your script:

```
import NPET_comm_FW_lib
```

This doesn't require installation via pip.

## Acknowledgements
This project was inspired by an earlier work created by G. Laurent,
which was later revised by J. Blazej and P. Trojanek.

## Third party licenses
This project includes licensed third-party software.
See the included [third-party license file](THIRD_PARTY_NOTICES.txt) for details.
