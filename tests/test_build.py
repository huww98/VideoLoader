import unittest

from videoloader import Video, VideoLoader, _ext

class TestVideoLoader(unittest.TestCase):
    def test_name(self):
        self.assertEqual(str(_ext._VideoLoader), "<class 'videoloader._ext._VideoLoader'>")
    def test_sub_typing(self):
        class VideoLoader_Test(_ext._VideoLoader):
            pass
        class Video_Test(_ext._Video):
            pass

    def test_add_video_not_exists(self):
        loader = VideoLoader()
        with self.assertRaisesRegex(RuntimeError, 'No such file or directory'):
            loader.add_video_file("Anything")

    def test_add_video(self):
        loader = VideoLoader()
        video = loader.add_video_file('tests/test_video.mp4')
        self.assertIsInstance(video, Video)


class TestVideo(unittest.TestCase):
    def test_not_newable(self):
        with self.assertRaises(TypeError):
            Video()
