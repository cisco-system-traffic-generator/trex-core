import time
import simpy
from abc import ABC

from ..pubsub.message import Topic

class WirelessServiceEventSubscription(ABC):
    """A WirelessServiceEventSubscription represents a Subscription on a WirelessServiceEvent.
    This is a one time subscription and is deregistered once a matching event is received.
    """

    def __init__(self, event):
        """Create a WirelessServiceEventSubscription from a WirelessServiceEvent.
        
        Args:
            event (WirelessServiceEvent): event to subscribe to
        """
        self.event = event
        self.topics = Topic(event.topics)
        self.suptopics = self.topics.suptopics

    def trigger(self):
        """Trigger the Subscription's event."""
        self.event.trigger()

    def predicate(self, x):
        return True


class WirelessServiceEvent(ABC):
    """A WirelessServiceEvent is a description of an event on a simulated wireless device,
    with a simpy event, the actual event used to wake up a coroutine in simpy's run loop.
    """

    def __init__(self, env, device, service, value):
        """Create an Event.

        Keywords Arguments:
        env -- the simpy encironment where the event should happen
        device -- the wireless device identifier (mac address) where the event happened, as a string
        service -- where the event come from, the service name as a string
        value -- the value of the event, determined by the service
        """
        self.device = device
        self.service = service
        self.value = value
        self.simpy_event = simpy.events.Event(env)
        self.env = env
        self.topics = "WirelessServiceEvent.{}.{}".format(self.device, self.service)

    def __repr__(self):
        return "Device: {} Service: {} Event: {}".format(self.device, self.service, self.value.__repr__())

    def __eq__(self, other):
        if not isinstance(self, other.__class__):
            return False
        return (self.device == other.device and self.service == other.service and self.value == other.value)

    def predicate(self):
        """Return the corresponding predicate of the event for use in PubSub."""
        def p(x):
            return self == x
        return p

    def trigger(self):
        """Trigger the simpy event, to be used when a corresponding PubSub event has been received."""
        self.simpy_event.succeed()