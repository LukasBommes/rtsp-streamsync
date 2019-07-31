from distutils.core import setup, Extension
import pkgconfig
import numpy as np

d = pkgconfig.parse('libavformat libswscale opencv4')

stream_sync = Extension('stream_sync',
                    include_dirs = ['/home/ffmpeg_sources/ffmpeg',
                                    *d['include_dirs'],
                                    np.get_include()],
                    library_dirs = d['library_dirs'],
                    libraries = d['libraries'],
                    sources = ['src/py_stream_sync.cpp',
                               'src/stream_sync.cpp',
                               '../video_cap/src/video_cap.cpp',
                               '../video_cap/src/time_cvt.cpp'],
                    extra_compile_args = ['-std=c++17'],
                    extra_link_args = ['-fPIC', '-Wl,-Bsymbolic'])

setup (name = 'stream_sync',
       version = '1.0',
       description = 'Synchronizes multiple RTSP streams based on UNIX timestamp of each frame.',
       ext_modules = [stream_sync])
