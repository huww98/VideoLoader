import unittest

import torch
import torch.utils.dlpack
import numpy as np

from videoloader import VideoLoader
import videoloader._ext


class TestGetBatchBasic(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader()
        self.video = loader.add_video_file('./tests/test_video.mp4')
        self.batch = self.video.get_batch([0, 1])

    def test_type(self):
        self.assertRegex(repr(self.batch),
                         '<capsule object "dltensor" at 0x[0-9a-f]+>')


class TestGetBatch(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader()
        self.video = loader.add_video_file('./tests/test_video.mp4')

    def test_pytorch_shape(self):
        batch = self.video.get_batch([0, 1])
        tensor = torch.utils.dlpack.from_dlpack(batch)
        self.assertEqual(tensor.size(), (2, 456, 256, 3))

    def test_numpy_shape(self):
        batch = self.video.get_batch([0, 1])
        array = videoloader._ext.dltensor_to_numpy(batch)
        self.assertEqual(array.shape, (2, 456, 256, 3))
