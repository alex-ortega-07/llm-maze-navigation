"""Entry point: runs an agent on the maze through the supervisor.

Usage:
    python src/run.py --width 10 --height 5
    python src/run.py --agent right-hand --width 10 --height 5
    python src/run.py --agent right-hand --width 10 --height 5 --seed 42
"""

import argparse

from agents.agent import Agent
from agents.random_agent import RandomAgent
from agents.wall_follower_agent import WallFollowerAgent
from supervisor import MazeConnection, Supervisor

# Agents that can be chosen with --agent
AGENTS = {
    "random": lambda: RandomAgent(),
    "right-hand": lambda: WallFollowerAgent("right"),
    "left-hand": lambda: WallFollowerAgent("left"),
}

MAX_SEED = 2**32 - 1  # maze.exe stores the seed in an unsigned int


def seed_type(text: str) -> int:
    """argparse type: an integer between 0 and MAX_SEED, checked here so errors appear before maze.exe starts."""
    try:
        value = int(text)
    except ValueError:
        raise argparse.ArgumentTypeError(f"'{text}' is not an integer")

    if not 0 <= value <= MAX_SEED:
        raise argparse.ArgumentTypeError(f"must be between 0 and {MAX_SEED}")

    return value


def main() -> None:
    parser = argparse.ArgumentParser(description="Run an agent on the maze.")
    parser.add_argument("--agent", choices=AGENTS, default="random", help="agent to run (default: random)")
    parser.add_argument("--exe", default="maze.exe", help="path to the maze executable")
    parser.add_argument("-w", "--width", type=int, help="number of columns")
    parser.add_argument("--height", type=int, help="number of rows")
    parser.add_argument("--max-steps", type=int, default=500, help="stop the episode after this many actions")
    parser.add_argument("--seed", type=seed_type, help="maze seed; the same seed and size give the same maze")
    args = parser.parse_args()

    maze_args = []
    if args.width is not None:
        maze_args += ["--width", str(args.width)]
    if args.height is not None:
        maze_args += ["--height", str(args.height)]
    if args.seed is not None:
        maze_args += ["--seed", str(args.seed)]

    connection = MazeConnection(args.exe, maze_args)
    agent: Agent = AGENTS[args.agent]()

    try:
        result = Supervisor(connection, agent, args.max_steps).run_episode()
        print(f"{agent}: reached_exit={result.reached_exit} steps={result.steps}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
