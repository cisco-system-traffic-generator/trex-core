"""Execution environment for events that synchronizes passing of time
with the real-time (aka *wall-clock time*).

"""
try:
    # Python >= 3.3
    from time import monotonic as time, sleep
except ImportError:
    # Python < 3.3
    from time import time, sleep

from simpy.core import Environment, EmptySchedule, Infinity


class RealtimeEnvironment(Environment):
    """Execution environment for an event-based simulation which is
    synchronized with the real-time (also known as wall-clock time). A time
    step will take *factor* seconds of real time (one second by default).
    A step from ``0`` to ``3`` with a ``factor=0.5`` will, for example, take at
    least
    1.5 seconds.

    The :meth:`step()` method will raise a :exc:`RuntimeError` if a time step
    took too long to compute. This behaviour can be disabled by setting
    *strict* to ``False``.

    """
    def __init__(self, initial_time=0, factor=1.0, strict=True):
        Environment.__init__(self, initial_time)

        self.env_start = initial_time
        self.real_start = time()
        self._factor = factor
        self._strict = strict

    @property
    def factor(self):
        """Scaling factor of the real-time."""
        return self._factor

    @property
    def strict(self):
        """Running mode of the environment. :meth:`step()` will raise a
        :exc:`RuntimeError` if this is set to ``True`` and the processing of
        events takes too long."""
        return self._strict

    def sync(self):
        """Synchronize the internal time with the current wall-clock time.

        This can be useful to prevent :meth:`step()` from raising an error if
        a lot of time passes between creating the RealtimeEnvironment and
        calling :meth:`run()` or :meth:`step()`.

        """
        self.real_start = time()

    def step(self):
        """Process the next event after enough real-time has passed for the
        event to happen.

        The delay is scaled according to the real-time :attr:`factor`. With
        :attr:`strict` mode enabled, a :exc:`RuntimeError` will be raised, if
        the event is processed too slowly.

        """
        evt_time = self.peek()

        if evt_time is Infinity:
            raise EmptySchedule()

        real_time = self.real_start + (evt_time - self.env_start) * self.factor

        if self.strict and time() - real_time > self.factor:
            # Events scheduled for time *t* may take just up to *t+1*
            # for their computation, before an error is raised.
            raise RuntimeError('Simulation too slow for real time (%.3fs).' % (
                time() - real_time))

        # Sleep in a loop to fix inaccuracies of windows (see
        # http://stackoverflow.com/a/15967564 for details) and to ignore
        # interrupts.
        while True:
            delta = real_time - time()
            if delta <= 0:
                break
            sleep(delta)

        return Environment.step(self)
