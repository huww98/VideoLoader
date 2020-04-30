import unittest

import torch
import torch.utils.dlpack

from videoloader import VideoLoader

class TestGetBatchBasic(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader()
        self.video = loader.add_video_file('./tests/test_video.mp4')
        self.batch = self.video.get_batch([0, 1])

    def test_type(self):
        self.assertRegex(repr(self.batch), '<capsule object "dltensor" at 0x[0-9a-f]+>')


class TestGetBatchPyTorch(unittest.TestCase):
    def setUp(self):
        loader = VideoLoader()
        self.video = loader.add_video_file('./tests/test_video.mp4')

    def test_shape(self):
        batch = self.video.get_batch([0, 1])
        tensor = torch.utils.dlpack.from_dlpack(batch)
        self.assertEqual(tensor.size(), (2, 456, 256, 3))
