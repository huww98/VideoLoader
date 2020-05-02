import unittest
import subprocess

import numpy as np
import numpy.testing

from videoloader import VideoLoader
import videoloader._ext

class GetBatchDataTest(unittest.TestCase):
    def test_comp_with_ffmpeg(self):
        loader = VideoLoader()
        video = loader.add_video_file('./tests/test_video.mp4')

        ffmpeg_proc = subprocess.run(
            ['ffmpeg', '-loglevel', 'warning', '-i', 'tests/test_video.mp4', '-pix_fmt', 'rgb24', '-f', 'rawvideo', '-'],
            stdout=subprocess.PIPE, check=True,
        )
        for num_frames_to_verify in [1,2,3,8,40]:
            with self.subTest(num_frames=num_frames_to_verify):
                batch = video.get_batch(range(num_frames_to_verify))
                batch = videoloader._ext.dltensor_to_numpy(batch)

                frames, width, height, channel = batch.shape
                self.assertEqual(frames, num_frames_to_verify)

                frame_size = batch.nbytes
                expected = np.frombuffer(ffmpeg_proc.stdout, dtype='uint8', count=frame_size)
                expected = expected.reshape((frames, height, width, channel))
                expected = expected.swapaxes(1, 2)

                for f in range(frames):
                    numpy.testing.assert_array_equal(batch[f], expected[f], f'Frame {f} mismatch')
