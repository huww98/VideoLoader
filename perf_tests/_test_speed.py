from pathlib import Path
import time

from videoloader import VideoLoader

BASE = Path('/tmp/answering_questions')

def main():
    files = list(BASE.glob('**/*.mp4'))
    frames = list(range(16))
    loader = VideoLoader()

    # Warm up
    for f in files[:16]:
        v = loader.add_video_file(f)
        v.get_batch(range(16))

    print(f"Reading {len(frames)} frames from {len(files)} files")
    print("initial pass")
    videos = []
    t1 = time.perf_counter()
    for f in files:
        v = loader.add_video_file(f)
        videos.append(v)
        v.get_batch(frames)
    t2 = time.perf_counter()
    print(f'Time: {t2-t1}')

    for i in range(3):
        print(f'pass {i + 1}')
        t1 = time.perf_counter()
        for v in videos:
            # with v.keep_awake():
                v.get_batch(frames)
        t2 = time.perf_counter()
        print(f'Time: {t2-t1}')

    print('Done')


if __name__ == "__main__":
    main()
