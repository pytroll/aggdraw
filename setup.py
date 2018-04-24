#!/usr/bin/env python
#
# Setup script for aggdraw
#
# Usage:
#
#   To build in current directory:
#   $ python setup.py build_ext -i
#
#   To build and install:
#   $ python setup.py install
#
from __future__ import print_function
import os
import sys
import subprocess

try:
    from setuptools import setup, Extension
except ImportError:
    from distutils.core import setup, Extension

VERSION = "1.3.2"

SUMMARY = "High quality drawing interface for PIL."

DESCRIPTION = """\

The aggdraw module implements the basic WCK 2D Drawing Interface on
top of the AGG library. This library provides high-quality drawing,
with anti-aliasing and alpha compositing, while being fully compatible
with the WCK renderer.

"""

try:
    # pointer to freetype build directory (tweak as necessary)
    FREETYPE_ROOT = subprocess.check_output(
        ['freetype-config', '--prefix']).strip().decode()
    print("=== freetype found: '{}'".format(FREETYPE_ROOT))
except (OSError, subprocess.CalledProcessError):
    print("=== freetype not available")
    FREETYPE_ROOT = None

sources = [
    # source code currently used by aggdraw
    # FIXME: link against AGG library instead?
    "agg2/src/agg_arc.cpp",
    "agg2/src/agg_bezier_arc.cpp",
    "agg2/src/agg_curves.cpp",
    "agg2/src/agg_path_storage.cpp",
    "agg2/src/agg_rasterizer_scanline_aa.cpp",
    "agg2/src/agg_trans_affine.cpp",
    "agg2/src/agg_vcgen_contour.cpp",
    # "agg2/src/agg_vcgen_dash.cpp",
    "agg2/src/agg_vcgen_stroke.cpp",
    ]

# define VERSION macro in C++ code, need to quote it
defines = [('VERSION', "\"{}\"".format(VERSION))]

include_dirs = ["agg2/include"]
library_dirs = []

libraries = []

if FREETYPE_ROOT:
    defines.append(("HAVE_FREETYPE2", None))
    sources.extend([
        "agg2/font_freetype/agg_font_freetype.cpp",
        ])
    include_dirs.append("agg2/font_freetype")
    include_dirs.append(os.path.join(FREETYPE_ROOT, "include"))
    include_dirs.append(os.path.join(FREETYPE_ROOT, "include/freetype2"))
    library_dirs.append(os.path.join(FREETYPE_ROOT, "lib"))
    libraries.append("freetype")

if sys.platform == "win32":
    libraries.extend(["kernel32", "user32", "gdi32"])

setup(
    name="aggdraw",
    version=VERSION,
    author="Fredrik Lundh",
    author_email="fredrik@pythonware.com",
    classifiers=[
        "Development Status :: 4 - Beta",
        # "Development Status :: 5 - Production/Stable",
        "Topic :: Multimedia :: Graphics",
        ],
    description=SUMMARY,
    download_url="http://www.effbot.org/downloads#aggdraw",
    license="Python (MIT style)",
    long_description=DESCRIPTION.strip(),
    platforms="Python 2.7 and later.",
    url="https://github.com/pytroll/aggdraw",
    ext_modules=[
        Extension("aggdraw", ["aggdraw.cxx"] + sources,
                  define_macros=defines,
                  include_dirs=include_dirs,
                  library_dirs=library_dirs, libraries=libraries
                  )
        ],
    python_requires='>=2.7,!=3.0.*,!=3.1.*,!=3.2.*,!=3.3.*',
    tests_require=['pillow'],
    )
