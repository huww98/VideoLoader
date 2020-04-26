from pathlib import Path

import psutil

from videoloader import VideoLoader

BASE = Path('/mnt/d/Downloads/answering_questions')

def main():
    this_process = psutil.Process()

    files = list(BASE.glob('**/*.mp4'))
    loader = VideoLoader()

    # Warm up
    for f in files[:16]:
        loader.add_video_file(f)

    init_rss = rss = this_process.memory_info().rss
    print(rss)
    for i, f in enumerate(files):
        loader.add_video_file(f)
        new_rss = this_process.memory_info().rss
        print(f'{i:3d} RSS: {new_rss} (+{new_rss - rss})')
        rss = new_rss

    print(f'Average: {(rss - init_rss) / len(files) / 1024:.2f} KB/file' )

    print('Done')


if __name__ == "__main__":
    main()
