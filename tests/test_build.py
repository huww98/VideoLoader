import unittest
from videoloader._ext import _VideoLoader

class TestVideoLoader(unittest.TestCase):
    def test_name(self):
        self.assertEqual(str(_VideoLoader), "<class 'videoloader._ext._VideoLoader'>")
    def test_sub_typing(self):
        class VideoLoader_Test(_VideoLoader):
            pass

    def test_add_video(self):
        loader = _VideoLoader()
        with self.assertRaisesRegex(RuntimeError, 'No such file or directory'):
            loader.add_video_file("Anything")
