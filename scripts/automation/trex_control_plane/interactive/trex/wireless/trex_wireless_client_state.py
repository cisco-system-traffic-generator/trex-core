from enum import IntEnum


class ClientState(IntEnum):
    """The state of a wireless Client."""
    PROBE = 0
    AUTHENTICATION = 1
    ASSOCIATION = 2
    IP_LEARN = 3
    RUN = 4
    CLOSE = 5
