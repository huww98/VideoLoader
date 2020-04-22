import os
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
        cmake_args = [
            '-S', str(ext.cmake_root),
            '-B', str(build_temp),
            '-DCMAKE_BUILD_TYPE=' + config,
            f'-D{ext.target.upper()}_DESTINATION={ext_path.parent}',
            f'-D{ext.target.upper()}_NAME={ext_path.name}',
        ]

        build_args = [
            '--config', config,
            '--target', ext.target,
            '--', '-j4'
        ]

        self.spawn(['cmake', ] + cmake_args)
        if not self.dry_run:
            self.spawn(['cmake', '--build', str(build_temp)] + build_args)

ext_module = CMakeExtension(
    name='videoloader._ext',
    cmake_root='videoloader/_ext',
    target='videoloader')

setup(
    name='VideoLoader',
    version='0.0.1',
    description='Enable high performance video data loading for machine learning.',
    packages=find_packages(exclude=['tests']),
    ext_modules=[ext_module],
    cmdclass={
        'build_ext': build_ext,
    },
)