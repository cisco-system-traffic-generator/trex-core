import multiprocessing
import logging

from .broker import PubSubBroker
from .message import PubSubMessage, SubscriptionRequest, Topic, SubscriptionCancelation
from .subscription import Subscription
from .utils import predicate_true

class PubSub:
    """A Publish/Subscribe component."""

    def __init__(self, log_level=logging.DEBUG, log_filter=None, log_queue=None):
        """Create a PubSub.
        
        Args:
            log_queue: the queue to send the logs ('str' of the received messages) to for logging purposes
        """

        # channel on which the publisher will publish messages and the broker read them
        # many to one unidirectional channel
        self.__publish_channel = multiprocessing.Queue()
        # channel on which the PubSubs will communicate with the broker
        # many to one unidirectional channel
        self.__broker_channel = multiprocessing.Queue()
        
        # pubsub message broker
        self.__broker = PubSubBroker(self.__publish_channel, self.__broker_channel, log_level, log_filter, log_queue)
        # process running the broker
        self.__broker_process = None

        self.__running = False

        self.__manager = multiprocessing.Manager()

    def start(self):
        """Start the PubSub system."""
        self.__broker_process = multiprocessing.Process(target=self.__broker.run, args=())
        self.__broker_process.daemon = True
        self.__broker_process.start()
        self.__running = True
    
    def stop(self):
        """Stop the PubSub system."""
        assert(self.__running)
        self.__broker_channel.put(None)
        self.__publish_channel.close()
        self.__broker_channel.close()
        self.__broker_process.join()
        self.__running = False

    def publish(self, message, topics):
        """Publish a message on the PubSub.
        
        Args:
            message: message (any serializable object) to publish tagged the topics of the TopicPublisher.
            topics: topics -- sequence of topics (sequence of strings) or string representing the topics separated by periods.
                e.g. ['module', 'submodule', 'event'] or 'module.submodule.event' (equivalent)
        """
        pubsub_message = PubSubMessage(topics=topics, value=message)
        self.__publish_channel.put(pubsub_message)

    def subscribe(self, topics, predicate=predicate_true):
        """Subscribe to a topics, returning a queue for listening to subscribed messages.

        Returns a Subscription (queue) for receiving the messages, and a subscription id for unsubscribing later.
        
        Args:
            topics: sequence of topics (sequence of strings) or string representing the topics separated by periods.
                e.g. ['module', 'submodule', 'event'] or 'module.submodule.event' (equivalent)
            predicate: function taking a message as argument and returning True or False, False resulting to the message being dropped 
                only functions defined at the top level of a module accepted
        """
        new_channel = self.__manager.Queue()
        if isinstance(topics, Topic):
            topic = topics
        else:
            topic = Topic(topics)
        self.__subscribe(new_channel, topic, predicate)
        subscription_id = new_channel.get()
        return Subscription(self, new_channel, subscription_id)

    def __subscribe(self, channel, topics, predicate):
        """Send a subscribe command to the PubSubBroker for it to register the subscribe.
        
        Args:
            channel: queue to send the messages to
            topics: topics to subscribe to
            predicate: predicate that message should verify to be sent
                must be a one parameter function that returns a boolean
                e.g. lambda x: "myString" in x
        """
        self.__broker_channel.put(SubscriptionRequest(channel, topics, predicate))

    def unsubscribe(self, subscription):
        """Unsubscribe to a previously subscribed set of messages.

        Args:
            subscription: the subscription to unsubscribe
        """
        if not subscription._valid:
            raise ValueError("a Subscription must not be unsubscribed more than once")
        subscription._valid = False
        self.__unsubscribe(subscription)

    def __unsubscribe(self, subscription):
        """Send an unsubscribe command to the PubSubBroker for it to deregister the subscribe.

        Args:
            subscription: the subscription to unsubscribe
        """
        self.__broker_channel.put(SubscriptionCancelation(subscription))

    def Publisher(self, prefix_topics=None):
        """Construct a Publisher, with default topics.
        Every message published will have its topics prefixed by given topics here.

        Args:
            pubsub: pubsub component
            prefix_topics: prefix topics
        """
        return Publisher(self, prefix_topics=prefix_topics)

class Publisher:
    """A publisher in PubSub. Has the 'publish' method."""

    def __init__(self, pubsub, prefix_topics=[]):
        """Construct a Publisher, with default topics.
        Every message published will have its topics prefixed by given topics here.

        Args:
            pubsub: pubsub component
            prefix_topics: prefix topics
        """
        self.__pubsub = pubsub
        if isinstance(prefix_topics, Topic):
            self.__prefix_topics = prefix_topics
        else:
            self.__prefix_topics = Topic(prefix_topics)
    
    def publish(self, message, suffix_topics=""):
        """Publish a message on the PubSub.

        Args:
            message: message (any serializable object) to publish tagged the topics of the TopicPublisher.
            suffix_topics: sequence of topics (sequence of strings) or string representing the topics separated by periods, that will be appended to the prefix topics of the Publisher.
                e.g. ['module', 'submodule', 'event'] or 'module.submodule.event' (equivalent)
        """
        self.__pubsub.publish(message, self.__prefix_topics + Topic(suffix_topics))

    def SubPublisher(self, prefix_topics=[]):
        """Construct a new Publisher that has topic prefixes of parent, and other subtopics.

        Args:
            prefix_topics: topics to add after the topics of parent
        """
        return Publisher(self.__pubsub, self.__prefix_topics + Topic(prefix_topics))
