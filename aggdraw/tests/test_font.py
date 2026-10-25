"""Tests for the Font class."""

import glob
import os

import numpy as np
import pytest

import aggdraw
from aggdraw.tests._helpers import WHITE, to_image


FONT_DIRS = [
    "/usr/share/fonts",  # linux
    "/usr/local/share/fonts",  # linux
    "/System/Library/Fonts",  # macos
    "/Library/Fonts",  # macos
    os.path.join(os.environ.get("WINDIR", r"C:\Windows"), "Fonts"),  # windows
]


# Fonts known to carry Latin glyphs, most preferred first. Taking the first .ttf in
# Other available fonts may not have expected glyph shapes/sizes so would fail tests.
PREFERRED_FONTS = (
    "DejaVuSans.ttf",  # most linux distros, conda envs
    "LiberationSans-Regular.ttf",  # fedora/rhel, ubuntu
    "FreeSans.ttf",  # gnu freefont
    "NotoSans-Regular.ttf",
    "Arial.ttf",  # macos /System/Library/Fonts/Supplemental, msttcorefonts
    "arial.ttf",  # windows
    "Verdana.ttf",
    "verdana.ttf",
)


def _find_font():
    """Return the path of a TrueType font with Latin glyphs, or None.

    aggdraw ships no font of its own and Pillow's default font is not a file
    on disk, so there is nothing portable to point Font() at.
    """
    found = {}
    for directory in FONT_DIRS:
        for path in glob.glob(os.path.join(directory, "**", "*.ttf"), recursive=True):
            found.setdefault(os.path.basename(path), path)
    for name in PREFERRED_FONTS:
        if name in found:
            return found[name]
    return None


def _font_or_skip(color="black", size=12):
    """Build a Font, skipping the test if fonts or FreeType are unavailable."""
    if not hasattr(aggdraw.Draw("RGB", (1, 1), "white")._draw, "text"):
        pytest.skip("built without FreeType, so there is no text renderer")
    path = _find_font()
    if path is None:
        pytest.skip("no suitable TrueType font found on this machine")
    return aggdraw.Font(color, path, size)


def test_font_metrics_are_str():
    """family and style are str, not bytes.

    They used to be built with PyBytes_FromString, a leftover from Python 2
    where that produced a str.
    """
    font = _font_or_skip("black")

    assert isinstance(font.family, str)
    assert isinstance(font.style, str)
    assert font.family
    assert isinstance(font.ascent, float)
    assert isinstance(font.descent, float)
    # the wrapper forwards to the same values on the C object
    assert font.family == font._font.family


def test_font_draws_text():
    """Text actually puts ink on the surface, in the requested color."""
    font = _font_or_skip("red", 24)

    surf = aggdraw.Draw("RGB", (200, 60), "white")
    width, height = surf.textsize("Hello", font)
    assert width > 0
    assert height > 0

    surf.text((5, 5), "Hello", font)
    arr = np.asarray(to_image(surf))
    ink = arr[np.any(arr != WHITE, axis=-1)]

    # were any red pixels drawn?
    assert np.any(np.all(ink == (255, 0, 0), axis=-1))
    # AGG blends coverage over the background, so red over white is always
    # (255, 255 - k, 255 - k): the red channel saturates and green tracks blue
    assert np.all(ink[:, 0] == 255)
    assert np.all(ink[:, 1] == ink[:, 2])
