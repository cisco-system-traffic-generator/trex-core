"""
Shared resources for storing a possibly unlimited amount of objects supporting
requests for specific objects.

The :class:`Store` operates in a FIFO (first-in, first-out) order. Objects are
retrieved from the store in the order they were put in. The *get* requests of a
:class:`FilterStore` can be customized by a filter to only retrieve objects
matching a given criterion.

"""
from heapq import heappush, heappop
from collections import namedtuple

from simpy.core import BoundClass
from simpy.resources import base


class StorePut(base.Put):
    """Request to put *item* into the *store*. The request is triggered once
    there is space for the item in the store.

    """
    def __init__(self, store, item):
        self.item = item
        """The item to put into the store."""
        super(StorePut, self).__init__(store)


class StoreGet(base.Get):
    """Request to get an *item* from the *store*. The request is triggered
    once there is an item available in the store.

    """
    pass


class FilterStoreGet(StoreGet):
    """Request to get an *item* from the *store* matching the *filter*. The
    request is triggered once there is such an item available in the store.

    *filter* is a function receiving one item. It should return ``True`` for
    items matching the filter criterion. The default function returns ``True``
    for all items, which makes the request to behave exactly like
    :class:`StoreGet`.

    """
    def __init__(self, resource, filter=lambda item: True):
        self.filter = filter
        """The filter function to filter items in the store."""
        super(FilterStoreGet, self).__init__(resource)


class Store(base.BaseResource):
    """Resource with *capacity* slots for storing arbitrary objects. By
    default, the *capacity* is unlimited and objects are put and retrieved from
    the store in a first-in first-out order.

    The *env* parameter is the :class:`~simpy.core.Environment` instance the
    container is bound to.

    """
    def __init__(self, env, capacity=float('inf')):
        if capacity <= 0:
            raise ValueError('"capacity" must be > 0.')

        super(Store, self).__init__(env, capacity)

        self.items = []
        """List of the items available in the store."""

    put = BoundClass(StorePut)
    """Request to put *item* into the store."""

    get = BoundClass(StoreGet)
    """Request to get an *item* out of the store."""

    def _do_put(self, event):
        if len(self.items) < self._capacity:
            self.items.append(event.item)
            event.succeed()

    def _do_get(self, event):
        if self.items:
            event.succeed(self.items.pop(0))


class PriorityItem(namedtuple('PriorityItem', 'priority item')):
    """Wrap an arbitrary *item* with an orderable *priority*.

    Pairs a *priority* with an arbitrary *item*. Comparisons of *PriorityItem*
    instances only consider the *priority* attribute, thus supporting use of
    unorderable items in a :class:`PriorityStore` instance.

    """

    def __lt__(self, other):
        return self.priority < other.priority


class PriorityStore(Store):
    """Resource with *capacity* slots for storing objects in priority order.

    Unlike :class:`Store` which provides first-in first-out discipline,
    :class:`PriorityStore` maintains items in sorted order such that
    the smallest items value are retreived first from the store.

    All items in a *PriorityStore* instance must be orderable; which is to say
    that items must implement :meth:`~object.__lt__()`. To use unorderable
    items with *PriorityStore*, use :class:`PriorityItem`.

    """

    def _do_put(self, event):
        if len(self.items) < self._capacity:
            heappush(self.items, event.item)
            event.succeed()

    def _do_get(self, event):
        if self.items:
            event.succeed(heappop(self.items))


class FilterStore(Store):
    """Resource with *capacity* slots for storing arbitrary objects supporting
    filtered get requests. Like the :class:`Store`, the *capacity* is unlimited
    by default and objects are put and retrieved from the store in a first-in
    first-out order.

    Get requests can be customized with a filter function to only trigger for
    items for which said filter function returns ``True``.

    .. note::

        In contrast to :class:`Store`, get requests of a :class:`FilterStore`
        won't necessarily be triggered in the same order they were issued.

        *Example:* The store is empty. *Process 1* tries to get an item of type
        *a*, *Process 2* an item of type *b*. Another process puts one item of
        type *b* into the store. Though *Process 2* made his request after
        *Process 1*, it will receive that new item because *Process 1* doesn't
        want it.

    """

    put = BoundClass(StorePut)
    """Request a to put *item* into the store."""

    get = BoundClass(FilterStoreGet)
    """Request a to get an *item*, for which *filter* returns ``True``, out of
    the store."""

    def _do_get(self, event):
        for item in self.items:
            if event.filter(item):
                self.items.remove(item)
                event.succeed(item)
                break
        return True
