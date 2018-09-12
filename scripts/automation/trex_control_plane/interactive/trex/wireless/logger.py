"""
Plugin WLC logging
main logger on a separate process that saves all received logs from other processes
"""

import logging
import logging.handlers
import multiprocessing
import threading
import re
import queue
import time
import inspect


class TaggerFilter(logging.Filter):
    """Always True Filter that tags a LogRecord with a given label and tag."""
    def __init__(self, label, tag):
        """Create a TaggerFilter that will tag LogRecords with 'label' and 'tag'.
        LogRecord.labels[label] = tag
        """
        super().__init__()
        self.label = label
        self.tag = tag

    def filter(self, record):
        if not hasattr(record, "labels"):
            record.labels = {}
        record.labels[self.label] = self.tag
        return True


class WhiteListTagFilter(logging.Filter):
    """White List Filter for logger records, parse for tagged records (see TaggerFilter)."""

    def __init__(self, label, white_list_tags):
        """Create a WhiteListTagFilter, filtering for LogRecords 'label' tagged with one of 'white_list_tags'."""
        super().__init__()
        self.label = label
        self.white_list_tags = white_list_tags

    def filter(self, record):
        if not hasattr(record, "labels") or self.label not in record.labels:
            return False
        if record.labels[self.label] in self.white_list_tags:
            return True
        return False


def get_queue_logger(queue, module_name, module_id, level, filter=None):
    """Returns a Logger forwarding logs to the queue.

    To be used only once per process, for child logger use getChildLogger function.
    For use in a multiprocess environment, with a log listener process listening to the queue.
    Multiple processes can use the same queue for centralized logging.
    """
    h = logging.handlers.QueueHandler(queue)
    logging.getLogger().addHandler(h)

    logger = logging.getLogger(module_id)
    logger.module_name = module_name
    tagger = TaggerFilter("MODULE", module_name)
    logger.addFilter(tagger)
    if filter:
        logger.addFilter(filter)
        logger.module_filter = filter
    logger.setLevel(level)
    logger.debug("logger initialized")
    return logger


def get_child_logger(logger, sub_module_name):
    """Return a child of given (queue) logger, tagged with module name."""
    assert(logger.module_name)
    sublogger = logger.getChild(sub_module_name)
    sublogger.module_name = logger.module_name + "." + sub_module_name
    sublogger.name = sublogger.module_name
    tagger = TaggerFilter("MODULE", sublogger.module_name)
    sublogger.addFilter(tagger)
    if hasattr(logger, "module_filter"):
        sublogger.addFilter(logger.module_filter)
    return sublogger


class LogListener(multiprocessing.Process):
    """Process that listens on a queue for LogRecord objects, and save the corresponding log to a file."""
    def __init__(self, queue, filename, log_level, log_filter=None):
        """Create a LogListener.

        Args:
            queue (multiprocessing.Queue): shared queue used to receive LogRecords from other processes.
            filename (string): name of the file to write logs to.
            log_level (logging.INFO): level of logs to be kept, see logging.INFO or others.
                Be aware that filtering is prefered to be done in the other processes.
            log_filter (logging.Filters): a filter for logs, see logging.Filters objects.
                Be aware that filtering is prefered to be done in the other processes.
        """
        super().__init__()
        self.name = "LogListener"
        self.queue = queue
        self.filename = filename
        self.log_level = log_level
        self.log_filter = log_filter

    def configure(self):
        root = logging.getLogger()
        root.setLevel(self.log_level)
        if self.log_filter:
            root.addFilter(self.log_filter)
        h = logging.FileHandler(self.filename, mode='w')
        f = logging.Formatter(
            '%(asctime)s %(levelname)-6.6s %(name)-50.50s - %(message)s')
        h.setFormatter(f)
        root.addHandler(h)

    def run(self):
        self.configure()
        while True:
            try:
                record = self.queue.get()
                if record is None:
                    # exit signal
                    self.queue.close()
                    break
                logger = logging.getLogger()
                logger.handle(record)
            except KeyboardInterrupt:
                pass
            except EOFError:
                break
            except:
                import sys
                import traceback
                traceback.print_exc(file=sys.stderr)
