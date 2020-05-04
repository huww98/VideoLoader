import unittest
import subprocess
from functools import reduce

import numpy as np
import numpy.testing
import torch
import torch.testing

from videoloader import VideoLoader
import videoloader._ext


class GetBatchDataTest(unittest.TestCase):
    def setUp(self):
        ffmpeg_proc = subprocess.run(
            ['ffmpeg', '-loglevel', 'warning', '-i', 'tests/test_video.mp4',
                '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
            stdout=subprocess.PIPE, check=True,
        )
        self.truth_data_buffer = ffmpeg_proc.stdout

    def get_truth(self, shape, frames):
        num_frames, width, height, channel = shape
        self.assertEqual(num_frames, len(frames))

        truth = np.frombuffer(
            self.truth_data_buffer, dtype='uint8')
        truth = truth.reshape((-1, height, width, channel))
        truth = truth.swapaxes(1, 2)
        truth = truth[frames]
        return truth

    def test_comp_with_ffmpeg(self):
        loader = VideoLoader()
        video = loader.add_video_file('./tests/test_video.mp4')
        frameIndices = [
            range(1), range(2), range(3), range(8), range(40),
            range(3, 18), list(range(3, 18)) + list(range(66, 88)),
            [1, 1, 1],
            [69, 55, 22, 80, 32],
        ]
        for frames in frameIndices:
            frames = list(frames)
            with self.subTest(frame_indices=frames):
                batch = video.get_batch(frames)

                expected = self.get_truth(batch.shape, frames)
                for i, f in enumerate(frames):
                    numpy.testing.assert_array_equal(
                        batch[i], expected[i], f'Frame {f} mismatch ({i}th frame in batch)')

    def test_pytorch_comp_with_ffmpeg(self):
        loader = VideoLoader(data_container='pytorch')
        video = loader.add_video_file('./tests/test_video.mp4')
        frames = list(range(2))
        batch = video.get_batch(frames)
        expected = self.get_truth(batch.shape, frames)

        expected = torch.from_numpy(expected.copy())

        torch.testing.assert_allclose(batch, expected, 0, 0)
