import random

from agents.agent import Action, Agent, Observation


class RandomAgent(Agent):
    """Picks a random move among the ones not blocked by a wall.

    Useful to test the whole system end to end and as a baseline to compare
    other agents against.
    """

    def act(self, obs: Observation) -> Action:
        return random.choice(obs["valid_actions"])
