"""
Core components for event-discrete simulation environments.

"""
import types
from heapq import heappush, heappop
from itertools import count

from simpy.events import (AllOf, AnyOf, Event, Process, Timeout, URGENT,
                          NORMAL)


Infinity = float('inf')  #: Convenience alias for infinity


class BoundClass(object):
    """Allows classes to behave like methods.

    The ``__get__()`` descriptor is basically identical to
    ``function.__get__()`` and binds the first argument of the ``cls`` to the
    descriptor instance.

    """
    def __init__(self, cls):
        self.cls = cls

    def __get__(self, obj, type=None):
        if obj is None:
            return self.cls
        return types.MethodType(self.cls, obj)

    @staticmethod
    def bind_early(instance):
        """Bind all :class:`BoundClass` attributes of the *instance's* class
        to the instance itself to increase performance."""
        cls = type(instance)
        for name, obj in cls.__dict__.items():
            if type(obj) is BoundClass:
                bound_class = getattr(instance, name)
                setattr(instance, name, bound_class)


class EmptySchedule(Exception):
    """Thrown by an :class:`Environment` if there are no further events to be
    processed."""
    pass


class StopSimulation(Exception):
    """Indicates that the simulation should stop now."""

    @classmethod
    def callback(cls, event):
        """Used as callback in :meth:`BaseEnvironment.run()` to stop the
        simulation when the *until* event occurred."""
        if event.ok:
            raise cls(event.value)
        else:
            raise event.value


class BaseEnvironment(object):
    """Base class for event processing environments.

    An implementation must at least provide the means to access the current
    time of the environment (see :attr:`now`) and to schedule (see
    :meth:`schedule()`) events as well as processing them (see :meth:`step()`.

    The class is meant to be subclassed for different execution environments.
    For example, SimPy defines a :class:`Environment` for simulations with
    a virtual time and and a :class:`~simpy.rt.RealtimeEnvironment` that
    schedules and executes events in real (e.g., wallclock) time.

    """
    @property
    def now(self):
        """The current time of the environment."""
        raise NotImplementedError(self)

    @property
    def active_process(self):
        """The currently active process of the environment."""
        raise NotImplementedError(self)

    def schedule(self, event, priority=NORMAL, delay=0):
        """Schedule an *event* with a given *priority* and a *delay*.

        There are two default priority values, :data:`~simpy.events.URGENT` and
        :data:`~simpy.events.NORMAL`.

        """
        raise NotImplementedError(self)

    def step(self):
        """Processes the next event."""
        raise NotImplementedError(self)

    def run(self, until=None):
        """Executes :meth:`step()` until the given criterion *until* is met.

        - If it is ``None`` (which is the default), this method will return
          when there are no further events to be processed.

        - If it is an :class:`~simpy.events.Event`, the method will continue
          stepping until this event has been triggered and will return its
          value.  Raises a :exc:`RuntimeError` if there are no further events
          to be processed and the *until* event was not triggered.

        - If it is a number, the method will continue stepping
          until the environment's time reaches *until*.

        """
        if until is not None:
            if not isinstance(until, Event):
                # Assume that *until* is a number if it is not None and
                # not an event.  Create a Timeout(until) in this case.
                at = float(until)

                if at <= self.now:
                    raise ValueError('until(=%s) should be > the current '
                                     'simulation time.' % at)

                # Schedule the event with before all regular timeouts.
                until = Event(self)
                until._ok = True
                until._value = None
                self.schedule(until, URGENT, at - self.now)

            elif until.callbacks is None:
                # Until event has already been processed.
                return until.value

            until.callbacks.append(StopSimulation.callback)

        try:
            while True:
                self.step()
        except StopSimulation as exc:
            return exc.args[0]  # == until.value
        except EmptySchedule:
            if until is not None:
                assert not until.triggered
                raise RuntimeError('No scheduled events left but "until" '
                                   'event was not triggered: %s' % until)

    def exit(self, value=None):
        """Stop the current process, optionally providing a ``value``.

        This is a convenience function provided for Python versions prior to
        3.3. From Python 3.3, you can instead use ``return value`` in
        a process.

        """
        raise StopIteration(value)


class Environment(BaseEnvironment):
    """Execution environment for an event-based simulation. The passing of time
    is simulated by stepping from event to event.

    You can provide an *initial_time* for the environment. By default, it
    starts at ``0``.

    This class also provides aliases for common event types, for example
    :attr:`process`, :attr:`timeout` and :attr:`event`.

    """
    def __init__(self, initial_time=0):
        self._now = initial_time
        self._queue = []  # The list of all currently scheduled events.
        self._eid = count()  # Counter for event IDs
        self._active_proc = None

        # Bind all BoundClass instances to "self" to improve performance.
        BoundClass.bind_early(self)

    @property
    def now(self):
        """The current simulation time."""
        return self._now

    @property
    def active_process(self):
        """The currently active process of the environment."""
        return self._active_proc

    process = BoundClass(Process)
    timeout = BoundClass(Timeout)
    event = BoundClass(Event)
    all_of = BoundClass(AllOf)
    any_of = BoundClass(AnyOf)

    def schedule(self, event, priority=NORMAL, delay=0):
        """Schedule an *event* with a given *priority* and a *delay*."""
        heappush(self._queue,
                 (self._now + delay, priority, next(self._eid), event))

    def peek(self):
        """Get the time of the next scheduled event. Return
        :data:`~simpy.core.Infinity` if there is no further event."""
        try:
            return self._queue[0][0]
        except IndexError:
            return Infinity

    def step(self):
        """Process the next event.

        Raise an :exc:`EmptySchedule` if no further events are available.

        """
        try:
            self._now, _, _, event = heappop(self._queue)
        except IndexError:
            raise EmptySchedule()

        # Process callbacks of the event. Set the events callbacks to None
        # immediately to prevent concurrent modifications.
        callbacks, event.callbacks = event.callbacks, None
        for callback in callbacks:
            callback(event)

        if not event._ok and not hasattr(event, '_defused'):
            # The event has failed and has not been defused. Crash the
            # environment.
            # Create a copy of the failure exception with a new traceback.
            exc = type(event._value)(*event._value.args)
            exc.__cause__ = event._value
            raise exc
