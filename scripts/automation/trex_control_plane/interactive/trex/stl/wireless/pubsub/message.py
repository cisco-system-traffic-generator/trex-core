import collections
import time
import datetime

from .utils import topics_as_list


class Topic:
    """A Topic is a hierarchical list of topics."""

    def __init__(self, topics):
        """Construct a Topic

        Args:
            topics: sequence of topics (sequence of strings) or string representing the topics separated by periods.
                e.g. ['module', 'submodule', 'event'] or 'module.submodule.event' (equivalent)
        """
        self.__list = topics_as_list(topics)

    def __str__(self):
        return '.'.join(self.__list)

    def __contains__(self, other):
        """Return True if 'other' is a sub Topic of this Topic.

        Examples:
        >>> Topic('module.submodule').__contains__(Topic('module.submodule.specific'))
        True
        >>> Topic('module.submodule.specific').__contains__(Topic('module.submodule'))
        False
        >>> Topic('').__contains__(Topic('something'))
        True
        """
        return other.__list[:len(self.__list)] == self.__list

    def __add__(self, other):
        return Topic(self.__list + other.__list)

    def suptopics(self):
        """Iterator of all Topic t that satisfy to 'self in t'."""
        for i in range(len(self.__list) + 1):
            yield Topic(self.__list[:i])


class PubSubMessage:
    """A Message that can be published.
    Is tagged with topics, that should be hierarchical.
    The value of a PubSubMessage can be anything serializable.
    """
    __slots__ = ['__topics', '__value', 'timestamp']

    def __init__(self, value, topics):
        """Construct a PubSubMessage from a list of topics and a value associated with this message.

        Args:
            value: a serializable object representing the PubSubMessage value
            topics: sequence of topics (sequence of strings) or string representing the topics separated by periods.
                e.g. ['module', 'submodule', 'event'] or 'module.submodule.event' (equivalent)
        """
        if isinstance(topics, Topic):
            self.__topics = topics
        else:
            self.__topics = Topic(topics)
        self.__value = value
        self.timestamp = time.time()

    @property
    def value(self):
        return self.__value

    def __str__(self):
        """Return a string summary of the PubSubMessage without the time."""
        return "{} | {}".format(str(self.__topics), str(self.__value))

    def __repr__(self):
        """Return the representation of the PubSubMessage."""
        time_str = datetime.datetime.fromtimestamp(self.timestamp)
        return "{} | {} | {}".format(str(time_str), str(self.__topics), str(self.__value))

    @property
    def topics(self):
        """Return the topics of the Message in string period separated format."""
        return str(self.__topics)

    @property
    def suptopics(self):
        """Iterator of all Topic t that satisfy to 'topic in t'."""
        return self.__topics.suptopics()

    def match_predicate(self, predicate):
        """Return True if 'predicate' is True for this Message.

        Args:
            predicate: a predicate for a PubSubMessage's value.

        Examples:
        >>> PubSubMessage("hello there", 'module.submodule').match_predicate(lambda x: True)
        True
        >>> PubSubMessage("hello there", 'module.submodule').match_predicate(lambda x: "hello" in x)
        True
        >>> PubSubMessage("hello there", 'module.submodule').match_predicate(lambda x: "general" in x)
        False
        """
        return predicate(self.__value)

    def match_topics(self, topics):
        """Return True if this PubSubMessage is matching a Topic.

        Args:
            topics: Topic to match

        Examples:
        >>> PubSubMessage(None, 'module.submodule').match_topics(Topic('module'))
        True
        >>> PubSubMessage(None, 'module.submodule').match_topics(Topic('module.submodule'))
        True
        >>> PubSubMessage(None, 'module.submodule').match_topics(Topic('module.other'))
        False
        """
        return self.__topics in topics


class SubscriptionRequest:
    """A Message sent from a PubSub to a PubSubBroker to command the Broker to register a subscription."""

    def __init__(self, channel, topics, predicate):
        """Construct a Subscription.

        Args:
            channel: queue to send the messages to
            topics: topics to subscribe to
            predicate: predicate that message should verify to be sent
                must be a one parameter function that returns a boolean
                e.g. lambda x: "myString" in x
        """
        self.channel = channel
        self.topics = topics
        self.predicate = predicate


class SubscriptionCancelation:
    """A Message sent from a PubSub to a PubSubBroker to command the Broker to deregister a subscription."""

    def __init__(self, subscription):
        """Construct a SubscriptionCancelation.

        Args:
            subscription: the subscription to unsubscribe
        """
        self.subscription_id = subscription._id
