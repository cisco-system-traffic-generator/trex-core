import unittest
import yaml
import os
from wireless.trex_wireless_config import *


class WirelessConfigTest(unittest.TestCase):
    """Tests for the configuration set at launch time."""

    def setUp(self):
        yml_file = os.path.dirname(os.path.realpath(__file__)) + "/config_correct_test.yaml"
        self.config_correct = load_config(yml_file)


    def test_read_correct_config(self):
        """Test the 'load_config' function with a correct yaml config file."""
        config = self.config_correct
    
        # test list
        self.assertEqual(len(config.ports), 2)
        self.assertEqual(config.ports, [0,1])

        # test two levels dict
        self.assertTrue(config.capwap.specific.ssid_timeout, 10)

        # test scalar value
        self.assertEqual("localhost", config.server_ip)

    def test_read_undefined_value(self):
        """Test the 'load_config' function with a correct yaml config file, with undefined value."""

        config = self.config_correct

        # test 1 level dict
        with self.assertRaises(AttributeError):
            _ = config.unknown_field

        # test 2 levels dict
        with self.assertRaises(AttributeError):
            _ = config.base_values.unknown_field

    def test_global_config(self):
        """Test the 'load_config' that should set the global variable 'config' with the parsed yaml."""
        global config
        self.assertEqual(config.server_ip, "localhost")
