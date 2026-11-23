# Design decisions

This file records the decisions taken in the project and the reasons behind them, so it is clear not only *what* the code does but *why* it is built this way.

To add a decision, append a new entry at the end with the next number, following the same structure: **Decision**, **Why**, and optionally **Details** and **Alternatives considered**.

---

## 1. Explicit Makefile, one rule per file
**Date:** 2026-11-11 · **Commit:** `58d0d75`

**Decision:** Build with a Makefile where every source file has its own written-out rule, instead of generic pattern rules and `wildcard`.

**Why:** The project only has two source files, and an explicit Makefile can be read top to bottom without knowing make's special syntax (`%`, `$<`, `$@`, `wildcard`, `patsubst`). It also only recompiles what changed, and adds `-Wall -Wextra`.

**Alternatives considered:** A compact version using `wildcard` and pattern rules, which picks up new files automatically. It may be worth switching to it if the project grows to many source files.

**Consequence:** Every new `.cpp` file needs its own rule and must be added to the `maze.exe` rule.

## 2. Makefile works from both PowerShell and MSYS2/Linux
**Date:** 2026-11-11 · **Commit:** `58d0d75`

**Decision:** Detect which `make` is running (`MAKE_HOST == Windows32` means `mingw32-make`) and use Windows `cmd` commands there (`if not exist ... mkdir`, `rmdir /s /q`, `del`), and Unix commands (`mkdir -p`, `rm -rf`) otherwise.

**Why:** `mingw32-make` run from PowerShell has no `sh`, `rm` or `mkdir -p`. `make clean` failed, and `mkdir -p build` created a folder literally named `-p`.

**Alternatives considered:** Hard-coding the path to MSYS2's `sh.exe` (machine-specific), or only supporting the MSYS2 terminal.

## 3. Ignore build output in git
**Date:** 2026-11-11 · **Commit:** `8ed0432`

**Decision:** Add a `.gitignore` with `build/` and `maze.exe`.

**Why:** Compiled files can be regenerated at any time and should not be committed.

## 4. `Direction` enum instead of strings in `quit_wall`
**Date:** 2026-11-11 · **Commit:** `93fcf20`

**Decision:** Replace `quit_wall(..., char *wall)` and comparisons like `wall == "right"` with an `enum class Direction { Up, Down, Left, Right }`.

**Why:** Comparing a `char*` with a string literal compares memory addresses, not text. It only worked because the compiler happened to reuse the same literal, and passing a literal to `char*` is not valid in modern C++. An enum is correct by construction and the compiler catches typos.

## 5. Check grid bounds before reading neighbours
**Date:** 2026-11-11 · **Commit:** `5d0e01c`

**Decision:** Move the neighbour logic of `add_visiting` into a `mark_visiting` helper that checks the position is inside the grid **before** reading it.

**Why:** The original code read `grid[y + 1][x]` first and checked the bounds afterwards, reading outside the array on the edges. With a fixed array this went unnoticed; with `std::vector` it can crash. The helper also removed four nearly identical blocks.

## 6. Remove unused code
**Date:** 2026-11-11 · **Commits:** `9089572`, `92bddc4`

**Decision:** Remove an `add_visiting` overload and a `parent_cell` variable that were never used, and make `draw_visiting_cell` actually use its `color` parameter instead of a hard-coded constant.

**Why:** Dead code and ignored parameters make the program harder to understand and hide bugs.

## 7. Maze size from the command line
**Date:** 2026-11-11 · **Commits:** `c3ae08a`, `88b1fe0`

**Decision:** The maze size is chosen when running the program with `-w`/`--width` and `-h`/`--height` (default 20 × 10, allowed range 2–50). `--help` prints the usage. Invalid values or unknown options print an error and exit with code 1, before SDL is initialised.

**Why:** Changing the size should not require editing the code and recompiling.

**Details:**
- The grid became a `std::vector<std::vector<Cell>>` (`Grid`), because a fixed array needs its size at compile time.
- Values are parsed with `strtol`, not `atoi`, so input like `abc` or `20x` is rejected instead of silently becoming `0`.
- The minimum is 2 so that the entrance and exit can be different cells. The maximum of 50 is a sanity limit; with 40 px cells, large values can still produce a window bigger than the screen.

## 8. Entrance and exit on the border
**Date:** 2026-11-12 · **Commit:** `4be8332`

**Decision:** When the maze has finished generating, pick **two different border cells** at random as entrance and exit, and **remove their outer wall** so the opening is visible.

**Why:** It looks like a classic maze and guarantees both ends are on the edge. Choosing them only after generation finishes keeps the generation animation unchanged.

**Details:**
- Corner cells can open on either of their two outer sides.
- A dedicated `open_border_wall` is used instead of `quit_wall`, because `quit_wall` also modifies the neighbouring cell, which does not exist outside the grid.
- The old `cell_start` / `cell_end` code, which was computed at start-up but never drawn, was removed.

**Alternatives considered:** Any cell of the grid, including interior ones, marked only by colour.

## 9. Semi-transparent markers
**Date:** 2026-11-12 · **Commit:** `4be8332`

**Decision:** Draw the entrance (green) and exit (red) as filled squares with alpha 120, enabling `SDL_BLENDMODE_BLEND` only while drawing them.

**Why:** A softer look that lets the background show through. Blending must be enabled for SDL to use the alpha channel; it is switched off afterwards so nothing else is affected.

## 10. Player square moved with WASD
**Date:** 2026-11-12 · **Commit:** `3e70cc8`

**Decision:** Add a separate yellow, smaller player square that starts on the entrance once the maze is finished. It moves one cell per key press with W/A/S/D (holding a key repeats), cannot go through walls, and stops when it reaches the exit, printing `You reached the exit!` to the console.

**Why:** This is the first step towards navigating the maze: it defines the moves (up, down, left, right) and the rules (walls block movement) that an automated agent will later use.

**Details:**
- Keys are read by **scancode** (physical key position), so WASD works the same on any keyboard layout (Spanish, AZERTY…).
- `can_move` also checks the grid bounds, because the entrance and exit have an open outer wall and the player could otherwise walk out of the maze.
- The event loop now processes **every** pending event each frame (`while(SDL_PollEvent(...))`). Before, it read only one event per frame and checked `event.type` even when no event had arrived, so key presses could be lost.
- The player is drawn smaller than the markers so both are visible on the same cell.

**Alternatives considered:** Moving the green entrance marker itself; allowing the player to pass through walls.

## 11. Communication between the environment and the supervisor
**Date:** 2026-11-18 · **Updated:** 2026-11-23 · **Status:** implemented on both sides (`src/supervisor.py` and `src/main.cpp`)

**Decision:** The supervisor launches `maze.exe --agent` as a **subprocess** and both sides communicate through **standard input/output**: the supervisor writes actions to the maze's stdin and reads observations from its stdout. Messages are **JSON, one per line** ([JSON Lines](https://jsonlines.org/)), encoded in UTF-8. Every message has a `type` field that says what kind of message it is. There are two message types:

**`observation`** — environment → supervisor. Sent once the maze has finished generating, and again after every action:
```json
{"type": "observation", "position": [3, 0], "valid_actions": ["up", "right"], "step": 0, "done": false, "last_action_valid": true}
```

| Field | Type | Description |
| --- | --- | --- |
| `position` | `[row, col]` | Current player cell, with `[0, 0]` at the top-left |
| `valid_actions` | list of strings | Moves not blocked by a wall: `"up"`, `"down"`, `"left"`, `"right"` |
| `step` | integer | Number of actions received so far in this episode, including invalid ones |
| `done` | boolean | `true` when the player has reached the exit |
| `last_action_valid` | boolean | `false` if the last action was blocked by a wall or was not a valid action; `true` otherwise (and in the first observation) |
| `exit` | `[row, col]` | *Optional.* Exit cell, only if it is revealed to the agent |
| `grid_size` | `[rows, cols]` | *Optional.* Maze size, only if it is revealed to the agent |

**`action`** — supervisor → environment. Sent after the agent has chosen a move:
```json
{"type": "action", "action": "right"}
```

| Field | Type | Description |
| --- | --- | --- |
| `action` | string | One of `"up"`, `"down"`, `"left"`, `"right"` |

**Invalid actions:** if the action is blocked by a wall, or the message is not a valid action (unknown value, malformed JSON...), the player **does not move**, `step` **still increases by 1**, and the environment replies with a normal observation with `last_action_valid: false`.

**Logs:** in `--agent` mode, stdout is used **only** for JSON messages. Any other output from `maze.exe` (errors, `You reached the exit!`) goes to stderr.

**Implementation:**
- **`maze.exe --agent`** disables the WASD controls. It sends the first observation once the maze has finished generating, and one more after each action it receives.
- **stdin is read in a background thread** (`StdinReader`, in `src/StdinReader.hpp/.cpp`), which puts each line in a queue protected by a mutex. The SDL loop takes the pending lines once per frame, so the window never freezes while waiting for the supervisor.
- **Actions are parsed with a minimal hand-written JSON reader** (`json_string_field` in `src/main.cpp`), not a library. It only handles flat objects with string values and no escaped quotes, which is all the `action` message needs. Observations are written with `std::cout` and flushed with `std::endl` so the supervisor receives them immediately.
- **`maze.exe` exits on its own when stdin is closed**, so it does not stay open if the supervisor stops.
- **`MazeConnection` launches `maze.exe` with an absolute path**, because on Windows Python does not look for a bare executable name in the current directory.
- **Limitations:**
  - At most about one action is applied per frame (60 FPS), so long episodes take several seconds.
  - `exit` and `grid_size` are not sent yet.
  - The supervisor does not use `last_action_valid` yet: `to_observation` drops it and `EpisodeResult` does not count invalid actions.

**Why:**
- **Standard input/output** is the simplest transport: no network code, the supervisor controls the maze's lifetime, and messages can be tested by running `maze.exe --agent` by hand and typing them.
- **JSON** can be read and written easily from both C++ and Python, and is human-readable, so messages can be inspected by hand while debugging.
- **One message per line** makes it trivial to know where a message ends: the receiver just reads a line.
- **The `type` field** allows new message kinds to be added later (e.g. reset) without breaking the existing ones.
- **The fields match `Observation` and `Action` in `src/agents/agent.py`**, so the supervisor can pass the parsed message to the agent with almost no conversion. Coordinates are sent as JSON arrays and become tuples on the Python side.
- **Actions are lowercase strings** (the values of the `Action` enum) rather than numbers or WASD keys, so the messages are self-explanatory and independent of the keyboard controls.
- **Invalid actions cost a step** instead of being rejected, so an agent that keeps choosing impossible moves (e.g. an LLM that ignores the walls) is penalised and cannot get stuck forever: the supervisor's step limit still applies. `last_action_valid` lets the supervisor count these mistakes as a metric.

**Alternatives considered:**
- A TCP socket on `localhost`: more flexible (separate processes, even separate machines) but more complex. It could replace stdin/stdout later by writing another `Connection` in the supervisor.
- For invalid actions: not counting the step (an agent could loop forever on invalid moves), or replying with a separate `error` message (one more message type to handle on both sides).

**Open questions:**
- Whether the agent may see the whole maze, and whether `exit` and `grid_size` are revealed.
