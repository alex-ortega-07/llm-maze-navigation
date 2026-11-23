# llm-maze-navigation
A research project on maze navigation by AI agents. A C++/SDL2 application generates a random maze, drawing each step of the generation in real time, and the maze can then be solved either by hand (WASD) or by an agent written in Python.

The maze is built with a **randomized Prim's algorithm**: starting from a random cell, it repeatedly picks a random frontier cell, connects it to an already visited neighbour by removing the wall between them, and continues until every cell has been visited.

## Architecture
The system has three parts:

| Part | File | Role |
| --- | --- | --- |
| **Environment** | `src/main.cpp` (`maze.exe`) | Generates and draws the maze, and owns the player position |
| **Supervisor** | `src/supervisor.py` | Receives the state from the environment, asks the agent for a move and sends it back |
| **Agent** | `src/agents/` | Receives an observation and chooses the next move |

The supervisor launches `maze.exe --agent` as a subprocess, and both sides exchange JSON messages, one per line, over standard input/output. The message format is described in [DECISIONS.md](DECISIONS.md) (decision 11).

## Requirements
- A C++17 compiler (`g++`)
- [SDL2](https://www.libsdl.org/) and SDL2_ttf
- `make`
- Python 3.11 or newer, to run agents (no external packages needed)

### Windows (MSYS2)
Open the **MSYS2 UCRT64** terminal and install:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf
```
To use the tools from PowerShell too, add `C:\msys64\ucrt64\bin` to your `PATH`.

> Run `maze.exe` from PowerShell or the MSYS2 UCRT64 terminal. Git Bash puts its own C++ runtime first in the `PATH`, and `maze.exe` fails to start there.

### Linux (Debian/Ubuntu)
```bash
sudo apt install g++ make libsdl2-dev libsdl2-ttf-dev python3
```

## Build
From the project root:

| Action | PowerShell / cmd | MSYS2 / Linux |
| --- | --- | --- |
| Build | `mingw32-make` | `make` |
| Build and run | `mingw32-make run` | `make run` |
| Remove build files | `mingw32-make clean` | `make clean` |

The executable is created as `maze.exe` in the project root, and object files go to `build/`.

## Playing manually
Run `maze.exe` (or `mingw32-make run`). Once the maze is fully generated, an **entrance** (green) and an **exit** (red) are picked at random on two different border cells, and their outer walls are opened. A yellow **player** square then appears on the entrance.

### Controls
| Key | Action |
| --- | --- |
| `W` / `A` / `S` / `D` | Move the player up / left / down / right |
| `ESC` | Exit |

The player moves one cell per key press (hold the key to keep moving) and cannot go through walls. When it reaches the exit, `You reached the exit!` is printed to the console and the player stops moving.

Note that the window is **borderless, always on top and hides the mouse cursor**, so `ESC` is the way to close it.

### Options
The maze size can be set from the command line (default: 20 columns × 10 rows, allowed range 2–50):

```bash
maze.exe -w 30 -h 15
maze.exe --width 30 --height 15
maze.exe --help
```

| Option | Description |
| --- | --- |
| `-w`, `--width <n>` | Number of columns |
| `-h`, `--height <n>` | Number of rows |
| `--agent` | Agent mode: read actions from stdin and write observations to stdout; WASD is disabled. Used by the supervisor |
| `--help` | Show the help and exit |

Each cell is 40 px wide, so large values can make the window bigger than the screen.

## Running an agent
Build `maze.exe` first, then run from the project root:

```bash
python src/run.py --width 10 --height 5
```

This launches `maze.exe` in agent mode and lets the agent play one episode. At the end it prints whether the agent reached the exit and how many steps it took, for example:

```
RandomAgent(name='RandomAgent'): reached_exit=True steps=20
```

| Option | Description |
| --- | --- |
| `--exe <path>` | Path to the maze executable (default `maze.exe`) |
| `-w`, `--width <n>` | Number of columns |
| `--height <n>` | Number of rows (there is no `-h`, which shows the help) |
| `--max-steps <n>` | Stop the episode after this many actions (default 500) |

The episode currently uses `RandomAgent`, which picks a random move among the ones not blocked by a wall. It is meant to test the whole system and as a baseline for other agents.

### Writing a new agent
Create a module in `src/agents/` with a subclass of `Agent` (from `src/agents/agent.py`) that implements `act`:

```python
from agents.agent import Action, Agent, Observation

class MyAgent(Agent):
    def act(self, obs: Observation) -> Action:
        ...
```

`reset()` (called at the start of each episode) and `update()` (called after each action) can be overridden to keep a memory between steps.

## Project structure
```
├── src/
│   ├── main.cpp            # Environment: maze generation, drawing, WASD and agent mode
│   ├── Cell.hpp/.cpp       # A maze cell and its walls
│   ├── StdinReader.hpp/.cpp# Reads stdin in a background thread (agent mode)
│   ├── supervisor.py       # Connects maze.exe with an agent
│   ├── run.py              # Entry point to run an episode
│   └── agents/
│       ├── agent.py        # Abstract Agent, Action and Observation
│       └── random_agent.py # RandomAgent
├── DECISIONS.md            # Design decisions and the reasons behind them
└── Makefile
```

## Screenshots
https://user-images.githubusercontent.com/85643095/198393196-648f2b0f-da55-4ce7-8859-6fb488ab3045.mp4
