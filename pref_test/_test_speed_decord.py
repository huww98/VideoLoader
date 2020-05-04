from pathlib import Path
import time

from decord import VideoReader

BASE = Path('/tmp/answering_questions')

def main():
    files = list(BASE.glob('**/*.mp4'))
    frames = list(range(16))

    # Warm up
    for f in files[:16]:
        v = VideoReader(str(f))
        v.get_batch(frames)

    print(f"Reading {len(frames)} frames from {len(files)} files")

    for i in range(3):
        print(f'pass {i + 1}')
        t1 = time.perf_counter()
        for f in files:
            v = VideoReader(str(f))
            v.get_batch(frames)
        t2 = time.perf_counter()
        print(f'Time: {t2-t1}')

    print('Done')


if __name__ == "__main__":
    main()
