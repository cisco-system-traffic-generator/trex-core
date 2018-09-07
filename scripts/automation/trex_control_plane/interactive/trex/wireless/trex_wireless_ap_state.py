from enum import IntEnum


class APState(IntEnum):
    """The state of an AP."""
    INIT, DISCOVER, DTLS, JOIN, RUN, CLOSING, CLOSED = range(7)
