# VideoLoader

[![Python test](https://github.com/huww98/VideoLoader/workflows/Test/badge.svg)](https://github.com/huww98/VideoLoader/actions)
[![Releases](https://img.shields.io/pypi/v/VideoLoader.svg)](https://github.com/huww98/VideoLoader/releases)

A python extension to enable high performance video data loading for machine learning.

Beta development stage

## Install

We only provide source package now. To install, you need to have cmake and ffmpeg preinstalled.

This package is tested against FFmpeg 4. To install that (for example, on Ubuntu), you can use:

```shell
sudo add-apt-repository ppa:jonathonf/ffmpeg-4  # for Ubuntu 18.04 and lower
sudo apt-get install -y libavcodec-dev libavfilter-dev libavformat-dev libavutil-dev
```

Then use pip as normal:

```shell
pip install VideoLoader
```
