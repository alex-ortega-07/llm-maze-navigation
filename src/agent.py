"""Base interface for maze-navigation agents.

The system has three parts:
    - Environment (maze.cpp): owns the maze and the player position.
    - Supervisor: reads the state from the environment, builds an observation,
      asks the agent for an action and applies it back to the environment.
    - Agent (this module): receives an observation and returns an action.

The agent never talks to the environment directly; it only sees what the
supervisor puts in the observation.
"""

from abc import ABC, abstractmethod
from enum import Enum
from typing import NotRequired, TypedDict


class Action(str, Enum):
    """A move of one cell."""

    UP = "up"
    DOWN = "down"
    LEFT = "left"
    RIGHT = "right"


class Observation(TypedDict):
    """What the supervisor gives the agent at each step.

    Positions are (row, col), with (0, 0) at the top-left cell.
    """

    position: tuple[int, int]
    """Current player cell."""

    valid_actions: list[Action]
    """Moves that are not blocked by a wall."""

    step: int
    """Number of actions taken so far in this episode."""

    exit: NotRequired[tuple[int, int]]
    """Exit cell, only if the supervisor chooses to reveal it."""

    grid_size: NotRequired[tuple[int, int]]
    """Maze size as (rows, cols), only if the supervisor chooses to reveal it."""


class Agent(ABC):
    """Abstract agent. Subclasses must implement `act`."""

    def __init__(self, name: str | None = None) -> None:
        self.name = name or type(self).__name__

    def reset(self) -> None:
        """Called by the supervisor at the start of every episode (new maze).

        Override it to clear any memory kept between steps.
        """

    @abstractmethod
    def act(self, obs: Observation) -> Action:
        """Choose the next move for the given observation."""

    def update(self, obs: Observation, action: Action, next_obs: Observation, done: bool) -> None:
        """Called by the supervisor after an action has been applied.

        Optional hook for agents that learn or keep a memory of the path
        (e.g. visited cells, conversation history for an LLM). Does nothing
        by default.
        """

    def __repr__(self) -> str:
        return f"{type(self).__name__}(name={self.name!r})"
