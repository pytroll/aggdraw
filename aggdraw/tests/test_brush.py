"""Tests for the Brush class."""

import pytest

from aggdraw.tests._helpers import to_image


def _fill(color, opacity=255):
    """Fill a small white surface with one brush and return an interior pixel."""
    from aggdraw import Brush, Draw

    surf = Draw("RGB", (20, 20), "white")
    surf.rectangle((0, 0, 20, 20), brush=Brush(color, opacity))
    return to_image(surf).getpixel((10, 10))


def test_brush():
    from aggdraw import Brush, Draw

    Brush("black")
    Brush("black", opacity=128)

    Brush(0)
    Brush((0, 0, 0))
    Brush("rgb(0, 0, 0)")
    Brush("gold")

    # Check that brushes work as expected
    brushes = {
        "black": Brush("black"),
        "red": Brush((255, 0, 0)),
        "black_50": Brush((0, 0, 0), opacity=128),
        "crimson": Brush("#DC143C"),
    }
    surf = Draw("RGB", (100, 100), "white")
    surf.rectangle((0, 0, 50, 50), brush=brushes["black"])
    surf.rectangle((50, 0, 100, 50), brush=brushes["red"])
    surf.rectangle((0, 50, 50, 100), brush=brushes["black_50"])
    surf.rectangle((50, 50, 100, 100), brush=brushes["crimson"])
    im = to_image(surf)
    assert im.getpixel((1, 1)) == (0, 0, 0)
    assert im.getpixel((51, 1)) == (255, 0, 0)
    assert im.getpixel((1, 51)) == (127, 127, 127)
    assert im.getpixel((51, 51)) == (220, 20, 60)


@pytest.mark.parametrize(
    ("color", "expected"),
    [
        ((255, 0, 0), (255, 0, 0)),  # RGB tuple
        ("#FFFF00", (255, 255, 0)),  # 6-digit hex, handled in C
        ("#ff0", (255, 255, 0)),  # 3-digit hex, via PIL
        ("chartreuse", (127, 255, 0)),  # the full CSS name set, via PIL
        ("hsl(0, 100%, 50%)", (255, 0, 0)),  # CSS color functions, via PIL
        (128, (128, 128, 128)),  # an integer is a shade of gray...
        (300, (44, 44, 44)),  # ...and values outside 0-255 wrap
        ("notacolor", (0, 0, 0)),  # unrecognized colors go silently black
    ],
)
def test_brush_color_formats(color, expected):
    assert _fill(color) == expected


def test_brush_rgba_tuple_overrides_opacity():
    # A 3-tuple takes its alpha channel from the opacity argument...
    half_red = (255, 127, 127)  # 50% red composited onto the white background
    assert _fill((255, 0, 0), opacity=128) == half_red
    # ...while a 4-tuple sets it directly, ignoring opacity entirely
    assert _fill((255, 0, 0, 128)) == half_red
    assert _fill((255, 0, 0, 255), opacity=128) == (255, 0, 0)
