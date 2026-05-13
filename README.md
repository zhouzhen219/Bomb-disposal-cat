# Bomb-disposal cat

This project uses CMake and SFML 3.

## Prerequisites

- CMake 3.28 or newer
- A C++17-capable compiler
- MinGW Makefiles on Windows, or another CMake generator supported by your toolchain

The repository already includes SFML in `third_party/SFML-3.0.0`, so a fresh clone should build without any machine-specific paths.

## Build

From the project root:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j 4
```

If your SFML copy is stored somewhere else, pass the root directory explicitly:

```bash
cmake -S . -B build -G "MinGW Makefiles" -DSFML_ROOT="D:/path/to/SFML-3.0.0"
cmake --build build -j 4
```

## Run

The executable is generated at `build/bin/main.exe` on Windows.

```bash
build/bin/main.exe
```

## Notes

- `CMakeLists.txt` uses a configurable `SFML_ROOT` cache variable and does not depend on an absolute local path.
- On Windows, the SFML DLLs are copied from `${SFML_ROOT}/bin` to the executable directory after the build.
