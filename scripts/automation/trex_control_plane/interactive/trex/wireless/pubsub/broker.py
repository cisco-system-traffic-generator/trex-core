import select

from .message import SubscriptionRequest, PubSubMessage, SubscriptionCancelation
from ..logger import get_queue_logger

import os
import logging

class PubSubBroker:
    """A Message Broker in a PubSub.
    Receives all PubSubMessages from Publishers, and has to filter and forward to subscribed Subscribers.
    """

    def __init__(self, publish_channel, broker_channel, log_level=logging.DEBUG, log_filter=None, log_queue=None):
        """Construct a PubSubBroker.

        Args:
            publish_channel: channel for receiving pubsub messages
                a (synchronized) queue attached to a Broker that has a 'get' method.
                many to one unidirectional channel
            broker_channel: channel for receiving subscribe/unsubscribe commands
                a (synchronized) queue attached to a Broker that hsa a 'get' method
                many to one unidirectional channel
            log_queue: if specified, the queue on to send log messages (as 'str')
        """
        self.__log_queue = log_queue
        self.__log_level = log_level
        self.__log_filter = log_filter
        self.logger = None

        self.__publish_channel = publish_channel
        self.__broker_channel = broker_channel

        # map topics -> subscriptions subscribed to this topics
        # used when subscribing and when looking up subscriber for a message
        self.__topics_to_subscriptions = {}
        # map subscription id -> subscription of this channel
        # used when unsubscribing
        self.__sub_id_to_subscription = {}

        # a subscription id is sent back to the subscriber when a subscribe is requested
        # this is used for identifying the subsciption, used when unsubscribing
        self.__next_subscription_id = 0

    def run(self):

        def init():
            # clearing handlers
            handlers = list(logging.getLogger().handlers)
            for h in handlers:
                logging.getLogger().removeHandler(h)
            if self.__log_queue:
                self.logger = get_queue_logger(self.__log_queue, "PubSub", "PubSub", self.__log_level, self.__log_filter)
                self.logger.info("started, PID %s" % os.getpid())

        init()

        try:
            while True:
                readable, _, _ = select.select([self.__broker_channel._reader, self.__publish_channel._reader],[], [])
                if self.__broker_channel._reader in readable:
                    msg = self.__broker_channel.get()

                    if msg is None:
                        # signal to stop
                        return

                    sub_unsub = msg
                    if isinstance(sub_unsub, SubscriptionRequest):
                        # Subscription Request
                        sub = sub_unsub
                        self.__register(sub)
                    elif isinstance(sub_unsub, SubscriptionCancelation):
                        # Subscription Cancelation
                        unsub = sub_unsub
                        self.__deregister(unsub)
                    else:
                        raise TypeError("SubscriptionRequest or SubscriptionCancelation expected")

                if self.__publish_channel._reader in readable:
                    pubsub_message = self.__publish_channel.get()
                    if not isinstance(pubsub_message, PubSubMessage):
                        raise TypeError("PubSubMessage")
                    # log
                    if self.logger:
                        self.logger.debug(pubsub_message.__repr__())

                    self.__publish(pubsub_message)
        except KeyboardInterrupt:
            self.__broker_channel.close()
        except EOFError:
            # stop
            pass

    def __publish(self, pubsub_message):
        """Process a pubsub message, dispatching it to all matched subscribers."""
        for sub in self.__subscribers(pubsub_message):
            sub.channel.put(pubsub_message)

    def __register(self, sub):
        """Register a subscription.

        Args:
            sub: SubscriptionRequest message
        """
        sub_id = self.__new_subscription_id()

        self.__sub_id_to_subscription[sub_id] = sub

        register_sub(self.__topics_to_subscriptions, sub)

        # send back the subscription id
        sub.channel.put(sub_id)

    def __deregister(self, unsub):
        """Deregister a subscription.

        Args:
            unsub: SubscriptionCancelation message
        """
        
        assert(unsub.subscription_id in self.__sub_id_to_subscription)

        sub = self.__sub_id_to_subscription[unsub.subscription_id]
        del self.__sub_id_to_subscription[unsub.subscription_id]
        deregister_sub(self.__topics_to_subscriptions, sub)

    def __subscribers(self, pubsub_message):
        """Return (a generator yielding) the Subscriptions that subscribed to the 'pubsub_message'."""
        yield from subscribers(self.__topics_to_subscriptions, pubsub_message)

    def __new_subscription_id(self):
        self.__next_subscription_id += 1
        return self.__next_subscription_id

def register_sub(topics_to_subs, sub):
    """Register a Subscription into a mapping topics->subscriptions.
    
    Args:
        topics_to_subs (dict): dictionnary topic -> list of subscriptions
        sub (Subscription): subscription to register
    """
    key = str(sub.topics)
    if key not in topics_to_subs:
        topics_to_subs[key] = []
    topics_to_subs[key].append(sub)
    return topics_to_subs

def deregister_sub(topics_to_subs, sub):
    """Deregister a Subscription from a mapping topics->subscriptions.

    Args:
        topics_to_subs (dict): dictionnary topic -> list of subscriptions
        sub (Subscription): subscription to deregister
    """
    key = str(sub.topics)
    if key in topics_to_subs:
        return topics_to_subs[str(sub.topics)].remove(sub)
    else:
        return topics_to_subs

def subscribers(topics_to_subs, pubsub_message):
    """Generator yielding the subscripbers of a message.
    
    Args:
        topics_to_subs (dict): dictionnary topic -> list of subscriptions constructed using function 'register_sub'
        pubsub_message (PubSubMessage): message
    """
    for suptopic in pubsub_message.suptopics:
        key = str(suptopic)
        if key in topics_to_subs:
            for sub in topics_to_subs[key]:
                if pubsub_message.match_predicate(sub.predicate):
                    yield sub
