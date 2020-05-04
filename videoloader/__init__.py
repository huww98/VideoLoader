from typing import Union, Iterable
import os
import contextlib

from . import _ext


def _data_convert_to_numpy(batch):
    return _ext.dltensor_to_numpy(batch)


def _data_convert_to_pytorch(batch):
    import torch.utils.dlpack
    return torch.utils.dlpack.from_dlpack(batch)


class Video(_ext._Video):
    ''' An opened video file.

    Some metadata of video is saved to enable efficient reading. But file
    discriptor is not kept open to save resource. To support large scale
    machine learning, it is designed to open millions of videos at the same
    time.

    Changing the video file content after open results in undefined behaviour.

    Get an instance of this class through `VideoLoader.add_video_file`
    '''

    def get_batch(self, frame_indices: Iterable[int]):
        ''' Get arbitrary number of frames in this video

        Pixel format is RGB24

        * frame_indices (Iterable[int]): Arbitrary number of frame indices.
            Can be repeated, out of order, sparse.

        Returns: numpy.ndarray or torch.Tensor. shape (frame, width, height, channel)
        '''
        with self.keep_awake():
            return self._data_convert(super().get_batch(frame_indices))

    @contextlib.contextmanager
    def keep_awake(self):
        ''' Keep this video active to perform multiple read in a row

        Example:

        ```python
        with video.keep_awake():
            a = video.get_batch([1, 2])
            b = video.get_batch([8, 9])
        ```
        '''
        self._kept_awake += 1
        try:
            yield self
        finally:
            self._kept_awake -= 1
            if self._kept_awake == 0:
                self.sleep()

    def sleep(self):
        ''' Enter sleeping state.

        Release buffer, close file descriptor, etc.
        It will be woke up automatically when reading data.
        '''
        return super().sleep()

    def is_sleeping(self) -> bool:
        ''' Whether this video is in sleeping state
        '''
        return super().is_sleeping()


class VideoLoader(_ext._VideoLoader):
    ''' Context to load video files.

    * data_container ('numpy' | 'pytorch' | None): Set the output format
    '''

    def __init__(self, data_container='numpy'):
        super().__init__(Video)
        self._data_convert = {
            None: lambda x: x,
            'numpy': _data_convert_to_numpy,
            'pytorch': _data_convert_to_pytorch,
        }.get(data_container)
        if self._data_convert is None:
            raise ValueError(f'Unsupported data container "{data_container}"')

    def add_video_file(self, url: Union[os.PathLike, str, bytes]) -> Video:
        ''' Open a new video

        * url: URL to the file to be opened.
            Only local file path supported currently

        The returned `Video` object should be saved and reused for efficient
        reading.
        '''
        video = super().add_video_file(url)
        video._data_convert = self._data_convert
        video._kept_awake = 0
        return video
