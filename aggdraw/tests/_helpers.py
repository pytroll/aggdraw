"""Shared utilities for inspecting what aggdraw actually drew."""

import numpy as np
from PIL import Image

import aggdraw


WHITE = (255, 255, 255)


def to_image(draw):
    """Convert a Draw surface to a PIL Image.

    Note that a BGRA surface reports its mode as "RGBA", so the channels of
    the returned image are in the wrong order for that one case.
    """
    return Image.frombytes(draw.mode, draw.size, draw.tobytes())


def ink_count(im):
    """Count the pixels in an image that aren't the white background.

    Only RGB images are supported. A single-band image has no band axis for
    the comparison to work against, and an RGBA one has a fourth band that
    WHITE says nothing about.
    """
    if im.mode != "RGB":
        raise NotImplementedError(f"ink_count only supports RGB images, got {im.mode!r}")
    return int(np.any(np.asarray(im) != WHITE, axis=-1).sum())


def render(path):
    """Draw a path in black on a white 100x100 surface.

    Drawing black on white means any pixel that isn't WHITE is part of the
    path. Antialiasing makes the edges gray rather than pure black, so tests
    check for "not the background" instead of for an exact ink color.
    """
    return draw_with("path", path, aggdraw.Pen("black", 1))


def draw_with(method, path, pen=None):
    """Draw a path on a fresh white surface with one of the Draw methods.

    ``symbol`` takes a position as well; (0, 0) draws the path where it was
    defined, matching what the other methods do. A pen of None is passed
    through as-is, so nothing is drawn -- the same as calling Draw directly.
    """
    draw = aggdraw.Draw("RGB", (100, 100), "white")
    if method == "symbol":
        draw.symbol((0, 0), path, pen)
    else:
        getattr(draw, method)(path, pen)
    return to_image(draw)
