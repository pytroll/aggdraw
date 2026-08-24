"""Tests for the Draw class."""

import aggdraw
import pytest

from aggdraw.tests._helpers import WHITE, to_image


def test_draw():
    from aggdraw import Draw
    from PIL import Image

    with pytest.raises(AttributeError) as excinfo:
        Draw("RGB")
    assert "'str' object has no attribute 'mode'" in str(excinfo.value)

    draw = Draw("RGB", (800, 600))
    assert draw.mode == "RGB"
    assert draw.size == (800, 600)

    draw = Draw("RGB", (800, 600), "white")
    assert draw.mode == "RGB"
    assert draw.size == (800, 600)

    im = Image.new("RGB", (600, 800))
    draw = Draw(im)
    assert draw.mode == "RGB"
    assert draw.size == (600, 800)


def test_flush():
    from aggdraw import Draw
    from PIL import Image

    im = Image.new("RGB", (600, 800))
    draw = Draw(im)
    assert draw.flush().mode == "RGB"


def test_transform():
    from aggdraw import Draw

    draw = Draw("RGB", (500, 500))

    draw.settransform()
    draw.settransform((250, 250))
    draw.settransform((1, 0, 250, 0, 1, 250))
    draw.settransform((2.0, 0.5, 250, 0.5, 2.0, 250))
    draw.settransform()


def test_graphics():
    from aggdraw import Draw, Pen, Brush

    draw = Draw("RGB", (500, 500))

    pen = Pen("black")
    brush = Brush("black")

    draw.line((50, 50, 100, 100), pen)

    draw.rectangle((50, 150, 100, 200), pen)
    draw.rectangle((50, 220, 100, 270), brush)
    draw.rectangle((50, 290, 100, 340), brush, pen)
    draw.rectangle((50, 360, 100, 410), pen, brush)

    draw.ellipse((120, 150, 170, 200), pen)
    draw.ellipse((120, 220, 170, 270), brush)
    draw.ellipse((120, 290, 170, 340), brush, pen)
    draw.ellipse((120, 360, 170, 410), pen, brush)

    draw.polygon((190 + 25, 150, 190, 200, 190 + 50, 200), pen)
    draw.polygon((190 + 25, 220, 190, 270, 190 + 50, 270), brush)
    draw.polygon((190 + 25, 290, 190, 340, 190 + 50, 340), brush, pen)
    draw.polygon((190 + 25, 360, 190, 410, 190 + 50, 410), pen, brush)


def test_polygon_closes_coords_but_not_path():
    """polygon() closes coordinates, but draws a Path as it was defined.

    A sequence of coordinates has no way to say whether it is closed, so
    polygon() always closes it. A Path carries that state itself, so it is
    left alone. Only the pen outline is affected: filling with a brush closes
    the shape either way.
    """

    def polygon(xy, **kwargs):
        draw = aggdraw.Draw("RGB", (100, 100), "white")
        draw.polygon(xy, **kwargs)
        return to_image(draw)

    corner = [20, 20, 80, 20, 80, 80]
    open_path = aggdraw.Path(corner)
    closed_path = aggdraw.Path(corner)
    closed_path.close()

    # The closing edge is a diagonal from (80, 80) back to (20, 20), so it
    # passes through the middle of the surface
    pen = aggdraw.Pen("black", 1)
    assert polygon(corner, pen=pen).getpixel((50, 50)) != WHITE
    assert polygon(open_path, pen=pen).getpixel((50, 50)) == WHITE
    assert polygon(closed_path, pen=pen).getpixel((50, 50)) != WHITE

    # A brush fills the same shape however the points were given
    brush = aggdraw.Brush("black")
    filled_coords = polygon(corner, brush=brush)
    filled_path = polygon(open_path, brush=brush)
    assert filled_coords.tobytes() == filled_path.tobytes()
