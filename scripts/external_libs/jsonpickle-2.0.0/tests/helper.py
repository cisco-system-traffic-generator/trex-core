import unittest


class SkippableTest(unittest.TestCase):
    def skip(self, msg):
        if hasattr(self, 'skipTest'):
            return self.skipTest(msg)
        return None
