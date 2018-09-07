import collections
import logging


def predicate_true(x):
    """Predicate True for any value."""
    return True


def topics_as_list(topics):
    """Return topics as a list of string.
    Performs type checking.

    Args:
        topics: sequence of topics (sequence of strings) or string representing the topics separated by periods.
            e.g. ['module', 'submodule', 'event'] or 'module.submodule.event' (equivalent)

    Examples:
        >>> topics_as_list('module.submodule.event')
        ['module', 'submodule', 'event']
        >>> topics_as_list(['module', 'submodule', 'event'])
        ['module', 'submodule', 'event']
        >>> topics_as_list('module,submodule,event')
        ['module,submodule,event']
        >>> topics_as_list(object())
        Traceback (most recent call last):
            ...
        ValueError: topics should be of type str or a sequence of str
    """
    if isinstance(topics, str):
        if not topics:
            return []
        return topics.split('.')
    elif isinstance(topics, collections.Sequence):
        if any([t for t in topics if not isinstance(t, str)]):
            raise ValueError(
                "topics should be of type str or a sequence of str")
        return topics
    else:
        raise ValueError("topics should be of type str or a sequence of str")
