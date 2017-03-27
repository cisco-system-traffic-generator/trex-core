"""
Resource for sharing homogeneous matter between processes, either continuous
(like water) or discrete (like apples).

A :class:`Container` can be used to model the fuel tank of a gasoline station.
Tankers increase and refuelled cars decrease the amount of gas in the station's
fuel tanks.

"""
from simpy.core import BoundClass
from simpy.resources import base


class ContainerPut(base.Put):
    """Request to put *amount* of matter into the *container*. The request will
    be triggered once there is enough space in the *container* available.

    Raise a :exc:`ValueError` if ``amount <= 0``.

    """
    def __init__(self, container, amount):
        if amount <= 0:
            raise ValueError('amount(=%s) must be > 0.' % amount)
        self.amount = amount
        """The amount of matter to be put into the container."""

        super(ContainerPut, self).__init__(container)


class ContainerGet(base.Get):
    """Request to get *amount* of matter from the *container*. The request will
    be triggered once there is enough matter available in the *container*.

    Raise a :exc:`ValueError` if ``amount <= 0``.

    """
    def __init__(self, container, amount):
        if amount <= 0:
            raise ValueError('amount(=%s) must be > 0.' % amount)
        self.amount = amount
        """The amount of matter to be taken out of the container."""

        super(ContainerGet, self).__init__(container)


class Container(base.BaseResource):
    """Resource containing up to *capacity* of matter which may either be
    continuous (like water) or discrete (like apples). It supports requests to
    put or get matter into/from the container.

    The *env* parameter is the :class:`~simpy.core.Environment` instance the
    container is bound to.

    The *capacity* defines the size of the container. By default, a container
    is of unlimited size. The initial amount of matter is specified by *init*
    and defaults to ``0``.

    Raise a :exc:`ValueError` if ``capacity <= 0``, ``init < 0`` or
    ``init > capacity``.

    """
    def __init__(self, env, capacity=float('inf'), init=0):
        if capacity <= 0:
            raise ValueError('"capacity" must be > 0.')
        if init < 0:
            raise ValueError('"init" must be >= 0.')
        if init > capacity:
            raise ValueError('"init" must be <= "capacity".')

        super(Container, self).__init__(env, capacity)

        self._level = init

    @property
    def level(self):
        """The current amount of the matter in the container."""
        return self._level

    put = BoundClass(ContainerPut)
    """Request to put *amount* of matter into the container."""

    get = BoundClass(ContainerGet)
    """Request to get *amount* of matter out of the container."""

    def _do_put(self, event):
        if self._capacity - self._level >= event.amount:
            self._level += event.amount
            event.succeed()
            return True

    def _do_get(self, event):
        if self._level >= event.amount:
            self._level -= event.amount
            event.succeed()
            return True
