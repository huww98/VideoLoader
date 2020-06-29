import unittest

import videoloader

class TestOpen(unittest.TestCase):
    def test_simple(self):
        videos = videoloader.open_video_tar('./tests/tar/test_videos.tar')
        self.assertEqual(len(videos), 3)
        self.assertTrue(isinstance(videos[0], videoloader.Video))

    def test_filtered(self):
        def filter(entry):
            return entry.path == 'answering_questions/-g3JhkJRVY4_000333_000343.mp4'
        videos = videoloader.open_video_tar('./tests/tar/test_videos.tar', filter)
        self.assertEqual(len(videos), 1)

    def test_filter_raise(self):
        class TestError(RuntimeError):
            pass
        def filter(entry):
            if entry.path == 'answering_questions/-g3JhkJRVY4_000333_000343.mp4':
                raise TestError()
            return True
        with self.assertRaises(TestError):
            videoloader.open_video_tar('./tests/tar/test_videos.tar', filter)

    def test_multi_thread(self):
        videos = videoloader.open_video_tar('./tests/tar/test_videos.tar', max_threads=4)
        self.assertEqual(len(videos), 3)
