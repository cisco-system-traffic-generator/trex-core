import unittest

from ..message import *

class PubSubMessageTest(unittest.TestCase):
    """Tests for PubSubMessage class."""

    def test_message_init_seq(self):
        """Test the Message constructor with sequences."""
        seqs = [
            [], ()
        ]
        try:
            for seq in seqs:
                _ = PubSubMessage(None, seq)
        except:
            self.fail("construction of message should work with a sequence")
        
    def test_message_init_object(self):
        """Test the Message constructor with non sequence type."""
        with self.assertRaises(ValueError):
            _ = PubSubMessage(None, object())

    def test_message_str(self):
        """Test the __str__ method with Messages constructed with a list or string as topics"""
        topics_list = ['1', '2', '3']
        topics_str = "1.2.3"
        value = "test value"
        message_list = PubSubMessage(value, topics_list)
        message_str = PubSubMessage(value, topics_str)
        self.assertEqual(str(message_list), "1.2.3 | test value")
        self.assertEqual(str(message_str), "1.2.3 | test value")

    def test_message_contains_subtopic(self):
        """Test the __contains__ method for a subtopic."""
        self.assertTrue(Topic('module.submodule').__contains__(Topic('module.submodule.specific')))

    def test_message_contains_suptopic(self):
        """Test the __contains__ method for a suptopic."""
        self.assertFalse(Topic('module.submodule.specific').__contains__(Topic('module.submodule')))

    def test_message_contains_root_topic(self):
        """Test the __contains__ method for the root topic ''"""
        self.assertTrue(Topic('').__contains__(Topic('something')))

    def test_message_match_predicate_true(self):
        """Test the match_predicate command when the predicate is always verified."""
        self.assertTrue(PubSubMessage("hello there", 'module.submodule').match_predicate(lambda x: True))

    def test_message_match_predicate_false(self):
        """Test the match_predicate command when the predicate is never verified."""
        self.assertFalse(PubSubMessage("hello there", 'module.submodule').match_predicate(lambda x: False))
