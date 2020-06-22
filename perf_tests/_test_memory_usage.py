from pathlib import Path

import psutil

from videoloader import Video

BASE = Path('/tmp/answering_questions')

def main():
    this_process = psutil.Process()

    files = list(BASE.glob('**/*.mp4'))

    # Warm up
    for f in files[:16]:
        Video(f)

    init_rss = rss = this_process.memory_info().rss
    print(rss)
    videos = []
    for i, f in enumerate(files):
        v = Video(f)
        v.sleep()
        videos.append(v)
        new_rss = this_process.memory_info().rss
        print(f'{i:3d} RSS: {new_rss} ({new_rss - rss:=+8d})')
        rss = new_rss

    print(f'Average: {(rss - init_rss) / len(files) / 1024:.2f} KB/file' )

    print('Done')


if __name__ == "__main__":
    main()
