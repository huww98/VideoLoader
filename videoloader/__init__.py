from typing import Union
import os

from . import _ext

class VideoLoader(_ext._VideoLoader):
    def add_video_file(self, url: Union[os.PathLike, str, bytes]):
        return super().add_video_file(url)
