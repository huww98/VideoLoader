import os
import sys
from pathlib import Path
from setuptools import setup, Extension, find_packages
from setuptools.command.build_ext import build_ext as _build_ext

class CMakeExtension(Extension):
    def __init__(self, name, cmake_root, target):
        self.cmake_root = cmake_root
        self.target = target
        super().__init__(name, sources=[])

class build_ext(_build_ext):
    def run(self):
        for ext in self.extensions:
            if isinstance(ext, CMakeExtension):
                self.build_cmake(ext)

        self.extensions = [e for e in self.extensions if not isinstance(e, CMakeExtension)]
        super().run()

    def build_cmake(self, ext: CMakeExtension):
        build_temp = Path(self.build_temp)
        build_temp.mkdir(parents=True, exist_ok=True)
        ext_path = Path(self.get_ext_fullpath(ext.name))
        ext_path.parent.mkdir(parents=True, exist_ok=True)

        config = 'Debug' if self.debug else 'Release'
        py_ver = sys.version_info
        cmake_args = [
            '-S', str(ext.cmake_root),
            '-B', str(build_temp),
            f'-DPython_VERSION={py_ver.major}.{py_ver.minor}',
            f'-DPython_ROOT_DIR={sys.prefix}',
            '-DPython_FIND_STRATEGY=LOCATION',
            f'-DCMAKE_BUILD_TYPE={config}',
            f'-D{ext.target}_DESTINATION={ext_path.parent}',
            f'-D{ext.target}_NAME={ext_path.name}',
        ]

        build_args = [
            '--config', config,
            '--target', ext.target,
            '--',
        ]
        if self.parallel is not None:
            build_args.extend(['-j', str(self.parallel)])

        self.spawn(['cmake', ] + cmake_args)
        if not self.dry_run:
            self.spawn(['cmake', '--build', str(build_temp)] + build_args)

ext_module = CMakeExtension(
    name='videoloader._ext',
    cmake_root='videoloader/_ext',
    target='videoloader')

this_directory = Path(__file__).parent
with (this_directory / 'README.md').open(encoding='utf-8') as f:
    long_description = f.read()

setup(
    name='VideoLoader',
    version='0.1.0',
    description='Enable high performance video data loading for machine learning.',
    long_description=long_description,
    long_description_content_type='text/markdown',
    url='https://github.com/huww98/VideoLoader',
    author='Weiwen Hu',
    author_email='huww98@outlook.com',
    license='MIT',
    packages=find_packages(exclude=['tests']),
    ext_modules=[ext_module],
    cmdclass={
        'build_ext': build_ext,
    },
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Developers',
        'Intended Audience :: Science/Research',
        'License :: OSI Approved :: MIT License',
        'Operating System :: POSIX',
        'Operating System :: POSIX :: Linux',
        'Topic :: Multimedia :: Video',
        'Topic :: Scientific/Engineering :: Artificial Intelligence',
        'Programming Language :: C++',
        'Programming Language :: Python',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.6',
        'Programming Language :: Python :: 3.7',
        'Programming Language :: Python :: 3.8',
        'Programming Language :: Python :: 3 :: Only',
        'Programming Language :: Python :: Implementation :: CPython',
    ],
    platforms=['Linux'],
    python_requires='>=3.6',
    install_requires=['numpy'],
    extras_require={
        'pytorch': ['torch>=0.3.0']
    },
    keywords='pytorch dataloader video',
)
