"""
A collection of utility functions:

.. autosummary::
   start_delayed

"""


def start_delayed(env, generator, delay):
    """Return a helper process that starts another process for *generator*
    after a certain *delay*.

    :meth:`~simpy.core.Environment.process()` starts a process at the current
    simulation time. This helper allows you to start a process after a delay of
    *delay* simulation time units::

        >>> from simpy import Environment
        >>> from simpy.util import start_delayed
        >>> def my_process(env, x):
        ...     print('%s, %s' % (env.now, x))
        ...     yield env.timeout(1)
        ...
        >>> env = Environment()
        >>> proc = start_delayed(env, my_process(env, 3), 5)
        >>> env.run()
        5, 3

    Raise a :exc:`ValueError` if ``delay <= 0``.

    """
    if delay <= 0:
        raise ValueError('delay(=%s) must be > 0.' % delay)

    def starter():
        yield env.timeout(delay)
        proc = env.process(generator)
        env.exit(proc)

    return env.process(starter())


def subscribe_at(event):
    """Register at the *event* to receive an interrupt when it occurs.

    The most common use case for this is to pass
    a :class:`~simpy.events.Process` to get notified when it terminates.

    Raise a :exc:`RuntimeError` if ``event`` has already occurred.

    """
    env = event.env
    subscriber = env.active_process

    def signaller(signaller, receiver):
        result = yield signaller
        if receiver.is_alive:
            receiver.interrupt((signaller, result))

    if event.callbacks is not None:
        env.process(signaller(event, subscriber))
    else:
        raise RuntimeError('%s has already terminated.' % event)
