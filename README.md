# maze-generator-algorithm
An application that generates mazes of a specific size, drawing each step of the generation in real time with SDL2.

The maze is built with a **randomized Prim's algorithm**: starting from a random cell, it repeatedly picks a random frontier cell, connects it to an already visited neighbour by removing the wall between them, and continues until every cell has been visited.

## Requirements
- A C++17 compiler (`g++`)
- [SDL2](https://www.libsdl.org/) and SDL2_ttf
- `make`

### Windows (MSYS2)
Open the **MSYS2 UCRT64** terminal and install:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf
```
To use the tools from PowerShell too, add `C:\msys64\ucrt64\bin` to your `PATH`.

### Linux (Debian/Ubuntu)
```bash
sudo apt install g++ make libsdl2-dev libsdl2-ttf-dev
```

## Build and run
From the project root:

| Action | PowerShell / cmd | MSYS2 / Linux |
| --- | --- | --- |
| Build | `mingw32-make` | `make` |
| Build and run | `mingw32-make run` | `make run` |
| Remove build files | `mingw32-make clean` | `make clean` |

The executable is created as `maze.exe` in the project root, and object files go to `build/`.

## Usage
Once the application is opened, press `ESC` to exit.

Note that the window is **borderless, always on top and hides the mouse cursor**, so `ESC` is the way to close it.

The maze size is set by the `GRID` constant (columns, rows) at the top of `src/main.cpp`.

### Screenshots
https://user-images.githubusercontent.com/85643095/198393196-648f2b0f-da55-4ce7-8859-6fb488ab3045.mp4
