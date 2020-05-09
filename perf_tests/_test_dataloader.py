from pathlib import Path
import sys
import time
import logging

from torch.utils.data import Dataset, DataLoader
from tqdm import tqdm

from videoloader import VideoLoader, Video
from videoloader.dataloader import VideoDatasetLoader


BASE = Path('/tmp/answering_questions')
files = list(BASE.glob('**/*.mp4'))
sampler = [(i, range(16)) for i, f in enumerate(files)] * 10

def main():
    loader = iter(VideoDatasetLoader(files, sampler, max_thread=4))

    for i, batch in enumerate(tqdm(loader)):
        if (i + 1) % 64 == 0:
            time.sleep(0.01)

    # for t, p in zip(loader.stat_thread, loader.stat_prefetch):
    #     print(f'{t:.2f} {p:3d}')


class MyDataset(Dataset):
    def __init__(self):
        self.loader = VideoLoader(data_container='pytorch')
        self.videos = list(files)

    def __len__(self):
        return len(sampler)

    def __getitem__(self, index):
        i, fs = sampler[index]
        v = self.videos[i]
        if not isinstance(v, Video):
            v = self.videos[i] = self.loader.add_video_file(v)
        return v.get_batch(fs)


def multi_process():
    loader = DataLoader(MyDataset(), num_workers=4, batch_size=64, collate_fn=lambda x:x)
    for batch in tqdm(loader):
        pass


def baseline():
    loader = VideoLoader(data_container='pytorch')
    videos = list(files)
    for i, fs in sampler:
        v = videos[i]
        if not isinstance(v, Video):
            v = videos[i] = loader.add_video_file(v)
        v.get_batch(fs)


if __name__ == "__main__":
    # logging.basicConfig(level=logging.DEBUG, stream=sys.stdout)
    main()
    # multi_process()
