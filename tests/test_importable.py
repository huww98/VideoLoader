import unittest

class TestImportable(unittest.TestCase):
    def test_import(self):
        import videoloader

    def test_ext_import(self):
        import videoloader._ext
