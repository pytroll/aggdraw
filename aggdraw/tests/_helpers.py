"""Shared utilities for inspecting what aggdraw actually drew."""

import numpy as np
from PIL import Image


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
