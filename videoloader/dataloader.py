import os
from typing import Union, List
import threading
import time
import math
import logging

from . import VideoLoader, Video

logger = logging.getLogger(__name__)


class VideoDatasetLoader:
    def __init__(self,
                 video_path: Union[str, bytes, os.PathLike],
                 sampler=None,
                 max_thread=1,
                 max_prefetch=128,
                 data_container='pytorch'):
        self.videos = list(video_path)
        self.loader = VideoLoader(data_container=data_container)
        self.max_thread = max_thread
        self.max_prefetch = max_prefetch
        self.sampler = sampler

    def __iter__(self):
        return _ThreadingVideoLoaderIterator(self)


class _PrefetchingVideo:
    __slots__ = ('loaded', 'data')

    def __init__(self):
        self.loaded = threading.Event()
        self.data = None


class _ThreadingVideoLoaderIterator:
    def __init__(self, dataset_loader: VideoDatasetLoader):
        self.dataset_loader = dataset_loader
        self._next_index = 0
        self._prefetch_index_iter = iter(dataset_loader.sampler)
        self._prefetch: List[_PrefetchingVideo] = []
        self._finished = False

        self._start_prefetch = threading.Condition()
        self._new_data = threading.Condition()

        self._runing_prefetch_thread = 0
        self._scheduled_prefetch_thread = dataset_loader.max_thread

        self._recent_load_time = [1e4 for _ in range(dataset_loader.max_prefetch)]
        self._recent_load_time_sum = sum(self._recent_load_time)

        t0 = time.perf_counter()
        self._recent_read_timepoint = [t0 - 1e-3 * i
                                       for i in range(dataset_loader.max_prefetch + 1)]
        self._recent_read_timepoint.reverse()

        self._threads = [threading.Thread(target=self._load_thread, name=f'Data prefetch {i}')
                         for i in range(dataset_loader.max_thread)]
        self.i = 0
        self.stat_thread = []
        self.stat_prefetch = []

        for t in self._threads:
            t.start()

    def __iter__(self):
        return self

    def _thread_should_run(self):
        return (self._runing_prefetch_thread < self._scheduled_prefetch_thread and
                len(self._prefetch) < self.dataset_loader.max_prefetch)

    def _update_load_speed(self, load_time):
        self._recent_load_time_sum -= self._recent_load_time.pop(0)
        self._recent_load_time_sum += load_time
        self._recent_load_time.append(load_time)

    def _update_read_speed(self, read_timepoint):
        self._recent_read_timepoint.pop(0)
        self._recent_read_timepoint.append(read_timepoint)

    def _do_schedule(self):
        read_time = self._recent_read_timepoint[-1] - \
            self._recent_read_timepoint[0]
        read_time *= 0.95  # We'd rather fetch faster than slower
        n_threads = self._recent_load_time_sum / read_time
        n_threads = min(n_threads, self.dataset_loader.max_thread)
        self._scheduled_prefetch_thread = n_threads
        add_threads = math.ceil(n_threads - self._runing_prefetch_thread)
        logger.debug('Scheduling %.2f threads. %d threads to wakeup. read_time: %.2f, load_time: %.2f',
            n_threads, add_threads, read_time, self._recent_load_time_sum)

        if add_threads > 0:
            self._start_prefetch.notify(add_threads)

    def _load_thread(self):
        thread = threading.current_thread()
        while not self._finished:
            with self._start_prefetch:
                while not self._thread_should_run():
                    self._start_prefetch.wait()
                self._runing_prefetch_thread += 1

                t0 = time.perf_counter()
                try:
                    v_index, frame_indices = next(self._prefetch_index_iter)
                except StopIteration:
                    self._runing_prefetch_thread -= 1
                    self._finished = True
                    return
                logger.debug('Thread %s: Fetching %d video',
                             thread.name, v_index)

                container = _PrefetchingVideo()
                self._prefetch.append(container)
                video = self.dataset_loader.videos[v_index]
                open_video = False
                if isinstance(video, threading.Event):
                    # This video is opening by other thread
                    video.wait()
                    video = self.dataset_loader.videos[v_index]
                    assert isinstance(video, Video)
                elif not isinstance(video, Video):
                    # This video has not been opened
                    self.dataset_loader.videos[v_index] = video_opened = threading.Event()
                    open_video = True

            if open_video:
                video = self.dataset_loader.videos[v_index] = \
                    self.dataset_loader.loader.add_video_file(video)
                video_opened.set()
            container.data = video.get_batch(frame_indices)

            with self._new_data:
                container.loaded.set()
                self._new_data.notify_all()

            with self._start_prefetch:
                t1 = time.perf_counter()
                self._runing_prefetch_thread -= 1
                self._update_load_speed(t1 - t0)
                self._do_schedule()

    def __next__(self):
        if self._finished and not self._prefetch:
            raise StopIteration()

        with self._new_data:
            self.stat_prefetch.append(len(self._prefetch))
            self.stat_thread.append(self._scheduled_prefetch_thread)
            while not self._prefetch:
                self._new_data.wait()
            container = self._prefetch.pop()

        container.loaded.wait()
        assert container.data is not None

        self.i += 1
        self._update_read_speed(time.perf_counter())
        if not self._finished:
            with self._start_prefetch:
                self._do_schedule()

        return container.data
