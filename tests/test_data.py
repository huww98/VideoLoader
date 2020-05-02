import unittest
import subprocess

import numpy as np
import numpy.testing
import torch
import torch.testing

from videoloader import VideoLoader
import videoloader._ext


def get_ground_truth():
    ffmpeg_proc = subprocess.run(
        ['ffmpeg', '-loglevel', 'warning', '-i', 'tests/test_video.mp4',
            '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
        stdout=subprocess.PIPE, check=True,
    )
    return ffmpeg_proc.stdout


class GetBatchDataTest(unittest.TestCase):
    def test_comp_with_ffmpeg(self):
        loader = VideoLoader()
        video = loader.add_video_file('./tests/test_video.mp4')
        truth = get_ground_truth()
        for num_frames_to_verify in [1, 2, 3, 8, 40]:
            with self.subTest(num_frames=num_frames_to_verify):
                batch = video.get_batch(range(num_frames_to_verify))

                frames, width, height, channel = batch.shape
                self.assertEqual(frames, num_frames_to_verify)

                expected = np.frombuffer(
                    truth, dtype='uint8', count=batch.nbytes)
                expected = expected.reshape((frames, height, width, channel))
                expected = expected.swapaxes(1, 2)

                for f in range(frames):
                    numpy.testing.assert_array_equal(
                        batch[f], expected[f], f'Frame {f} mismatch')


class GetBatchDataPytorchTest(unittest.TestCase):
    def test_comp_with_ffmpeg(self):
        loader = VideoLoader(data_container='pytorch')
        video = loader.add_video_file('./tests/test_video.mp4')
        truth = get_ground_truth()
        batch = video.get_batch(range(2))
        frames, width, height, channel = batch.size()

        expected = np.frombuffer(
            truth, dtype='uint8', count=batch.numel())
        expected = torch.from_numpy(expected.copy())
        expected = expected.view(frames, height, width, channel)
        expected = expected.transpose(1, 2)

        torch.testing.assert_allclose(batch, expected, 0, 0)
