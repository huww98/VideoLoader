import time
from pathlib import Path
import random

import torchvision
from torchvision.datasets.video_utils import VideoClips

torchvision.set_video_backend('video_reader')

BASE = Path('/tmp/answering_questions')

def main():
    files = list(BASE.glob('**/*.mp4'))
    frames = list(range(16))

    t1 = time.perf_counter()
    video_clips = VideoClips(
        [str(f) for f in files],
        clip_length_in_frames=16,
    )
    t2 = time.perf_counter()
    print('Prepare:', t2 - t1)

    def read_video(i):
        # clip_index = random.randrange(0, len(video_clips.clips[i]))
        # print(FILES[i], clip_index, len(video_clips.clips[i]))
        clip_index = video_clips.cumulative_sizes[i] - len(video_clips.clips[i])
        video, audio, info, video_index = video_clips.get_clip(clip_index)
        assert video_index == i
        # print(video.size())

    # Warm up
    for i in range(16):
        read_video(i)

    print(f"Reading {len(frames)} frames from {len(files)} files")

    for i in range(3):
        print(f'pass {i + 1}')
        t1 = time.perf_counter()
        for i in range(len(files)):
            read_video(i)
        t2 = time.perf_counter()
        print(f'Time: {t2-t1}')

    print('Done')


if __name__ == "__main__":
    main()
