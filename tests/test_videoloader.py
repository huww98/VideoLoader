import unittest

from videoloader import VideoLoader

class VideoLoaderTest(unittest.TestCase):
    def setUp(self):
        self.loader = VideoLoader()

    def test_file_not_found(self):
        with self.assertRaises(FileNotFoundError):
            self.loader.add_video_file('Anything')

    def test_open_dir(self):
        with self.assertRaises(IsADirectoryError):
            self.loader.add_video_file('tests')
