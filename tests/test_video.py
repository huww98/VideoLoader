import unittest
import fractions

import torch
import torch.utils.dlpack
import numpy as np
import numpy.testing

from videoloader import VideoLoader
import videoloader._ext


class TestGetBatchBasic(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader(data_container=None)
        self.video = loader.add_video_file('./tests/test_video.mp4')
        self.batch = self.video.get_batch([0, 1])

    def test_type(self):
        self.assertRegex(repr(self.batch),
                         '<capsule object "dltensor" at 0x[0-9a-f]+>')

    def test_len(self):
        self.assertEqual(self.video.num_frames(), 300)
        self.assertEqual(len(self.video), 300)

    def test_out_of_range(self):
        with self.assertRaises(IndexError):
            self.video.get_batch([9999])

    def test_fps(self):
        fps = self.video.average_frame_rate()
        self.assertIsInstance(fps, fractions.Fraction)
        self.assertEqual(fps, fractions.Fraction(30000, 1001))


class TestGetBatch(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader()
        self.video = loader.add_video_file('./tests/test_video.mp4')

    def test_numpy_shape(self):
        batch = self.video.get_batch([0, 1])
        self.assertIsInstance(batch, np.ndarray)
        self.assertEqual(batch.shape, (2, 456, 256, 3))


class TestGetBatchPytorch(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader(data_container='pytorch')
        self.video = loader.add_video_file('./tests/test_video.mp4')

    def test_pytorch_shape(self):
        batch = self.video.get_batch([0, 1])
        self.assertTrue(torch.is_tensor(batch))
        self.assertEqual(batch.size(), (2, 456, 256, 3))


class TestSleepState(unittest.TestCase):
    def setUp(self):
        self.loader = VideoLoader()

    def test_created_awake(self):
        video = self.loader.add_video_file('./tests/test_video.mp4')
        self.assertFalse(video.is_sleeping())

    def test_sleeping_after_read(self):
        video = self.loader.add_video_file('./tests/test_video.mp4')
        video.get_batch([0, 1])
        self.assertTrue(video.is_sleeping())

    def test_keep_awake(self):
        video = self.loader.add_video_file('./tests/test_video.mp4')
        with video.keep_awake():
            video.get_batch([0, 1])
            self.assertFalse(video.is_sleeping())
        self.assertTrue(video.is_sleeping())
