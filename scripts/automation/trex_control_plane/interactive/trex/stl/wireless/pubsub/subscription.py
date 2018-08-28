class Subscription:
    """A Subscription represents a subscription in PubSub.
    It is a wrapper on a queue, which purpose is to abstact deregistration of the Subscription using the subscription_id.
    """

    def __init__(self, pubsub, queue, subscription_id):
        """Construct a Subscription.

        Args:
            pubsub: pubsub component
            queue: subscription's queue (the queue where subscribed messages can be read from),
            subscription_id: id of the subscription, given by the Broker
        """
        self.__pubsub = pubsub
        self._queue = queue
        self._id = subscription_id
        self.get = self._queue.get # other method of queue should not be exported
        self.get_nowait = self._queue.get_nowait # other method of queue should not be exported
        self._valid = True

    def unsubscribe(self):
        self.__pubsub.unsubscribe(self)

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.unsubscribe()
        return