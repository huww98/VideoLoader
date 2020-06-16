import unittest

from videoloader import Video, _ext

class TestVideo(unittest.TestCase):
    def test_name(self):
        self.assertEqual(str(_ext._Video), "<class 'videoloader._ext._Video'>")
    def test_sub_typing(self):
        class Video_Test(_ext._Video):
            pass

    def test_add_video(self):
        video = Video('tests/test_video.mp4')
        self.assertIsInstance(video, Video)

    def test_video_type_check(self):
        with self.assertRaises(TypeError):
            _ext._Video(123)
