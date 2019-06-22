import unittest
import multiprocessing

from ..pubsub import *
from ..message import *

def predicate_contains_hello(x):
    """Predicate True when 'hello' is in value."""
    return 'hello' in x

class PubSubTest(unittest.TestCase):
    """'Black Box' testing of the pubsub module."""

    def setUp(self):
        # initialize and start PubSub
        self.pubsub = PubSub()
        self.pubsub.start()

    def tearDown(self):
        # stop PubSub
        self.pubsub.stop()

    def test_example(self):
        """One process test, one subscriber, one publisher."""
        queue = self.pubsub.subscribe("a.b.c")
        self.pubsub.publish("test message", "a.b.c")
        # expected message to receive
        msg = PubSubMessage("test message", "a.b.c")
        got = queue.get()
        self.assertTrue(queue._queue.empty())
        queue = self.pubsub.unsubscribe(queue)
        self.assertEqual(str(got), str(msg))

    def test_publisher(self):
        """Test the Publisher class."""
        queue = self.pubsub.subscribe("a.b.c")
        publisher = self.pubsub.Publisher("a.b")
        publisher.publish("test message", "c")
        # expected message to receive
        msg = PubSubMessage("test message", "a.b.c")
        got = queue.get()
        self.assertTrue(queue._queue.empty())
        queue = self.pubsub.unsubscribe(queue)
        self.assertEqual(str(got), str(msg))

    def test_subpublisher(self):
        """Test the subpublisher method of publisher."""
        queue = self.pubsub.subscribe("a.b.c")
        publisher = self.pubsub.Publisher("a")
        subpublisher = publisher.SubPublisher("b")
        subpublisher.publish("test message", "c")
        # expected message to receive
        msg = PubSubMessage("test message", "a.b.c")
        got = queue.get()
        self.assertTrue(queue._queue.empty())
        queue = self.pubsub.unsubscribe(queue)
        self.assertEqual(str(got), str(msg))

    def test_predicate(self):
        queue = self.pubsub.subscribe("a.b.c", predicate_contains_hello)
        self.pubsub.publish("bonjour ici", "a.b.c") # should not be received
        self.pubsub.publish("hello there", "a.b.c") # should be received
        # expected message to receive
        msg = PubSubMessage("hello there", "a.b.c")
        got = queue.get()

        self.assertTrue(queue._queue.empty()) # should only receive one message

        queue = self.pubsub.unsubscribe(queue)

        self.assertEqual(str(got), str(msg))
    
    def test_with_statement(self):
        """Tests the subscription using the with statement."""
        with self.pubsub.subscribe("a.b.c") as sub:
            self.pubsub.publish("test message", "a.b.c")
            got = sub.get()

        # expected message to receive
        msg = PubSubMessage("test message", "a.b.c")

        with self.assertRaises(ValueError):
            sub.unsubscribe()

        self.assertEqual(str(got), str(msg))

    def test_multiple_subscribers_same_topics(self):
        """Tests with two subscriber for the exact same topics, one publisher."""
        sub1 = self.pubsub.subscribe("a.b.c")
        sub2 = self.pubsub.subscribe("a.b.c")

        self.pubsub.publish("test message", "a.b.c")
        # expected message to receive
        msg = PubSubMessage("test message", "a.b.c")
        got1 = sub1.get()
        got2 = sub2.get()
        self.pubsub.unsubscribe(sub1)
        self.pubsub.unsubscribe(sub2)

        self.assertEqual(str(got1), str(got2))
        self.assertEqual(str(got2), str(msg))

    def test_multiple_subscribers_same_suptopics(self):
        """Tests with two subscriber for the same suptopic, one publisher."""
        sub1 = self.pubsub.subscribe("a")
        sub2 = self.pubsub.subscribe("a.b.c")

        self.pubsub.publish("test message", "a.b.c.d")
        # expected message to receive
        msg = PubSubMessage("test message", "a.b.c.d")

        got1 = sub1.get()
        got2 = sub2.get()
        self.pubsub.unsubscribe(sub1)
        self.pubsub.unsubscribe(sub2)

        self.assertEqual(str(got1), str(got2))
        self.assertEqual(str(got2), str(msg))

    def test_multiple_subscribers_different_level(self):
        """Tests with two subscriber, same root topic, different topics, two publishers with diffrent topic level (one 'a.b.c' one 'a.b.c'."""
        sub1 = self.pubsub.subscribe("a")
        sub2 = self.pubsub.subscribe("a.b.c")

        # should be received by sub1 and sub2
        self.pubsub.publish("test message 1","a.b.c")
        # should be received only by sub1
        self.pubsub.publish("test message 2", "a.b")
        # expected messages to receive
        msg1 = str(PubSubMessage("test message 1", "a.b.c"))
        msg2 = str(PubSubMessage("test message 2", "a.b"))

        got1 = [str(sub1.get()), str(sub1.get())]
        got2 = str(sub2.get())

        self.pubsub.unsubscribe(sub1)
        self.pubsub.unsubscribe(sub2)

        self.assertEqual(got2, msg1)  # sub2 received msg1
        self.assertTrue(sub2._queue.empty())  # sub2 only received 1 message
        # sub1 received msg1 and msg2
        self.assertEqual(set(got1), set([msg1, msg2]))

    def test_multiple_subscribers_different_topics(self):
        """Tests with two subscriber for different topics, two publishers."""
        sub1 = self.pubsub.subscribe("a.b.c")
        sub2 = self.pubsub.subscribe( "c.b.a")

        self.pubsub.publish("test message1", "a.b.c")
        self.pubsub.publish("test message2", "c.b.a")
        # expected messages to receive
        msg1 = PubSubMessage("test message1", "a.b.c")
        msg2 = PubSubMessage("test message2", "c.b.a")

        got1 = sub1.get()
        got2 = sub2.get()

        self.pubsub.unsubscribe(sub1)
        self.pubsub.unsubscribe(sub2)

        self.assertEqual(str(got1), str(msg1))
        self.assertEqual(str(got2), str(msg2))

        # ensures no other message is received
        self.assertTrue(sub1._queue.empty())
        self.assertTrue(sub2._queue.empty())

    def test_with_processes(self):
        """Tests PubSub with subprocesses.
        PubSub is launched on the main process.
        A subprocess (writer) publishes a message, while another subprocess (reader) is waiting for this message.
        The reader should receive the message and send it to the main process.
        """

        def worker_reader(pubsub, out_queue):
            """Subscribes, wait for a message with the subscription, and return the message on 'out_queue'."""
            channel = pubsub.subscribe("main.worker")
            # notify main process that the worker started
            out_queue.put("started")
            out_queue.put(channel.get())
            pubsub.unsubscribe(channel)
            # done
            return

        def worker_writer(pubsub, publisher):
            subpub = publisher.SubPublisher("worker") # worker submodule of main
            subpub.publish("hello there", "")
            return

        # expected message to receive
        msg = PubSubMessage("hello there", "main.worker")
        publisher = self.pubsub.Publisher("main")

        out_queue = multiprocessing.Queue()
        reader = multiprocessing.Process(
            target=worker_reader, args=(self.pubsub, out_queue,))
        reader.start()

        out_queue.get()  # waiting for the process to start and send a notification

        writer = multiprocessing.Process(
            target=worker_writer, args=(self.pubsub, publisher))
        writer.start()

        got = out_queue.get()
        self.assertEqual(str(got), str(msg))

        reader.join()
        writer.join()
