"""Supervisor: connects the maze environment with an agent.

Loop for one episode:
    1. Receive an observation from the environment.
    2. Ask the agent for an action.
    3. Send the action back to the environment.
    4. Repeat until the player reaches the exit or the step limit is hit.

Messages follow the JSON Lines format described in DECISIONS.md (decision 11).
"""

import json
import os
import subprocess
from dataclasses import dataclass
from typing import Protocol

from agents.agent import Action, Agent, Observation


class Connection(Protocol):
    """Anything the supervisor can exchange messages with (real maze.exe, a fake maze for tests...)."""

    def send(self, message: dict) -> None: ...
    def receive(self) -> dict: ...
    def close(self) -> None: ...


class MazeConnection:
    """Runs maze.exe as a subprocess and talks to it through stdin/stdout, one JSON message per line."""

    def __init__(self, exe_path: str, args: list[str] | None = None) -> None:
        # Absolute path: on Windows, Python does not look for a bare "maze.exe" in the current directory
        self.process = subprocess.Popen(
            [os.path.abspath(exe_path), "--agent", *(args or [])],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            text=True,
            bufsize=1,  # line-buffered
        )

    def send(self, message: dict) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(json.dumps(message) + "\n")
        self.process.stdin.flush()

    def receive(self) -> dict:
        assert self.process.stdout is not None
        line = self.process.stdout.readline()
        if not line:
            raise ConnectionError("maze.exe closed the connection")
        return json.loads(line)

    def close(self) -> None:
        if self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=5)


def to_observation(message: dict) -> Observation:
    """Converts an `observation` message into the Observation the agent expects."""
    if message.get("type") != "observation":
        raise ValueError(f"Expected an observation message, got: {message}")

    obs: Observation = {
        "position": tuple(message["position"]),
        "valid_actions": [Action(a) for a in message["valid_actions"]],
        "step": message["step"],
    }

    # Only added if previously defined
    if "exit" in message:
        obs["exit"] = tuple(message["exit"])

    if "grid_size" in message:
        obs["grid_size"] = tuple(message["grid_size"])

    return obs


@dataclass
class EpisodeResult:
    reached_exit: bool
    steps: int


class Supervisor:
    def __init__(self, connection: Connection, agent: Agent, max_steps: int = 500) -> None:
        self.connection = connection
        self.agent = agent
        self.max_steps = max_steps

    def run_episode(self) -> EpisodeResult:
        self.agent.reset()

        message = self.connection.receive()
        obs = to_observation(message)
        done = message["done"]

        while not done and obs["step"] < self.max_steps:
            action = self.agent.act(obs)
            self.connection.send({"type": "action", "action": action.value})

            message = self.connection.receive()
            next_obs = to_observation(message)
            done = message["done"]

            self.agent.update(obs, action, next_obs, done)
            obs = next_obs

        return EpisodeResult(reached_exit=done, steps=obs["step"])
