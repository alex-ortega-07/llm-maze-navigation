"""Entry point: runs an agent on the maze through the supervisor.

Usage:
    python src/run.py --width 10 --height 5
    python src/run.py --agent right-hand --width 10 --height 5
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


def main() -> None:
    parser = argparse.ArgumentParser(description="Run an agent on the maze.")
    parser.add_argument("--agent", choices=AGENTS, default="random", help="agent to run (default: random)")
    parser.add_argument("--exe", default="maze.exe", help="path to the maze executable")
    parser.add_argument("-w", "--width", type=int, help="number of columns")
    parser.add_argument("--height", type=int, help="number of rows")
    parser.add_argument("--max-steps", type=int, default=500, help="stop the episode after this many actions")
    args = parser.parse_args()

    maze_args = []
    if args.width is not None:
        maze_args += ["--width", str(args.width)]
    if args.height is not None:
        maze_args += ["--height", str(args.height)]

    connection = MazeConnection(args.exe, maze_args)
    agent: Agent = AGENTS[args.agent]()

    try:
        result = Supervisor(connection, agent, args.max_steps).run_episode()
        print(f"{agent}: reached_exit={result.reached_exit} steps={result.steps}")
    finally:
        connection.close()


if __name__ == "__main__":
    main()
